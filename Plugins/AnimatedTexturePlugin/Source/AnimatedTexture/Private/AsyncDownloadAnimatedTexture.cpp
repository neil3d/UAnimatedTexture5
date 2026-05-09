/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Async blueprint node for downloading UAnimatedTexture2D via HTTP(S).
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#include "AsyncDownloadAnimatedTexture.h"
#include "AnimatedTextureDownloader.h"
#include "AnimatedTextureFunctionLibrary.h"
#include "AnimatedTexture2D.h"
#include "AnimatedTextureModule.h"

#include "Async/Async.h"

UAsyncDownloadAnimatedTexture::UAsyncDownloadAnimatedTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 参考引擎 UAsyncTaskDownloadImage 的做法：在对象创建时即 AddToRoot，
	// 避免 NewObject -> Activate 之间的窗口期被 GC。CDO 需要跳过。
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AddToRoot();
	}
}

UAsyncDownloadAnimatedTexture* UAsyncDownloadAnimatedTexture::DownloadAnimatedTexture(
	UObject* WorldContextObject, const FString& Url, float Timeout, int32 MaxBytes)
{
	UAsyncDownloadAnimatedTexture* Action = NewObject<UAsyncDownloadAnimatedTexture>();
	Action->WorldContextWeak = WorldContextObject;
	Action->UrlCached = Url;
	Action->TimeoutCached = Timeout;
	Action->MaxBytesCached = MaxBytes;
	return Action;
}

void UAsyncDownloadAnimatedTexture::Activate()
{
	// 生命周期说明：AddToRoot 已在构造函数中完成；此处只负责启动下载。
	Downloader = MakeShared<FAnimatedTextureDownloader, ESPMode::ThreadSafe>();

	TWeakObjectPtr<UAsyncDownloadAnimatedTexture> WeakThis(this);

	Downloader->OnComplete.BindLambda(
		[WeakThis](const TArray<uint8>& Body, const FString& ETag)
		{
			UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->HandleComplete(Body, ETag);
		});

	Downloader->OnError.BindLambda(
		[WeakThis](EAnimatedTextureLoadError Err)
		{
			UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->HandleError(Err);
		});

	Downloader->BeginDownload(UrlCached, TimeoutCached, MaxBytesCached);
}

void UAsyncDownloadAnimatedTexture::Cancel()
{
	// 线程模型：
	// - 任意线程可调用 Cancel；HandleComplete / HandleError 依赖 UE5 默认的
	//   EHttpRequestDelegateThreadPolicy::CompleteOnGameThread，固定在 GameThread 触发。
	// - 三方共享 bFinished（TAtomic<bool>），通过 Exchange(true) 一次性"领取"完成权；
	//   后到者观察到旧值为 true 后立即 return，不会重复广播或重复 RemoveFromRoot。
	// - OnCanceled 广播 + RemoveFromRoot 必须在 GameThread；非 GameThread 时分发过去。
	if (bFinished.Exchange(true))
	{
		return; // 已被其他路径领走
	}

	if (Downloader.IsValid())
	{
		Downloader->Cancel();
	}

	TWeakObjectPtr<UAsyncDownloadAnimatedTexture> WeakThis(this);
	auto Finalize = [WeakThis]()
	{
		UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->OnCanceled.Broadcast();
		Self->RemoveFromRoot();
	};
	if (IsInGameThread())
	{
		Finalize();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Finalize));
	}
}

void UAsyncDownloadAnimatedTexture::HandleComplete(const TArray<uint8>& Body, const FString& ETag)
{
	// HTTP 回调依赖 UE5 默认的 CompleteOnGameThread 策略，保证此处在 GameThread。
	check(IsInGameThread());

	// 与 Cancel / HandleError 通过 bFinished.Exchange(true) 抢占；后到者直接 return。
	if (bFinished.Exchange(true))
	{
		return;
	}

	// WorldContext 已失效：静默退出。
	if (WorldContextWeak.IsStale())
	{
		RemoveFromRoot();
		return;
	}

	UAnimatedTexture2D* Texture = UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromMemory(
		Body, EAnimatedTextureType::None, /*Outer=*/ nullptr, NAME_None);

	if (!Texture)
	{
		OnFailed.Broadcast(EAnimatedTextureLoadError::DecodeFailed);
	}
	else
	{
		OnSuccess.Broadcast(Texture, UrlCached, ETag);
	}
	RemoveFromRoot();
}

void UAsyncDownloadAnimatedTexture::HandleError(EAnimatedTextureLoadError Error)
{
	// HTTP 回调依赖 UE5 默认的 CompleteOnGameThread 策略，保证此处在 GameThread。
	check(IsInGameThread());

	if (bFinished.Exchange(true))
	{
		return;
	}

	if (WorldContextWeak.IsStale())
	{
		RemoveFromRoot();
		return;
	}

	OnFailed.Broadcast(Error);
	RemoveFromRoot();
}

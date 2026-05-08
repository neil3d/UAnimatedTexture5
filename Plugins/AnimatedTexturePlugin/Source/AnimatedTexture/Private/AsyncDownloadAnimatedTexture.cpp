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
	// 线程模型说明：
	// - HTTP 回调（HandleComplete/HandleError）依赖 UE5 默认的 EHttpRequestDelegateThreadPolicy::CompleteOnGameThread，
	//   因此都在 GameThread 触发；
	// - Cancel() 由蓝图触发时也在 GameThread，但允许从任意线程调用，故下方保留 GameThread 分发；
	// - bFinished 当前仅在 GameThread 读写，无需原子化；若未来把 HTTP 策略切到 CompleteOnHttpThread，
	//   需把 bFinished 改为 TAtomic 或加锁。
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (Downloader.IsValid())
	{
		Downloader->Cancel();
	}

	// 在 GameThread 上广播 OnCanceled（Cancel 可能被任意线程/帧调用，这里统一到 GameThread）。
	TWeakObjectPtr<UAsyncDownloadAnimatedTexture> WeakThis(this);
	if (IsInGameThread())
	{
		OnCanceled.Broadcast();
		RemoveFromRoot();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->OnCanceled.Broadcast();
			Self->RemoveFromRoot();
		});
	}
}

void UAsyncDownloadAnimatedTexture::HandleComplete(const TArray<uint8>& Body, const FString& ETag)
{
	// HTTP 回调依赖 UE5 默认的 CompleteOnGameThread 策略，保证此处在 GameThread。
	check(IsInGameThread());

	// 可能已经被 Cancel 或提前结束。
	if (bFinished)
	{
		return;
	}

	// WorldContext 已失效：静默退出。
	if (WorldContextWeak.IsStale())
	{
		bFinished = true;
		RemoveFromRoot();
		return;
	}

	UAnimatedTexture2D* Texture = UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromMemory(
		Body, EAnimatedTextureType::None, /*Outer=*/ nullptr, NAME_None);

	bFinished = true;
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

	if (bFinished)
	{
		return;
	}

	if (WorldContextWeak.IsStale())
	{
		bFinished = true;
		RemoveFromRoot();
		return;
	}

	bFinished = true;
	OnFailed.Broadcast(Error);
	RemoveFromRoot();
}

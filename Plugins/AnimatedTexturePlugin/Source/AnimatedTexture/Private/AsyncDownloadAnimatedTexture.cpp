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
	// 使节点对象在下载期间不被 GC。
	AddToRoot();

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

	// 在 GameThread 上执行 UObject 操作。HTTP 回调默认已在 GameThread，但稳妥起见再守一次。
	auto DoOnGameThread = [this, Body = Body, ETag]()
	{
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
	};

	if (IsInGameThread())
	{
		DoOnGameThread();
	}
	else
	{
		TWeakObjectPtr<UAsyncDownloadAnimatedTexture> WeakThis(this);
		TArray<uint8> BodyCopy = Body;
		FString ETagCopy = ETag;
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, BodyCopy = MoveTemp(BodyCopy), ETagCopy = MoveTemp(ETagCopy)]() mutable
		{
			UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->HandleComplete(BodyCopy, ETagCopy);
		});
	}
}

void UAsyncDownloadAnimatedTexture::HandleError(EAnimatedTextureLoadError Error)
{
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

	auto DoOnGameThread = [this, Error]()
	{
		if (bFinished)
		{
			return;
		}
		bFinished = true;
		OnFailed.Broadcast(Error);
		RemoveFromRoot();
	};

	if (IsInGameThread())
	{
		DoOnGameThread();
	}
	else
	{
		TWeakObjectPtr<UAsyncDownloadAnimatedTexture> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Error]()
		{
			UAsyncDownloadAnimatedTexture* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->HandleError(Error);
		});
	}
}

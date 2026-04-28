/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Lightweight HTTP(S) downloader implementation.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#include "AnimatedTextureDownloader.h"
#include "AnimatedTextureModule.h"

#include "HttpModule.h"
#include "Async/Async.h"

FAnimatedTextureDownloader::FAnimatedTextureDownloader()
{
}

FAnimatedTextureDownloader::~FAnimatedTextureDownloader()
{
	// 保底：若对象析构时请求仍然 in-flight，主动取消，避免回调抵达后回访已销毁委托。
	if (PendingRequest.IsValid() && !bFinished)
	{
		PendingRequest->CancelRequest();
	}
}

bool FAnimatedTextureDownloader::BeginDownload(const FString& InUrl, float InTimeoutSec, int32 InMaxBytes)
{
	Url = InUrl;
	MaxBytes = InMaxBytes;
	bCanceled = false;
	bFinished = false;

	// 只允许 http / https
	if (!Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
		&& !Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: invalid URL scheme: %s"), *Url);

		// 延迟到下一帧的 GameThread 再投递失败，保证所有回调语义一致（异步触发）。
		TWeakPtr<FAnimatedTextureDownloader, ESPMode::ThreadSafe> WeakSelf = AsShared();
		AsyncTask(ENamedThreads::GameThread, [WeakSelf]()
		{
			TSharedPtr<FAnimatedTextureDownloader, ESPMode::ThreadSafe> Self = WeakSelf.Pin();
			if (!Self.IsValid() || Self->bCanceled || Self->bFinished)
			{
				return;
			}
			Self->bFinished = true;
			Self->OnError.ExecuteIfBound(EAnimatedTextureLoadError::InvalidUrl);
		});
		return false;
	}

	const float ResolvedTimeout = (InTimeoutSec > 0.0f) ? InTimeoutSec : 30.0f;

	// 注意：UE 5.3 里 FHttpModule::CreateRequest 返回的是 TSharedRef。
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(Url);
	Request->SetTimeout(ResolvedTimeout);

	// 以弱引用持有 self，避免在请求生命周期中阻止对象释放。
	TWeakPtr<FAnimatedTextureDownloader, ESPMode::ThreadSafe> WeakSelf = AsShared();
	Request->OnProcessRequestComplete().BindLambda(
		[WeakSelf](TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Req,
			TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Resp,
			bool bSucceeded)
		{
			TSharedPtr<FAnimatedTextureDownloader, ESPMode::ThreadSafe> Self = WeakSelf.Pin();
			if (!Self.IsValid())
			{
				return;
			}
			Self->OnHttpRequestComplete(Req, Resp, bSucceeded);
		});

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: ProcessRequest failed for %s"), *Url);
		bFinished = true;
		OnError.ExecuteIfBound(EAnimatedTextureLoadError::HttpFailed);
		return false;
	}

	PendingRequest = Request;
	return true;
}

void FAnimatedTextureDownloader::Cancel()
{
	if (bCanceled || bFinished)
	{
		return;
	}
	bCanceled = true;

	if (PendingRequest.IsValid())
	{
		PendingRequest->CancelRequest();
	}

	// 取消语义：由上层节点（UAsyncDownloadAnimatedTexture）自身广播 OnCanceled；
	// 这里不触发 OnComplete / OnError。
}

void FAnimatedTextureDownloader::OnHttpRequestComplete(
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> /*HttpRequest*/,
	TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse,
	bool bSucceeded)
{
	// 请求已被取消：不再派发任何回调。
	if (bCanceled)
	{
		bFinished = true;
		PendingRequest.Reset();
		return;
	}

	bFinished = true;
	PendingRequest.Reset();

	if (!bSucceeded || !HttpResponse.IsValid())
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: HTTP request failed: %s"), *Url);
		OnError.ExecuteIfBound(EAnimatedTextureLoadError::HttpFailed);
		return;
	}

	const int32 StatusCode = HttpResponse->GetResponseCode();
	if (StatusCode < 200 || StatusCode >= 300)
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: bad status %d for %s"), StatusCode, *Url);
		OnError.ExecuteIfBound(EAnimatedTextureLoadError::HttpBadStatus);
		return;
	}

	const TArray<uint8>& Content = HttpResponse->GetContent();
	if (Content.Num() <= 0)
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: empty body for %s"), *Url);
		OnError.ExecuteIfBound(EAnimatedTextureLoadError::EmptyBody);
		return;
	}

	if (MaxBytes > 0 && Content.Num() > MaxBytes)
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("FAnimatedTextureDownloader: body size %d exceeds limit %d for %s"),
			Content.Num(), MaxBytes, *Url);
		OnError.ExecuteIfBound(EAnimatedTextureLoadError::SizeLimitExceeded);
		return;
	}

	const FString ETag = HttpResponse->GetHeader(TEXT("ETag"));
	OnComplete.ExecuteIfBound(Content, ETag);
}

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Lightweight HTTP(S) downloader for AnimatedTexture bytes (.gif / .webp).
 * Modeled after Engine's FWebImage (ImageDownload module) but does NOT depend on
 * SlateCore / ImageWrapper / ImageDownload modules.
 *
 * Private to this module; only used by UAsyncDownloadAnimatedTexture.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#pragma once

#include "CoreMinimal.h"
#include "AnimatedTextureLoadTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

/**
 * FAnimatedTextureDownloader
 *
 * - 持有一个 IHttpRequest；生命周期由使用方通过 TSharedPtr 管理。
 * - BeginDownload 发起 GET 请求；完成时调用 OnComplete（成功）或 OnError（失败）。
 * - 回调在 GameThread 上触发（HTTP 模块默认 Tick 驱动即在 GameThread）。
 * - Cancel() 会取消底层请求；一旦取消，Complete / Error 回调不会再被触发。
 *
 * 用法：
 *   auto D = MakeShared<FAnimatedTextureDownloader, ESPMode::ThreadSafe>();
 *   D->OnComplete.BindLambda([](bool bOk, const TArray<uint8>& Body, const FString& ETag){ ... });
 *   D->OnError.BindLambda([](EAnimatedTextureLoadError Err){ ... });
 *   D->BeginDownload(Url, 30.0f, 32 * 1024 * 1024);
 */
class FAnimatedTextureDownloader : public TSharedFromThis<FAnimatedTextureDownloader, ESPMode::ThreadSafe>
{
public:
	/** 下载成功回调：Body 非空，ETag 可为空串。 */
	DECLARE_DELEGATE_TwoParams(FOnComplete, const TArray<uint8>& /*Body*/, const FString& /*ETag*/);
	/** 下载失败回调：带错误码。 */
	DECLARE_DELEGATE_OneParam(FOnError, EAnimatedTextureLoadError);

	FOnComplete OnComplete;
	FOnError OnError;

	FAnimatedTextureDownloader();
	~FAnimatedTextureDownloader();

	/**
	 * 发起下载。
	 * @param InUrl         目标 URL；仅支持 http:// / https://。
	 * @param InTimeoutSec  HTTP 超时，秒。<=0 时使用默认 30。
	 * @param InMaxBytes    响应体字节上限（超过则视为失败）。<=0 时不限制。
	 * @return true 表示请求已提交；false 表示 URL 非法等原因未发起。
	 */
	bool BeginDownload(const FString& InUrl, float InTimeoutSec, int32 InMaxBytes);

	/** 取消正在进行的下载；之后不再触发 OnComplete / OnError。 */
	void Cancel();

	/** 是否已取消 */
	bool IsCanceled() const { return bCanceled; }

	/** 当前请求的 URL（可能是空串） */
	const FString& GetUrl() const { return Url; }

private:
	void OnHttpRequestComplete(
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest,
		TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse,
		bool bSucceeded);

private:
	FString Url;
	int32 MaxBytes = 0;

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingRequest;

	bool bCanceled = false;
	bool bFinished = false;
};

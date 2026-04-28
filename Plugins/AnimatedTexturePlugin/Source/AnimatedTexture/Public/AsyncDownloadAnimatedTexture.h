/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Async blueprint node for downloading UAnimatedTexture2D via HTTP(S).
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AnimatedTextureLoadTypes.h"
#include "AsyncDownloadAnimatedTexture.generated.h"

class UAnimatedTexture2D;
class FAnimatedTextureDownloader;

/** 成功回调：Texture 非空；Url / ETag 为附加信息。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAnimatedTextureDownloadSuccess,
	UAnimatedTexture2D*, Texture,
	FString, Url,
	FString, ETag);

/** 取消回调：无参。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAnimatedTextureDownloadCanceled);

/** 失败回调：带错误码。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAnimatedTextureDownloadFailed,
	EAnimatedTextureLoadError, Error);

/**
 * 蓝图异步节点：通过 HTTP(S) 下载 .gif / .webp 并生成 UAnimatedTexture2D。
 * 提供 OnSuccess / OnCanceled / OnFailed 三个执行引脚。
 * 调用 Cancel() 可主动取消下载；Cancel 后会触发一次 OnCanceled 事件。
 */
UCLASS()
class ANIMATEDTEXTURE_API UAsyncDownloadAnimatedTexture : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FAnimatedTextureDownloadSuccess OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAnimatedTextureDownloadCanceled OnCanceled;

	UPROPERTY(BlueprintAssignable)
	FAnimatedTextureDownloadFailed OnFailed;

	/**
	 * 静态工厂方法：创建并启动一个异步下载节点。
	 *
	 * @param WorldContextObject 蓝图上下文对象（一般自动填充）。
	 * @param Url                HTTP(S) URL。
	 * @param Timeout            超时（秒），默认 30。
	 * @param MaxBytes           最大下载字节数，默认 32MB；<=0 表示不限制。
	 */
	UFUNCTION(BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Download Animated Texture",
			Category = "AnimatedTexture|Runtime Load"))
	static UAsyncDownloadAnimatedTexture* DownloadAnimatedTexture(
		UObject* WorldContextObject,
		const FString& Url,
		float Timeout = 30.0f,
		int32 MaxBytes = 33554432 /* 32 * 1024 * 1024 */);

	/** 取消下载。会触发一次 OnCanceled；之后不再触发 OnSuccess/OnFailed。 */
	UFUNCTION(BlueprintCallable, Category = "AnimatedTexture|Runtime Load")
	void Cancel();

	//~ Begin UBlueprintAsyncActionBase Interface
	virtual void Activate() override;
	//~ End UBlueprintAsyncActionBase Interface

private:
	/** 发起 HTTP 完成回调（成功路径）。 */
	void HandleComplete(const TArray<uint8>& Body, const FString& ETag);
	/** 发起 HTTP 失败回调。 */
	void HandleError(EAnimatedTextureLoadError Error);

private:
	UPROPERTY()
	TWeakObjectPtr<UObject> WorldContextWeak;

	FString UrlCached;
	float TimeoutCached = 30.0f;
	int32 MaxBytesCached = 32 * 1024 * 1024;

	TSharedPtr<FAnimatedTextureDownloader, ESPMode::ThreadSafe> Downloader;

	bool bFinished = false;
};

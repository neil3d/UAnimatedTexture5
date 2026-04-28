/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Async blueprint node for loading UAnimatedTexture2D from local file.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AnimatedTextureLoadTypes.h"
#include "AsyncLoadAnimatedTextureFromFile.generated.h"

class UAnimatedTexture2D;

/**
 * 异步本地加载结果委托。
 * @param Texture 加载成功时的 UAnimatedTexture2D 对象；失败时为 nullptr。
 * @param Error   错误码。成功时为 None。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAnimatedTextureLoadFileResult,
	UAnimatedTexture2D*, Texture,
	EAnimatedTextureLoadError, Error);

/**
 * 蓝图异步节点：从本地文件异步加载 UAnimatedTexture2D。
 * 文件 IO 在后台线程执行；对象构造与 UpdateResource 回到 GameThread。
 *
 * 使用方式（蓝图）：
 *   - 节点名 "Load Animated Texture From File Async"
 *   - 输入 File Path；输出 OnSuccess / OnFailed 两个执行引脚 + Texture / Error 两个数据引脚。
 */
UCLASS()
class ANIMATEDTEXTURE_API UAsyncLoadAnimatedTextureFromFile : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FAnimatedTextureLoadFileResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FAnimatedTextureLoadFileResult OnFailed;

	/**
	 * 静态工厂方法：创建并启动一个异步加载节点。
	 * @param WorldContextObject 蓝图上下文（一般自动填充）。若失效节点会静默退出。
	 * @param FilePath 本地文件路径；相对路径相对于 FPaths::ProjectDir()。
	 */
	UFUNCTION(BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Load Animated Texture From File Async",
			Category = "AnimatedTexture|Runtime Load"))
	static UAsyncLoadAnimatedTextureFromFile* LoadAnimatedTextureFromFileAsync(
		UObject* WorldContextObject, const FString& FilePath);

	//~ Begin UBlueprintAsyncActionBase Interface
	virtual void Activate() override;
	//~ End UBlueprintAsyncActionBase Interface

private:
	UPROPERTY()
	TWeakObjectPtr<UObject> WorldContextWeak;

	FString FilePathCached;
};

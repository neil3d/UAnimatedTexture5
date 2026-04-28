/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Runtime Load error types for AnimatedTexture plugin
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "AnimatedTextureLoadTypes.generated.h"

/**
 * 运行时加载 / 下载 UAnimatedTexture2D 时可能出现的错误类别。
 * 用于同步 API 的 out 参数以及异步节点的 OnFailed 回调。
 */
UENUM(BlueprintType)
enum class EAnimatedTextureLoadError : uint8
{
	/** 无错误（成功占位值） */
	None				UMETA(DisplayName = "None"),
	/** HTTP 请求层失败（网络错误、DNS 失败、超时等） */
	HttpFailed			UMETA(DisplayName = "Http Failed"),
	/** HTTP 响应状态码非 2xx */
	HttpBadStatus		UMETA(DisplayName = "Http Bad Status"),
	/** HTTP 响应 Body 为空 */
	EmptyBody			UMETA(DisplayName = "Empty Body"),
	/** 解码 gif / webp 字节流失败 */
	DecodeFailed		UMETA(DisplayName = "Decode Failed"),
	/** 本地文件不存在或无法打开 */
	FileNotFound		UMETA(DisplayName = "File Not Found"),
	/** 字节流格式无法识别（magic bytes 嗅探失败） */
	InvalidFormat		UMETA(DisplayName = "Invalid Format"),
	/** 下载大小超过上限 */
	SizeLimitExceeded	UMETA(DisplayName = "Size Limit Exceeded"),
	/** URL 非法或 scheme 不支持 */
	InvalidUrl			UMETA(DisplayName = "Invalid Url")
};

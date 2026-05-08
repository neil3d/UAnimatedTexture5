/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Runtime Load function library for AnimatedTexture plugin.
 * Provides synchronous/asynchronous APIs to build UAnimatedTexture2D at runtime
 * from in-memory byte stream, local file, or (via async nodes) HTTP(S) URL.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimatedTexture2D.h"
#include "AnimatedTextureLoadTypes.h"
#include "AnimatedTextureFunctionLibrary.generated.h"

class UImage;

/**
 * UAnimatedTextureFunctionLibrary
 *
 * Runtime Load 入口集合。所有函数对蓝图和 C++ 均可见（除内部原子函数 InitAnimatedTextureFromMemory 之外）。
 *
 * 调用关系（唯一真源）：
 *   LoadAnimatedTextureFromFile   -> LoadAnimatedTextureFromMemory -> InitAnimatedTextureFromMemory -> UAnimatedTexture2D::ImportFile
 *   AsyncLoadAnimatedTextureFromFile -> LoadAnimatedTextureFromMemory
 *   AsyncDownloadAnimatedTexture     -> LoadAnimatedTextureFromMemory
 *   UAnimatedTextureFactory::FactoryCreateBinary -> InitAnimatedTextureFromMemory (Editor 走 PostEditChange 触发 UpdateResource)
 */
UCLASS()
class ANIMATEDTEXTURE_API UAnimatedTextureFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 原子初始化函数（纯 C++ 接口，不暴露蓝图）：
	 * 向一个**已存在**的 UAnimatedTexture2D 填充字节流与类型，但不会 NewObject 也不会调用 UpdateResource。
	 * 用于 Editor Factory 走 CreateOrOverwriteAsset 之后的填充，以及 Runtime 的包装层 LoadAnimatedTextureFromMemory 复用。
	 *
	 * @param Target      必须是一个已经构造好的 UAnimatedTexture2D 对象。
	 * @param Buffer      字节流指针，不能为 nullptr。
	 * @param BufferSize  字节流长度（字节）。必须 > 0。
	 * @param Type        若为 None，则会自动用 magic bytes 嗅探；嗅探失败返回 false。
	 * @return            是否成功写入（类型识别 + 字节填充）。注意：并不保证后续解码一定成功。
	 */
	static bool InitAnimatedTextureFromMemory(UAnimatedTexture2D* Target, const uint8* Buffer, int32 BufferSize, EAnimatedTextureType Type = EAnimatedTextureType::None);

	/**
	 * 从一段字节流同步创建 UAnimatedTexture2D。
	 * 内部会 NewObject 出一个新的瞬态对象、调用 InitAnimatedTextureFromMemory、最后主动调用 UpdateResource。
	 * 失败时返回 nullptr。必须在 GameThread 调用。
	 *
	 * @param Bytes   gif / webp 字节流内容。
	 * @param Type    可选类型指定；EAnimatedTextureType::None 表示自动嗅探。
	 * @param Outer   新建对象的 Outer；为空则使用 GetTransientPackage()。
	 * @param Name    新建对象名；NAME_None 表示由引擎自动取名。
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimatedTexture|Runtime Load", meta = (AdvancedDisplay = "Type,Outer,Name"))
	static UAnimatedTexture2D* LoadAnimatedTextureFromMemory(
		const TArray<uint8>& Bytes,
		EAnimatedTextureType Type = EAnimatedTextureType::None,
		UObject* Outer = nullptr,
		FName Name = NAME_None);

	/**
	 * 从本地文件路径同步加载。失败时返回 nullptr，并通过 OutError 返回错误码。
	 * 相对路径相对于 FPaths::ProjectDir()；绝对路径按原样使用。
	 * 内部会先用文件扩展名推断类型，再 fallback 到 magic bytes 嗅探。
	 * 必须在 GameThread 调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimatedTexture|Runtime Load")
	static UAnimatedTexture2D* LoadAnimatedTextureFromFile(
		const FString& FilePath,
		EAnimatedTextureLoadError& OutError);

	/**
	 * 把 UAnimatedTexture2D 设置到 UMG 的 UImage 控件上。
	 *
	 * 为什么需要这个包装：
	 *   引擎的 UImage::SetBrushFromTexture 形参类型是 UTexture2D*，而
	 *   UAnimatedTexture2D 派生自 UTexture（不是 UTexture2D），所以
	 *   Cast<UTexture2D>(animTex) 在运行时会返回 nullptr，结果是 Brush 被清空、
	 *   控件显示为空。本函数通过 SetBrushResourceObject 走通用路径，避免类型陷阱。
	 *   设计背景见 Docs/BaseClassChoice.md。
	 *
	 * @param Image       目标 UImage；为 nullptr 时函数无副作用并打 warning log。
	 * @param Texture     要绑定的动画纹理；为 nullptr 时把 Brush.ResourceObject 清空。
	 * @param bMatchSize  若为 true 且 Texture 有效，按解码后的纹理尺寸更新 Brush.ImageSize。
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimatedTexture|UMG",
		meta = (DisplayName = "Set Brush from Animated Texture"))
	static void SetBrushFromAnimatedTexture(
		UImage* Image,
		UAnimatedTexture2D* Texture,
		bool bMatchSize = false);
};

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * 版本兼容基础设施头文件
 * 集中定义引擎版本检测宏和 RHI 兼容性辅助函数/宏，
 * 为后续多引擎版本（UE 5.3+）兼容开发提供基础。
 *
 * 当前仅包含针对当前目标 UE 版本的单一实现路径，
 * 后续可在此文件中通过条件编译添加不同版本的实现分支。
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "RHI.h"
#include "RHICommandList.h"

// ============================================================================
// 引擎版本检测宏
// ============================================================================

/**
 * 引擎版本比较宏：判断当前引擎版本是否 >= 指定版本
 * 用法示例：
 *   #if AT_UE_VERSION_GE(5, 4)
 *       // UE 5.4+ 专用代码
 *   #else
 *       // UE 5.3 及更早版本代码
 *   #endif
 */
#define AT_UE_VERSION_GE(MajorVersion, MinorVersion) \
	((ENGINE_MAJOR_VERSION > (MajorVersion)) || \
	 (ENGINE_MAJOR_VERSION == (MajorVersion) && ENGINE_MINOR_VERSION >= (MinorVersion)))

/**
 * 引擎版本比较宏：判断当前引擎版本是否 < 指定版本
 */
#define AT_UE_VERSION_LT(MajorVersion, MinorVersion) \
	(!AT_UE_VERSION_GE(MajorVersion, MinorVersion))

/**
 * 引擎版本比较宏：判断当前引擎版本是否等于指定版本
 */
#define AT_UE_VERSION_EQ(MajorVersion, MinorVersion) \
	(ENGINE_MAJOR_VERSION == (MajorVersion) && ENGINE_MINOR_VERSION == (MinorVersion))

// ============================================================================
// RHI 兼容性辅助函数
// 将版本敏感的 RHI API 调用封装在此处，业务逻辑通过这些函数间接调用引擎 API。
// 后续多版本适配时，只需在此文件中添加条件编译分支，无需修改业务代码。
// ============================================================================

namespace AnimatedTextureCompat
{

/**
 * 创建 2D 纹理的兼容性封装
 * @param RHICmdList - RHI 命令列表
 * @param Name - 纹理名称
 * @param SizeX - 纹理宽度
 * @param SizeY - 纹理高度
 * @param Format - 像素格式
 * @param NumMips - Mip 层级数
 * @param NumSamples - 采样数
 * @param Flags - 纹理创建标志
 * @return 创建的纹理 RHI 引用
 */
inline FTextureRHIRef AT_CreateTexture2D(
	FRHICommandListBase& RHICmdList,
	const TCHAR* Name,
	uint32 SizeX,
	uint32 SizeY,
	EPixelFormat Format,
	uint32 NumMips,
	uint32 NumSamples,
	ETextureCreateFlags Flags)
{
	// UE 5.3+ : 使用 RHICreateTexture（全局函数，接受 FRHITextureCreateDesc）
	// 注意：后续版本可能需要切换到 RHICmdList.CreateTexture()
	return RHICreateTexture(
		FRHITextureCreateDesc::Create2D(Name, SizeX, SizeY, Format)
			.SetNumMips(NumMips)
			.SetNumSamples(NumSamples)
			.SetFlags(Flags)
	);
}

/**
 * 更新 2D 纹理数据的兼容性封装
 * @param RHICmdList - RHI 命令列表（Immediate）
 * @param Texture - 目标纹理
 * @param MipIndex - Mip 层级索引
 * @param Region - 更新区域
 * @param SrcPitch - 源数据行字节数
 * @param SrcData - 源数据指针
 */
inline void AT_UpdateTexture2D(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* Texture,
	uint32 MipIndex,
	const FUpdateTextureRegion2D& Region,
	uint32 SrcPitch,
	const uint8* SrcData)
{
	// UE 5.3+ : 使用 RHICmdList 成员函数替代已废弃的全局 RHIUpdateTexture2D()
	RHICmdList.UpdateTexture2D(Texture, MipIndex, Region, SrcPitch, SrcData);
}

/**
 * 更新纹理引用的兼容性封装
 * @param TextureRef - 纹理引用 RHI
 * @param NewTexture - 新的纹理 RHI（可为 nullptr）
 */
inline void AT_UpdateTextureReference(
	FRHITextureReference* TextureRef,
	FRHITexture* NewTexture)
{
	RHIUpdateTextureReference(TextureRef, NewTexture);
}

/**
 * 将帧像素数据上传到 GPU 纹理的高层兼容性封装
 *
 * 该函数封装了从 FRHITexture 获取纹理尺寸、构建更新区域、上传像素数据的完整流程，
 * 内部通过条件编译处理不同引擎版本的 RHI API 差异：
 *   - UE 5.7+：FRHITexture2D 子类型已移除，直接使用 FRHITexture* 操作
 *   - UE 5.3~5.6：通过 GetTexture2D() 获取 FTexture2DRHIRef 后操作
 *
 * @param RHICmdList - RHI 命令列表（Immediate）
 * @param TextureRHI - 目标纹理 RHI 指针（调用方需确保非空）
 * @param SrcData - 帧像素数据指针（BGRA 格式，大小应匹配纹理尺寸）
 * @return 操作是否成功（纹理无效时返回 false）
 */
inline bool AT_UpdateFrameToTexture(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* TextureRHI,
	const uint8* SrcData)
{
#if AT_UE_VERSION_GE(5, 7)
	// UE 5.7+ : FRHITexture2D 子类型已移除，直接使用统一的 FRHITexture*
	uint32 TexWidth = TextureRHI->GetSizeX();
	uint32 TexHeight = TextureRHI->GetSizeY();
#else
	// UE 5.3~5.6 : 通过 GetTexture2D() 获取特化类型并验证
	FTexture2DRHIRef Texture2DRHI = TextureRHI->GetTexture2D();
	if (!Texture2DRHI)
		return false;

	uint32 TexWidth = Texture2DRHI->GetSizeX();
	uint32 TexHeight = Texture2DRHI->GetSizeY();
#endif

	uint32 SrcPitch = TexWidth * sizeof(FColor);

	FUpdateTextureRegion2D Region;
	Region.SrcX = Region.SrcY = Region.DestX = Region.DestY = 0;
	Region.Width = TexWidth;
	Region.Height = TexHeight;

	AT_UpdateTexture2D(RHICmdList, TextureRHI, 0, Region, SrcPitch, SrcData);
	return true;
}

} // namespace AnimatedTextureCompat

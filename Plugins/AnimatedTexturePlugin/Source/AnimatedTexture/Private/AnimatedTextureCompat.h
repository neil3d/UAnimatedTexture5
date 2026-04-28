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

} // namespace AnimatedTextureCompat

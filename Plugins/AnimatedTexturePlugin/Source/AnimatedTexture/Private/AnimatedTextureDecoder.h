/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"

class FAnimatedTextureDecoder
{
public:
	FAnimatedTextureDecoder() = default;
	virtual ~FAnimatedTextureDecoder() {}

	virtual bool LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize) = 0;
	virtual void Close() = 0;

	/**
	 * @return frame delay in milliseconds
	 */
	virtual uint32 NextFrame(uint32 DefaultFrameDelay, bool bLooping) = 0;
	virtual void Reset() = 0;

	virtual uint32 GetWidth() const = 0;
	virtual uint32 GetHeight() const = 0;
	virtual const FColor* GetFrameBuffer() const = 0;

	virtual uint32 GetDuration(uint32 defaultFrameDelay) const = 0;
	virtual bool SupportsTransparency() const = 0;

	/**
	 * 比特流中声明的"应循环次数"（0 表示无限循环 / 未声明）。
	 * 用于支持文件原生 loop_count，配合 UAnimatedTexture2D::bRespectFileLoopCount 使用。
	 * 默认实现返回 0，表示派生类未提供该信息。
	 */
	virtual uint32 GetLoopCount() const { return 0; }

	/**
	 * 已完成的循环次数（NextFrame 内部跨过末尾时累加）。
	 * 用于和 GetLoopCount 比较，决定是否停止播放。
	 */
	virtual uint32 GetCompletedLoops() const { return 0; }

public:
	FAnimatedTextureDecoder(const FAnimatedTextureDecoder&) = delete;
	FAnimatedTextureDecoder& operator=(const FAnimatedTextureDecoder&) = delete;
};

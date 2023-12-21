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

public:
	FAnimatedTextureDecoder(const FAnimatedTextureDecoder&) = delete;
	FAnimatedTextureDecoder& operator=(const FAnimatedTextureDecoder&) = delete;
};

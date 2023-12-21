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
#include "AnimatedTextureDecoder.h"
#include "giflib/gif_lib.h"

class FGIFDecoder : public FAnimatedTextureDecoder
{
public:
	FGIFDecoder() = default;
	virtual ~FGIFDecoder();

	virtual bool LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize) override;
	virtual void Close() override;

	virtual uint32 NextFrame(uint32 DefaultFrameDelay, bool bLooping) override;
	virtual void Reset() override;

	virtual uint32 GetWidth() const override;
	virtual uint32 GetHeight() const override;
	virtual const FColor* GetFrameBuffer() const override;

	virtual uint32 GetDuration(uint32 DefaultFrameDelay) const override;
	virtual bool SupportsTransparency() const override;

private:
	void ClearFrameBuffer(ColorMapObject* ColorMap, bool bTransparent);
	void GCB_Background(int left, int top, int width, int height,
		ColorMapObject* colorMap, bool bTransparent);

private:
	int mCurrentFrame = 0;
	int mLoopCount = 0;
	bool mDoNotDispose = false;

	GifFileType* mGIF = nullptr;
	TArray<FColor> mFrameBuffer;
};

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "WebpDecoder.h"
#include "AnimatedTextureModule.h"

FWebpDecoder::~FWebpDecoder()
{
	Close();
}

bool FWebpDecoder::LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize)
{
	// get image width height
	int ret = WebPGetInfo(InBuffer, InBufferSize, &Width, &Height);
	if (ret != VP8_STATUS_OK)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Get Webp info failed, %d."), ret);
		return false;
	}

	// create WebPAnimDecoder
	WebPAnimDecoderOptions opt;
	memset(&opt, 0, sizeof(opt));
	opt.color_mode = MODE_BGRA;
	opt.use_threads = 0;

	WebPData WData = { InBuffer, InBufferSize };
	Decoder = WebPAnimDecoderNew(&WData, &opt);
	if (!Decoder)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error parsing image."));
		return false;
	}

	// get anim info
	WebPAnimInfo AnimInfo;
	if (!WebPAnimDecoderGetInfo(Decoder, &AnimInfo)) {
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error getting global info about the animation."));
		return false;
	}

	return true;
}

void FWebpDecoder::Close()
{
	if (Decoder)
	{
		WebPAnimDecoderDelete(Decoder);
		Decoder = nullptr;
	}
}

uint32 FWebpDecoder::PlayFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	return 0;
}

void FWebpDecoder::Reset()
{
}

const FColor* FWebpDecoder::GetFrameBuffer() const
{
	return nullptr;
}

uint32 FWebpDecoder::GetDuration(uint32 DefaultFrameDelay) const
{
	return 0;
}

bool FWebpDecoder::SupportsTransparency() const
{

	return false;
}



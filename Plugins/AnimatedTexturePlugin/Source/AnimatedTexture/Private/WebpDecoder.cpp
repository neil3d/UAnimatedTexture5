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
	int ret = WebPGetFeatures(InBuffer, InBufferSize, &Features);
	if (ret != VP8_STATUS_OK)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Get Webp info failed, %d."), ret);
		return false;
	}

	// create WebPAnimDecoder
	WebPAnimDecoderOptions opt;
	WebPAnimDecoderOptionsInit(&opt);
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
	if (!WebPAnimDecoderGetInfo(Decoder, &AnimInfo)) {
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error getting global info about the animation."));
		return false;
	}

	// TODO
	int Timestamp = 0;
	uint8* Buffer = nullptr;
	while (WebPAnimDecoderHasMoreFrames(Decoder)) {
		WebPAnimDecoderGetNext(Decoder, &Buffer, &Timestamp);
	}
	WebPAnimDecoderReset(Decoder);
	Duration = Timestamp;

	return true;
}

void FWebpDecoder::Close()
{
	if (Decoder)
	{
		WebPAnimDecoderDelete(Decoder);
		Decoder = nullptr;
		FrameBuffer = nullptr;
	}
}

uint32 FWebpDecoder::NextFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	if (Decoder == nullptr)
		return DefaultFrameDelay;

	// restart
	if (!WebPAnimDecoderHasMoreFrames(Decoder))
	{
		if (bLooping)
			WebPAnimDecoderReset(Decoder);
		else
			return DefaultFrameDelay;
	}

	// decode next frame
	int Timestamp = 0;
	if (!WebPAnimDecoderGetNext(Decoder, &FrameBuffer, &Timestamp))
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error decoding frame."));
	}

	// frame duration
	int FrameDuration = Timestamp - PrevFrameTimestamp;
	PrevFrameTimestamp = Timestamp;
	return FrameDuration;
}

void FWebpDecoder::Reset()
{
	WebPAnimDecoderReset(Decoder);
	FrameBuffer = nullptr; 
}

const FColor* FWebpDecoder::GetFrameBuffer() const
{
	return (FColor*)FrameBuffer;
}

uint32 FWebpDecoder::GetDuration(uint32 DefaultFrameDelay) const
{
	return Duration;
}

bool FWebpDecoder::SupportsTransparency() const
{
	return Features.has_alpha != 0;
}



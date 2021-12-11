/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/


#include "AnimatedTexture2D.h"
#include "AnimatedTextureResource.h"
#include "GIFDecoder.h"
#include "WebpDecoder.h"

float UAnimatedTexture2D::GetSurfaceWidth() const
{
	if (Decoder) return Decoder->GetWidth();
	return 1.0f;
}

float UAnimatedTexture2D::GetSurfaceHeight() const
{
	if (Decoder) return Decoder->GetHeight();
	return 1.0f;
}

FTextureResource* UAnimatedTexture2D::CreateResource()
{
	if (FileType == EAnimatedTextureType::None
		|| FileBlob.Num() <= 0)
		return nullptr;

	// create decoder
	switch (FileType)
	{
	case EAnimatedTextureType::Gif:
		Decoder = MakeShared<FGIFDecoder, ESPMode::ThreadSafe>();
		break;
	case EAnimatedTextureType::Webp:
		Decoder = MakeShared<FWebpDecoder, ESPMode::ThreadSafe>();
		break;
	}

	check(Decoder);
	if (Decoder->LoadFromMemory(FileBlob.GetData(), FileBlob.Num()))
	{
		AnimationLength = Decoder->GetDuration(DefaultFrameDelay * 1000) / 1000.0f;
		SupportsTransparency = Decoder->SupportsTransparency();
	}
	else
	{
		Decoder.Reset();
		return nullptr;
	}

	// create RHI resource object
	FTextureResource* NewResource = new FAnimatedTextureResource(this);
	return NewResource;
}

void UAnimatedTexture2D::Tick(float DeltaTime)
{
	if (!bPlaying)
		return;
	if (!Decoder)
		return;

	FrameTime += DeltaTime * PlayRate;
	if (FrameTime < FrameDelay)
		return;

	FrameTime = 0;
	FrameDelay = RenderFrameToTexture();
}


#if WITH_EDITOR
void UAnimatedTexture2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool RequiresNotifyMaterials = false;
	bool ResetAnimState = false;

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		static const FName SupportsTransparencyName = GET_MEMBER_NAME_CHECKED(UAnimatedTexture2D, SupportsTransparency);

		if (PropertyName == SupportsTransparencyName)
		{
			RequiresNotifyMaterials = true;
			ResetAnimState = true;
		}
	}// end of if(prop is valid)

	if (ResetAnimState)
	{
		FrameDelay = RenderFrameToTexture();
		FrameTime = 0;
	}

	if (RequiresNotifyMaterials)
		NotifyMaterials();
}
#endif // WITH_EDITOR

void UAnimatedTexture2D::ImportFile(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize)
{
	FileType = InFileType;
	FileBlob = TArray<uint8>(InBuffer, InBufferSize);
}

float UAnimatedTexture2D::RenderFrameToTexture()
{
	// decode a new frame to memory buffer
	int nFrameDelay = Decoder->NextFrame(DefaultFrameDelay * 1000, bLooping);

	// copy frame to RHI texture
	struct FRenderCommandData
	{
		FTextureResource* RHIResource;
		const uint8* FrameBuffer;
	};

	typedef TSharedPtr<FRenderCommandData, ESPMode::ThreadSafe> FCommandDataPtr;
	FCommandDataPtr CommandData = MakeShared<FRenderCommandData, ESPMode::ThreadSafe>();
	CommandData->RHIResource = Resource;
	CommandData->FrameBuffer = (const uint8*)(Decoder->GetFrameBuffer());

	//-- equeue render command
	ENQUEUE_RENDER_COMMAND(AnimTexture2D_RenderFrame)(
		[CommandData](FRHICommandListImmediate& RHICmdList)
		{
			if (!CommandData->RHIResource || !CommandData->RHIResource->TextureRHI)
				return;

			FTexture2DRHIRef Texture2DRHI = CommandData->RHIResource->TextureRHI->GetTexture2D();
			if (!Texture2DRHI)
				return;

			uint32 TexWidth = Texture2DRHI->GetSizeX();
			uint32 TexHeight = Texture2DRHI->GetSizeY();
			uint32 SrcPitch = TexWidth * sizeof(FColor);

			FUpdateTextureRegion2D Region;
			Region.SrcX = Region.SrcY = Region.DestX = Region.DestY = 0;
			Region.Width = TexWidth;
			Region.Height = TexHeight;

			RHIUpdateTexture2D(Texture2DRHI, 0, Region, SrcPitch, CommandData->FrameBuffer);
		});

	return nFrameDelay / 1000.0f;
}

float UAnimatedTexture2D::GetAnimationLength() const
{
	return AnimationLength;
}

void UAnimatedTexture2D::Play()
{
	bPlaying = true;
}

void UAnimatedTexture2D::PlayFromStart()
{
	FrameTime = 0;
	FrameDelay = 0;
	bPlaying = true;
	if (Decoder) Decoder->Reset();
}

void UAnimatedTexture2D::Stop()
{
	bPlaying = false;
}

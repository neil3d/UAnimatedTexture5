/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/


// ReSharper disable All
#include "AnimatedTexture2D.h"
#include "AnimatedTextureResource.h"
#include "AnimatedTextureCompat.h"
#include "GIFDecoder.h"
#include "WebpDecoder.h"
#include "RenderingThread.h"

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

	const FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
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
	// 解码新的一帧到内存缓冲区
	int nFrameDelay = Decoder->NextFrame(DefaultFrameDelay * 1000, bLooping);

	// 获取帧缓冲数据
	const FColor* SrcFrameBuffer = Decoder->GetFrameBuffer();
	FTextureResource* TextureResource = GetResource();
	if (!SrcFrameBuffer || !TextureResource)
		return nFrameDelay / 1000.0f;

	// 拷贝帧缓冲数据的副本，确保渲染线程读取时游戏线程不会修改该数据
	// （GIF 解码器的 FrameBuffer 在下一帧解码时会被覆盖，
	//  WebP 解码器的 FrameBuffer 由 libwebp 内部管理，同样可能被覆盖）
	const uint32 FrameWidth = Decoder->GetWidth();
	const uint32 FrameHeight = Decoder->GetHeight();
	const uint32 BufferSize = FrameWidth * FrameHeight * sizeof(FColor);

	struct FRenderCommandData
	{
		FTextureResource* RHIResource;
		TArray<uint8> FrameBufferCopy;
	};

	typedef TSharedPtr<FRenderCommandData, ESPMode::ThreadSafe> FCommandDataPtr;
	FCommandDataPtr CommandData = MakeShared<FRenderCommandData, ESPMode::ThreadSafe>();
	CommandData->RHIResource = TextureResource;
	CommandData->FrameBufferCopy.SetNumUninitialized(BufferSize);
	FMemory::Memcpy(CommandData->FrameBufferCopy.GetData(), SrcFrameBuffer, BufferSize);

	//-- 提交渲染命令
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

			AnimatedTextureCompat::AT_UpdateTexture2D(RHICmdList, Texture2DRHI, 0, Region, SrcPitch, CommandData->FrameBufferCopy.GetData());
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

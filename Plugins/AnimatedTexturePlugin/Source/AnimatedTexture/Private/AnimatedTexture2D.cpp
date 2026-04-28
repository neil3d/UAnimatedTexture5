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
#include "Misc/Paths.h"

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

EAnimatedTextureType UAnimatedTexture2D::DetectTypeFromExtension(const FString& FilenameOrExt)
{
	// 取最右侧一段作为扩展名（兼容 "foo.gif" / ".gif" / "gif" / "a/b/c.webp" 等）
	FString Ext = FPaths::GetExtension(FilenameOrExt, /*bIncludeDot=*/ false);
	if (Ext.IsEmpty())
	{
		// FPaths::GetExtension 对于不含 '.' 的输入返回空串；此时把整串视为扩展名
		Ext = FilenameOrExt;
	}
	Ext.TrimStartAndEndInline();
	// 剥掉可能的前导 '.'
	while (Ext.StartsWith(TEXT(".")))
	{
		Ext.RightChopInline(1, /*bAllowShrinking=*/ false);
	}

	if (Ext.Compare(TEXT("gif"), ESearchCase::IgnoreCase) == 0)
	{
		return EAnimatedTextureType::Gif;
	}
	if (Ext.Compare(TEXT("webp"), ESearchCase::IgnoreCase) == 0)
	{
		return EAnimatedTextureType::Webp;
	}
	return EAnimatedTextureType::None;
}

EAnimatedTextureType UAnimatedTexture2D::DetectTypeFromMagic(const uint8* Buffer, int32 Size)
{
	if (!Buffer || Size <= 0)
	{
		return EAnimatedTextureType::None;
	}

	// GIF: "GIF87a" / "GIF89a" (6 bytes)
	if (Size >= 6
		&& Buffer[0] == 'G' && Buffer[1] == 'I' && Buffer[2] == 'F'
		&& Buffer[3] == '8'
		&& (Buffer[4] == '7' || Buffer[4] == '9')
		&& Buffer[5] == 'a')
	{
		return EAnimatedTextureType::Gif;
	}

	// WebP: "RIFF" ???? "WEBP"  (12 bytes 起步)
	if (Size >= 12
		&& Buffer[0] == 'R' && Buffer[1] == 'I' && Buffer[2] == 'F' && Buffer[3] == 'F'
		&& Buffer[8] == 'W' && Buffer[9] == 'E' && Buffer[10] == 'B' && Buffer[11] == 'P')
	{
		return EAnimatedTextureType::Webp;
	}

	return EAnimatedTextureType::None;
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

			AnimatedTextureCompat::AT_UpdateFrameToTexture(RHICmdList, CommandData->RHIResource->TextureRHI, CommandData->FrameBufferCopy.GetData());
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

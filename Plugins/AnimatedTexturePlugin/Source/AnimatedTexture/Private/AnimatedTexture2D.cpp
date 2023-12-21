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
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Components/Image.h"
#include "Async/Async.h"


bool UAnimatedTexture2D::NonzeroSize()
{
	return GifSize.X != 0 && GifSize.Y != 0;
}

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
	if (!Decoder)
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
	}

	bPlaying = true;
	
	// create RHI resource object
	FTextureResource* NewResource = new FAnimatedTextureResource(this);
	GifSize = FVector2D(NewResource->GetSizeX(), NewResource->GetSizeY());
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
	CommandData->RHIResource = GetResource();
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


UAnimatedTexture2D* UAnimatedTexture2D::LoadAnimatedTexture2D(FString FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		FilePath = FPaths::Combine(FPaths::ProjectDir(), FilePath);
		if (!FPaths::FileExists(FilePath))
		{
			return NULL;
		}
	}


	FString Extension = FPaths::GetExtension(FilePath, true);
	if (Extension.Compare(TEXT(".gif"), ESearchCase::IgnoreCase) == 0 || Extension.Compare(TEXT(".webp"), ESearchCase::IgnoreCase) == 0)
	{
		TArray<uint8> RawFileData;
		if (FFileHelper::LoadFileToArray(RawFileData, *FilePath))
		{
			EAnimatedTextureType AnimTextureType = EAnimatedTextureType::None;
			if (Extension.Compare(TEXT(".gif"), ESearchCase::IgnoreCase) == 0)
				AnimTextureType = EAnimatedTextureType::Gif;
			else if (Extension.Compare(TEXT(".webp"), ESearchCase::IgnoreCase) == 0)
				AnimTextureType = EAnimatedTextureType::Webp;

			TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe> GifDecoder;
			if (UAnimatedTexture2D::CanDecoderGif(AnimTextureType, RawFileData.GetData(), RawFileData.Num(), GifDecoder))
			{
				UAnimatedTexture2D* LoadedAT2D = NewObject<UAnimatedTexture2D>();
				LoadedAT2D->CreateTransient(AnimTextureType, RawFileData.GetData(), RawFileData.Num(), GifDecoder);
				LoadedAT2D->UpdateResource();
				if (LoadedAT2D)
				{
					return LoadedAT2D;
				}
			}
		}
	}


	return NULL;
}


void UAnimatedTexture2D::AsyncLoadLocalGif(const FString& URL, FAsyncGifTextureFormLocalDelegate LocalDelegate /*= FAsyncGifTextureFormLocalDelegate()*/)
{
	AsyncTask(ENamedThreads::AnyHiPriThreadNormalTask, [URL, LocalDelegate]()
		{
			UAnimatedTexture2D* AnimatedTexture2D = UAnimatedTexture2D::LoadAnimatedTexture2D(URL);
			if (LocalDelegate.IsBound())
			{
				AsyncTask(ENamedThreads::GameThread, [AnimatedTexture2D, LocalDelegate]()
					{
						LocalDelegate.ExecuteIfBound(AnimatedTexture2D);
					});
				return;
			}
		});
}

UAnimatedTexture2D* UAnimatedTexture2D::DownLoadAnimatedTexture2D(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize)
{

	TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe> GifDecoder;
	if (UAnimatedTexture2D::CanDecoderGif(InFileType, InBuffer, InBufferSize, GifDecoder))
	{
		UAnimatedTexture2D* LoadedAT2D = NewObject<UAnimatedTexture2D>();
		LoadedAT2D->CreateTransient(InFileType, InBuffer, InBufferSize, GifDecoder);
		LoadedAT2D->UpdateResource();
		if (LoadedAT2D)
		{
			return LoadedAT2D;
		}
	}
	return NULL;
}

bool UAnimatedTexture2D::CanDecoderGif(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize, TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe>& GifDecoder)
{

	TArray<uint8> GifFileBlob = TArray<uint8>(InBuffer, InBufferSize);

	if (InFileType != EAnimatedTextureType::None || GifFileBlob.Num() > 0)
	{
		// create decoder
		switch (InFileType)
		{
		case EAnimatedTextureType::Gif:
			GifDecoder = MakeShared<FGIFDecoder, ESPMode::ThreadSafe>();
			break;
		case EAnimatedTextureType::Webp:
			GifDecoder = MakeShared<FWebpDecoder, ESPMode::ThreadSafe>();
			break;
		}

		if (GifDecoder)
		{
			return GifDecoder->LoadFromMemory(GifFileBlob.GetData(), GifFileBlob.Num());
		}
	}

	return false;
}


void UAnimatedTexture2D::CreateTransient(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize, TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe> GifDecoder)
{
	bPlaying = false;
	FileType = InFileType;
	FileBlob = TArray<uint8>(InBuffer, InBufferSize);
	if (GifDecoder)
	{
		Decoder = GifDecoder;
		AnimationLength = Decoder->GetDuration(DefaultFrameDelay * 1000) / 1000.0f;
		SupportsTransparency = Decoder->SupportsTransparency();
	}
}


void UAnimatedTexture2D::SetBrushFromAnimatedTexture2D(UImage* Target, UAnimatedTexture2D* AnimatedTexture, bool bMatchSize)
{
	if (AnimatedTexture && Target)
	{
		Target->SetBrushResourceObject(AnimatedTexture);
		if (bMatchSize)
		{
			if (AnimatedTexture->NonzeroSize())
			{
				Target->Brush.ImageSize = AnimatedTexture->GifSize;
			}
		}
	}
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

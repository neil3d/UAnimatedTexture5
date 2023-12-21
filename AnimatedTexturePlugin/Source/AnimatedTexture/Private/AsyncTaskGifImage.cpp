// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncTaskGifImage.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "AnimatedTexture2D.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

UAsyncTaskGifImage::UAsyncTaskGifImage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AddToRoot();
	}
}

UAsyncTaskGifImage* UAsyncTaskGifImage::LoadGIF(FString URL)
{
	UAsyncTaskGifImage* GifTask = NewObject<UAsyncTaskGifImage>();

	GifTask->Start(URL);
	return GifTask;
}

void UAsyncTaskGifImage::Start(FString URL)
{
	if (URL.StartsWith(TEXT("http")))
	{
		AnimatedTextureType = EAnimatedTextureType::None;
		FString Extension = FPaths::GetExtension(URL, true);
		if (Extension.Compare(TEXT(".gif"), ESearchCase::IgnoreCase) == 0)
		{
			AnimatedTextureType = EAnimatedTextureType::Gif;
		}
		else if (Extension.Compare(TEXT(".webp"), ESearchCase::IgnoreCase) == 0)
		{
			AnimatedTextureType = EAnimatedTextureType::Webp;
		}
		if (AnimatedTextureType != EAnimatedTextureType::None)
		{
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

			HttpRequest->OnProcessRequestComplete().BindUObject(this, &UAsyncTaskGifImage::HandleImageRequest);
			HttpRequest->SetURL(URL);
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->ProcessRequest();
		}
	}
	else
	{
		UAnimatedTexture2D::AsyncLoadLocalGif(URL, FAsyncGifTextureFormLocalDelegate::CreateUObject(this, &UAsyncTaskGifImage::HandleAsyncLoad));
	}
}

void UAsyncTaskGifImage::HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid() && HttpResponse->GetContentLength() > 0)
	{
		UAnimatedTexture2D* AT2D = UAnimatedTexture2D::DownLoadAnimatedTexture2D(AnimatedTextureType,HttpResponse->GetContent().GetData(), HttpResponse->GetContentLength());
		if (AT2D)
		{
			OnSuccess.Broadcast(AT2D);
			return;
		}
	}
	OnFail.Broadcast(nullptr);
}

void UAsyncTaskGifImage::HandleAsyncLoad(UAnimatedTexture2D* AnimatedTexture2D)
{
	if (AnimatedTexture2D)
	{
		OnSuccess.Broadcast(AnimatedTexture2D);
		return;
	}
	
	OnFail.Broadcast(nullptr);
}

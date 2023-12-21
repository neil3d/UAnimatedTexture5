// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/IHttpRequest.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "AsyncTaskGifImage.generated.h"

class UAnimatedTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoadGifImageDelegate, UAnimatedTexture2D*, Texture);

UCLASS()
class ANIMATEDTEXTURE_API UAsyncTaskGifImage : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=( BlueprintInternalUseOnly="true" ))
	static UAsyncTaskGifImage* LoadGIF(FString URL);

public:

	UPROPERTY(BlueprintAssignable)
	FLoadGifImageDelegate OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FLoadGifImageDelegate OnFail;

	EAnimatedTextureType AnimatedTextureType;

public:

	void Start(FString URL);

private:

	void HandleImageRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void HandleAsyncLoad(UAnimatedTexture2D* AnimatedTexture2D);
};

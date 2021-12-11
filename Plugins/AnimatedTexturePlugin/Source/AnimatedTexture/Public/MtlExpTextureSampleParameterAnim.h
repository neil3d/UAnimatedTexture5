// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MtlExpTextureSampleParameterAnim.generated.h"

UCLASS(collapsecategories, hidecategories = Object)
class ANIMATEDTEXTURE_API UMtlExpTextureSampleParameterAnim : public UMaterialExpressionTextureSampleParameter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
		virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

	//~ Begin UMaterialExpressionTextureSampleParameter Interface
	virtual bool TextureIsValid(UTexture* InTexture, FString& OutMessage);
	virtual void SetDefaultTexture();
	//~ End UMaterialExpressionTextureSampleParameter Interface
	
};

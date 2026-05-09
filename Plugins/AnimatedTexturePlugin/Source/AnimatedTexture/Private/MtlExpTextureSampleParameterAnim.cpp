/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/


#include "MtlExpTextureSampleParameterAnim.h"
#include "AnimatedTexture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTargetCube.h"

#define LOCTEXT_NAMESPACE "MaterialExpression"


UMtlExpTextureSampleParameterAnim::UMtlExpTextureSampleParameterAnim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> DefaultTexture;
		FText NAME_Texture;
		FText NAME_Parameters;
		FConstructorStatics()
			: DefaultTexture(TEXT("/Engine/EngineResources/DefaultTexture"))
			, NAME_Texture(LOCTEXT("Texture", "Texture"))
			, NAME_Parameters(LOCTEXT("Parameters", "Parameters"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.DefaultTexture.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
}

#if WITH_EDITOR
void UMtlExpTextureSampleParameterAnim::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ParamAnimTexture"));
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

#if WITH_EDITOR
bool UMtlExpTextureSampleParameterAnim::TextureIsValid(UTexture* InTexture, FString& OutMessage)
{
	if (!InTexture)
	{
		OutMessage = TEXT("NULL Texture");
		return false;
	}

	// 白名单 = UAnimatedTexture2D（本插件类型）+ UE 5.3 Parameter2D 引擎默认接受的全部 2D 类型。
	// 父类 UMaterialExpressionTextureSampleParameter::TextureIsValid 直接返回 false（abstract-style），
	// 无法通过 Super 委托复用 Parameter2D 的逻辑，这里只能手写镜像。
	if (InTexture->IsA<UAnimatedTexture2D>()
		|| InTexture->IsA<UTexture2D>()
		|| InTexture->IsA<UTextureRenderTarget2D>()
		|| InTexture->IsA<UTexture2DDynamic>()
		|| InTexture->GetMaterialType() == MCT_TextureExternal)
	{
		return true;
	}

	OutMessage = TEXT("Invalid texture type");
	return false;
}

void UMtlExpTextureSampleParameterAnim::SetDefaultTexture()
{
	Texture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), NULL, LOAD_None, NULL);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MtlExpTextureSampleParameterAnim.generated.h"

/**
 * 材质参数表达式：让材质参数能接受 UAnimatedTexture2D。
 * 故意派生自 UMaterialExpressionTextureSampleParameter（而非 ...Parameter2D），
 * 以绕开父类 TextureIsValid 中的 IsA(UTexture2D) 硬检查。
 * 详见 Docs/BaseClassChoice.md §4.2。
 */
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
#if WITH_EDITOR
	virtual bool TextureIsValid(UTexture* InTexture, FString& OutMessage) override;
	virtual void SetDefaultTexture() override;
#endif // WITH_EDITOR
	//~ End UMaterialExpressionTextureSampleParameter Interface
	
};

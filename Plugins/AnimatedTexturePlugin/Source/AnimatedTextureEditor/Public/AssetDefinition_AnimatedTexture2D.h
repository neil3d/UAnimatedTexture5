/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Asset Definition for UAnimatedTexture2D.
 * 在 Content Browser 里给 UAnimatedTexture2D 提供独立的颜色条与 "AnimatedTexture" 分类，
 * 取代默认的 *Other* 分组与 UTexture 默认外观。
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_AnimatedTexture2D.generated.h"

UCLASS()
class UAssetDefinition_AnimatedTexture2D : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// Begin UAssetDefinition Interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	// End UAssetDefinition Interface
};

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Asset Definition for UAnimatedTexture2D.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AssetDefinition_AnimatedTexture2D.h"
#include "AnimatedTexture2D.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureEditorModule.h"
#include "Toolkits/IToolkitHost.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_AnimatedTexture2D"

// 图标资源由 FAnimatedTextureStyle 注册到 ClassIcon.AnimatedTexture2D /
// ClassThumbnail.AnimatedTexture2D 槽位（见 AnimatedTextureStyle.cpp），无需在此处再设。

FText UAssetDefinition_AnimatedTexture2D::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeName", "Animated Texture");
}

FLinearColor UAssetDefinition_AnimatedTexture2D::GetAssetColor() const
{
	// 与引擎 Texture2D 的橙黄色 FColor(192, 64, 0) 区分开，使用青绿色。
	return FLinearColor(FColor(0, 192, 128));
}

TSoftClassPtr<UObject> UAssetDefinition_AnimatedTexture2D::GetAssetClass() const
{
	return UAnimatedTexture2D::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_AnimatedTexture2D::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(LOCTEXT("AnimatedTextureCategory", "AnimatedTexture"))
	};
	return Categories;
}

EAssetCommandResult UAssetDefinition_AnimatedTexture2D::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	// 默认实现 UAssetDefinitionDefault::OpenAssets 走通用属性编辑器（Details only），
	// 我们想要纹理预览面板，所以直接复用引擎自带的 FTextureEditorToolkit ——
	// ITextureEditorModule::CreateTextureEditor 接受任意 UTexture*，对继承自 UTexture
	// 的 UAnimatedTexture2D 同样成立。
	ITextureEditorModule* TextureEditorModule = FModuleManager::LoadModulePtr<ITextureEditorModule>(TEXT("TextureEditor"));
	if (!TextureEditorModule)
	{
		return EAssetCommandResult::Unhandled;
	}

	for (UAnimatedTexture2D* Texture : OpenArgs.LoadObjects<UAnimatedTexture2D>())
	{
		if (Texture)
		{
			TextureEditorModule->CreateTextureEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Texture);
		}
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE

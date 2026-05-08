/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Slate style set implementation.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AnimatedTextureStyle.h"

#include "Interfaces/IPluginManager.h"	// Projects
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"

#define IMAGE_PLUGIN_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

TSharedPtr<FSlateStyleSet> FAnimatedTextureStyle::StyleInstance = nullptr;

FName FAnimatedTextureStyle::GetStyleSetName()
{
	static const FName StyleSetName(TEXT("AnimatedTextureStyle"));
	return StyleSetName;
}

void FAnimatedTextureStyle::Initialize()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = MakeShared<FSlateStyleSet>(GetStyleSetName());

	// 把 ContentRoot 指向 <Plugin>/Resources，用相对路径加载 IconNN.png。
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AnimatedTexturePlugin"));
	if (Plugin.IsValid())
	{
		StyleInstance->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon64(64.0f, 64.0f);

	// ClassIcon.<ClassName> / ClassThumbnail.<ClassName> 是引擎按命名约定查询的槽位 ——
	// 与 SClassIconFinder / FSlateIconFinder 配合使用。给两个尺寸都注册可以让 Class Picker
	// 列表里和 Content Browser 默认缩略图都显示自定义图。
	FSlateStyleSet* StyleSet = StyleInstance.Get();
	StyleSet->Set(TEXT("ClassIcon.AnimatedTexture2D"), new IMAGE_PLUGIN_BRUSH(TEXT("Icon16"), Icon16));
	StyleSet->Set(TEXT("ClassThumbnail.AnimatedTexture2D"), new IMAGE_PLUGIN_BRUSH(TEXT("Icon64"), Icon64));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance.Get());
}

void FAnimatedTextureStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance.Get());
		StyleInstance.Reset();
	}
}

#undef IMAGE_PLUGIN_BRUSH

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Slate style set for AnimatedTexture editor visuals.
 * 注册 ClassIcon.AnimatedTexture2D / ClassThumbnail.AnimatedTexture2D 两个槽位，
 * 让 Content Browser 与 Class Picker 显示插件自带图标，而不是默认 Texture2D 图标。
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FAnimatedTextureStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};

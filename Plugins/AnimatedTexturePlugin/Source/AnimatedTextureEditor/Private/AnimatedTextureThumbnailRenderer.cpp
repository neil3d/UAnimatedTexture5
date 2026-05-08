/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/


#include "AnimatedTextureThumbnailRenderer.h"
#include "AnimatedTexture2D.h"

#include "CanvasTypes.h"	// Engine
#include "CanvasItem.h"	// Engine
#include "Engine/Texture2D.h"	// Engine
#include "ThumbnailRendering/ThumbnailManager.h"	// UnrealEd

void UAnimatedTextureThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UAnimatedTexture2D* Texture = Cast<UAnimatedTexture2D>(Object);

	if (Texture != nullptr)
	{
		OutWidth = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceWidth());
		OutHeight = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceHeight());
	}
	else
	{
		OutWidth = OutHeight = 0;
	}
}

void UAnimatedTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UAnimatedTexture2D* Texture = Cast<UAnimatedTexture2D>(Object);
	if (Texture != nullptr && Texture->GetResource() != nullptr)
	{
		if (Texture->SupportsTransparency)
		{
			// If support transparentcy, draw a checkerboard background first.
			// 起点必须用 (X, Y)，否则当调用方传入非零偏移（AssetView 浮层、复合缩略图等）时
			// 棋盘会与下方按 (X, Y) 绘制的纹理错位。对照引擎 UTexture2DThumbnailRenderer::Draw。
			const int32 CheckerDensity = 8;
			UTexture2D* Checker = UThumbnailManager::Get().CheckerboardTexture;
			Canvas->DrawTile(
				(float)X, (float)Y, (float)Width, (float)Height,		// Dimensions
				0.0f, 0.0f, CheckerDensity, CheckerDensity,				// UVs
				FLinearColor::White, Checker->GetResource());			// Tint & Texture
		}

		// Use A canvas tile item to draw
		FCanvasTileItem CanvasTile(FVector2D(X, Y), Texture->GetResource(), FVector2D(Width, Height), FLinearColor::White);
		CanvasTile.BlendMode = Texture->SupportsTransparency ? SE_BLEND_Translucent : SE_BLEND_Opaque;
		CanvasTile.Draw(Canvas);
	}// end of if(texture is valid)
	else if (Texture != nullptr)
	{
		// 资源尚未创建（导入中 / 解码失败）的占位绘制：纯棋盘背景 + 半透明灰雾。
		// 与正常路径在视觉上区分明显，便于诊断"对象存在但 RHI 没起来"的状态。
		const int32 CheckerDensity = 8;
		UTexture2D* Checker = UThumbnailManager::Get().CheckerboardTexture;
		if (Checker && Checker->GetResource())
		{
			Canvas->DrawTile(
				(float)X, (float)Y, (float)Width, (float)Height,
				0.0f, 0.0f, CheckerDensity, CheckerDensity,
				FLinearColor::White, Checker->GetResource());
		}

		FCanvasTileItem Overlay(FVector2D(X, Y), FVector2D(Width, Height), FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));
		Overlay.BlendMode = SE_BLEND_Translucent;
		Overlay.Draw(Canvas);
	}
}
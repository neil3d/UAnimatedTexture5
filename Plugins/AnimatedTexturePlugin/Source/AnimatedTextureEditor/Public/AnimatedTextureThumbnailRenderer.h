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
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "AnimatedTextureThumbnailRenderer.generated.h"

/**
 * UAnimatedTexture2D 的 Content Browser 缩略图渲染器：
 * 透明纹理时先绘棋盘背景，再叠当前帧的 RHI 资源。
 * 不驱动播放 —— 显示哪一帧由 UAnimatedTexture2D::Tick（编辑器内也会跑）决定。
 */
UCLASS()
class ANIMATEDTEXTUREEDITOR_API UAnimatedTextureThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_BODY()
	
public:
	// Begin UThumbnailRenderer Object
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	// End UThumbnailRenderer Object
};

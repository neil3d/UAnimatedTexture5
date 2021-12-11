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
#include "Engine/Texture.h"
#include "AnimatedTexture2D.generated.h"

UENUM()
enum class EAnimatedTextureType : uint8
{
	None,
	Gif,
	Webp
};


/**
 * Animated Texutre
 * @see class UTexture2D
 */
UCLASS()
class ANIMATEDTEXTURE_API UAnimatedTexture2D : public UTexture
{
	GENERATED_BODY()

public:
	//~ Begin UTexture Interface.
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual float GetSurfaceDepth() const override { return 0; }
	virtual uint32 GetSurfaceArraySize() const override { return 0; }

	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }
	//~ End UTexture Interface.

public:
	void ImportFile(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize);

protected:
	UPROPERTY()
		EAnimatedTextureType FileType;

	UPROPERTY()
		TArray<uint8> FileBlob;
};

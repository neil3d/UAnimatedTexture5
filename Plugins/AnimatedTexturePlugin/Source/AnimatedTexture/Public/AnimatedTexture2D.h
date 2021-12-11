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
#include "Tickable.h"	// Engine
#include "AnimatedTexture2D.generated.h"

class FAnimatedTextureDecoder;

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
UCLASS(BlueprintType, Category = AnimatedTexture)
class ANIMATEDTEXTURE_API UAnimatedTexture2D : public UTexture, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		bool SupportsTransparency = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		float DefaultFrameDelay = 1.0f / 10;	// used while Frame.Delay==0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		bool bLooping = true;

public:	// Playback APIs
	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void Play();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void PlayFromStart();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void Stop();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetLooping(bool bNewLooping) { bLooping = bNewLooping; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		bool IsLooping() const { return bLooping; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetPlayRate(float NewRate) { PlayRate = NewRate; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		float GetPlayRate() const { return PlayRate; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		float GetAnimationLength() const;

public:	// UTexture Interface
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual float GetSurfaceDepth() const override { return 0; }
	virtual uint32 GetSurfaceArraySize() const override { return 0; }

	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }

public:	// FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override
	{
		return true;
	}
	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimatedTexture2D, STATGROUP_Tickables);
	}
	virtual bool IsTickableInEditor() const
	{
		return true;
	}

	virtual UWorld* GetTickableGameObjectWorld() const
	{
		return GetWorld();
	}
public:	// UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

public: // Internal APIs
	void ImportFile(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize);

	float RenderFrameToTexture();

private:
	UPROPERTY()
		EAnimatedTextureType FileType = EAnimatedTextureType::None;

	UPROPERTY()
		TArray<uint8> FileBlob;

private:
	TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe> Decoder;

	float AnimationLength = 0.0f;
	float FrameDelay = 0.0f;
	float FrameTime = 0.0f;
	bool bPlaying = true;
};

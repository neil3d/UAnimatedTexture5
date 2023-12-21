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
class UImage;

UENUM()
enum class EAnimatedTextureType : uint8
{
	None,
	Gif,
	Webp
};

DECLARE_DELEGATE_OneParam(FAsyncGifTextureFormLocalDelegate, UAnimatedTexture2D*);

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

	UFUNCTION(BlueprintPure, Category = AnimatedTexture)
		bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetLooping(bool bNewLooping) { bLooping = bNewLooping; }

	UFUNCTION(BlueprintPure, Category = AnimatedTexture)
		bool IsLooping() const { return bLooping; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetPlayRate(float NewRate) { PlayRate = NewRate; }

	UFUNCTION(BlueprintPure, Category = AnimatedTexture)
		float GetPlayRate() const { return PlayRate; }

	UFUNCTION(BlueprintPure, Category = AnimatedTexture)
		float GetAnimationLength() const;

	UFUNCTION(BlueprintPure, Category = AnimatedTexture)
	FVector2D GetGifSize() { return GifSize; };

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
	bool NonzeroSize();

public:	// UTexture Interface
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual float GetSurfaceDepth() const override { return 0; }
	virtual uint32 GetSurfaceArraySize() const override { return 0; }

	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }
	virtual ETextureClass GetTextureClass() const override { return ETextureClass::TwoD; }

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

public:
	//************************************
	// Method:    LoadAnimatedTexture2D runtime模式下读取gif
	// FullName:  UAnimatedTexture2D::LoadAnimatedTexture2D
	// Access:    public static 
	// Returns:   UAnimatedTexture2D*
	// Qualifier:
	// Parameter: FString FilePath gif路径
	//************************************
	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
	static UAnimatedTexture2D* LoadAnimatedTexture2D(FString FilePath);

	static void AsyncLoadLocalGif(const FString& URL, FAsyncGifTextureFormLocalDelegate LocalDelegate = FAsyncGifTextureFormLocalDelegate());

	static UAnimatedTexture2D* DownLoadAnimatedTexture2D(EAnimatedTextureType FileType, const uint8* Buffer, uint32 BufferSize);


	//************************************
	// Method:    CanDecoderGif 是否可以解密gif文件
	// FullName:  UAnimatedTexture2D::CanDecoderGif
	// Access:    public static 
	// Returns:   bool
	// Qualifier:
	// Parameter: EAnimatedTextureType InFileType
	// Parameter: const uint8 * InBuffer
	// Parameter: uint32 InBufferSize
	//************************************
	static bool CanDecoderGif(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize, TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe>& Decoder);

	void CreateTransient(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize, TSharedPtr<FAnimatedTextureDecoder, ESPMode::ThreadSafe> Decoder);

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
	static void SetBrushFromAnimatedTexture2D(UImage* Target, UAnimatedTexture2D* AnimatedTexture, bool bMatchSize);

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
	FVector2D GifSize;
};

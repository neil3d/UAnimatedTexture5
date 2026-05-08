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
 * Animated Texture
 *
 * 设计说明（基类选择）：
 *   本类刻意派生自 UTexture，**既不**派生自 UTexture2D，**也不**派生自 UTexture2DDynamic。
 *   - 不选 UTexture2D：避免 FTextureSource / PlatformData / Mip Streaming /
 *     默认 Texture Editor 这一坨"导入纹理"专用机制；它们对 GIF/WebP 帧缓冲毫无意义，
 *     还要写一堆 override 才能不崩。
 *   - 不选 UTexture2DDynamic：UPROPERTY 字段会与父类 SamplerXAddress/SizeX/Format
 *     等冗余，且已有 .uasset 资产需要带 PostLoad 回填的迁移路径，跨 5.3-5.7 回归成本
 *     远大于"省掉一个 30 行 UMG 包装函数"的收益。
 *
 *   本类与引擎里 UTexture2DDynamic / UMediaTexture / UTextureRenderTarget2D 一样，
 *   都是"非可流式、运行时动态更新的 2D 纹理"路径。详细背景与对照见
 *   `Docs/BaseClassChoice.md`。
 *
 * UMG 使用提示：
 *   UImage::SetBrushFromTexture 形参是 UTexture2D*，会拒绝本类型。请改用
 *   UAnimatedTextureFunctionLibrary::SetBrushFromAnimatedTexture（C++ 与蓝图均可）。
 *
 * @see class UTexture2DDynamic
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

	// 若开启，并且文件比特流声明了有限的 loop_count（>0），
	// 在播放完该次数后将自动停止；声明为 0（无限）的文件不受影响。
	// 仅 WebP 暴露 loop_count；GIF 当前不解析 NETSCAPE2.0 应用扩展，开启后等价于无限。
	// 默认关闭以保持与历史版本一致的行为。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, AdvancedDisplay)
		bool bRespectFileLoopCount = false;

	// 启用 libwebp 多线程解码（WebPAnimDecoderOptions::use_threads）。
	// 对较大的 WebP 动画通常能减少单帧解码耗时；GIF 解码器忽略此开关。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, AdvancedDisplay)
		bool bUseMultithreadedDecode = false;

	// 启用预乘 alpha (MODE_bgrA)。开启后必须搭配使用 (One, OneMinusSrcAlpha)
	// 的混合方式才能得到正确合成；保持关闭则与原有材质工作流兼容。仅 WebP 生效。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, AdvancedDisplay)
		bool bPremultipliedAlpha = false;

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
	virtual ETextureClass GetTextureClass() const override { return ETextureClass::TwoDDynamic; }

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

	float RenderFrameToTexture(bool bEffectiveLooping = true);

	/**
	 * 根据文件扩展名（或包含扩展名的完整文件名）推断动画纹理类型。
	 * 接受形如 ".gif" / "gif" / "foo.webp" 等输入，内部做规范化处理，大小写不敏感。
	 * 无法识别时返回 EAnimatedTextureType::None。
	 */
	static EAnimatedTextureType DetectTypeFromExtension(const FString& FilenameOrExt);

	/**
	 * 根据文件内容 magic bytes 嗅探动画纹理类型：
	 *  - "GIF87a" / "GIF89a" -> Gif
	 *  - "RIFF....WEBP"      -> Webp
	 * 无法识别时返回 EAnimatedTextureType::None。
	 */
	static EAnimatedTextureType DetectTypeFromMagic(const uint8* Buffer, int32 Size);

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

	// 调试用：通过 CVar `at.DumpFrames` 启用时，对解码出的每帧顺序编号写盘到
	// <Saved>/AnimatedTextureDump/<Name>_frame_NNNN.png，便于和 Chrome 等参考实现对比。
	int32 FrameDumpCounter = 0;
};

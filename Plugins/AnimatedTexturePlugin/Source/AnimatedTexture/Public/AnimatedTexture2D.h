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
	UAnimatedTexture2D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		bool SupportsTransparency = true;

	// 开启（默认，与历史行为一致）：CreateResource 后由解码器结果覆盖上面的 SupportsTransparency。
	// 关闭：尊重用户在 Inspector / 蓝图里设置的 SupportsTransparency 值，不被运行期检测覆盖；
	//   适用于"明知文件没透明像素但仍想走 alpha 通道"或反之的边缘场景。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, AdvancedDisplay)
		bool bAutoDetectTransparency = true;

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

	/**
	 * 与 UTexture2D::HasAlphaChannel 同名同签名（**非虚函数**）。
	 *
	 * 设计说明：UTexture 基类不暴露 virtual HasAlphaChannel，UTexture2D 才有这个方法且非虚。
	 * 因此本方法**无法用 override 覆盖父类 vtable**——engine 内部以 UTexture* 持有的代码
	 * 走不到这里。提供它的目的是：
	 *   1) 让显式持有 UAnimatedTexture2D*（包括蓝图/插件下游代码）的调用方能直接拿到真实答案；
	 *   2) 与引擎其它纹理类的 API 表面保持对称，便于阅读 / 复制粘贴；
	 *   3) 避免默认 fall-through 到 UTexture 上没有此方法导致 Cast<UTexture2D>(at)->HasAlphaChannel()
	 *      返回的 false 被误用作"无 alpha"判定。
	 *
	 * 返回值跟随 SupportsTransparency；CreateResource 阶段若 bAutoDetectTransparency 为真，
	 * SupportsTransparency 会被解码器探测结果覆盖，所以与解码器/RHI 实际像素 alpha 对齐。
	 */
	bool HasAlphaChannel() const { return SupportsTransparency; }

public:	// FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override
	{
		// 收紧 Tick 调度门槛：CDO/Archetype、未 CreateResource 或已 Stop 的实例都不进
		// FTickableObjectsManager 的轮询。Tick 函数体内仍保留 bPlaying / Decoder 防御，
		// 应对 Stop 与 Tick 同帧调度的窗口期。
		return !IsTemplate() && Decoder.IsValid() && bPlaying;
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
	virtual void PostLoad() override;
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
	// 仅 GameThread 访问（CreateResource / Tick / RenderFrameToTexture），
	// 不需要 ThreadSafe 引用计数；强引用降级为默认 ESPMode 省一次原子开销。
	TSharedPtr<FAnimatedTextureDecoder> Decoder;

	float AnimationLength = 0.0f;
	float FrameDelay = 0.0f;
	float FrameTime = 0.0f;
	bool bPlaying = true;

	// 调试用：通过 CVar `at.DumpFrames` 启用时，对解码出的每帧顺序编号写盘到
	// <Saved>/AnimatedTextureDump/<Name>_frame_NNNN.png，便于和 Chrome 等参考实现对比。
	int32 FrameDumpCounter = 0;
};

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from WebP file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#pragma once

#include "CoreMinimal.h"
#include "AnimatedTextureDecoder.h"
#include "libwebp/src/webp/decode.h"
#include "libwebp/src/webp/demux.h"

/**
* @see https://developers.google.com/speed/webp/docs/api
*/
class FWebpDecoder : public FAnimatedTextureDecoder
{
public:
	FWebpDecoder() = default;
	virtual ~FWebpDecoder() override;

	virtual bool LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize) override;
	virtual void Close() override;

	virtual uint32 NextFrame(uint32 DefaultFrameDelay, bool bLooping) override;
	virtual void Reset() override;

	virtual uint32 GetWidth() const override { return AnimInfo.canvas_width; }
	virtual uint32 GetHeight() const override { return AnimInfo.canvas_height; }
	virtual const FColor* GetFrameBuffer() const override;

	virtual uint32 GetDuration(uint32 DefaultFrameDelay) const override;
	virtual bool SupportsTransparency() const override;

	virtual uint32 GetLoopCount() const override { return AnimInfo.loop_count; }
	virtual uint32 GetCompletedLoops() const override { return CompletedLoops; }

public:
	// 解码配置：必须在 LoadFromMemory 之前设置才会生效。
	// 对应 libwebp 的 WebPAnimDecoderOptions::use_threads。
	// 默认关闭以保持与历史版本一致的行为。
	bool bUseThreads = false;

	// 对应 libwebp 的 color_mode：true=MODE_bgrA(预乘 alpha)，false=MODE_BGRA(直 alpha)。
	// 切到预乘后，材质混合需要使用 (One, OneMinusSrcAlpha) 的混合方式才能得到正确合成结果。
	bool bPremultipliedAlpha = false;

private:
	// 单帧最小延时钳制（毫秒）。WebP 比特流允许 duration=0，
	// 直接返回会造成上层 Tick 忙等；钳到 10ms 既不会显著影响实际动画播放，
	// 又能避免 CPU 飙升。
	static constexpr uint32 kMinFrameDelayMs = 10;

private:
	int PrevFrameTimestamp = 0;
	uint32 Duration = 0;
	uint32 CompletedLoops = 0;

	// 在 NextFrame 检测"循环边界"用：
	// libwebp 不直接告诉你"刚刚跨了一圈"，需要通过 HasMoreFrames 自检。
	// 但跨圈的 timestamp 复位逻辑必须在 GetNext 之前完成（先 Reset 再 GetNext），
	// 故这里只用 CompletedLoops 计数即可，不再额外标志位。

	WebPAnimInfo AnimInfo{};
	WebPBitstreamFeatures Features{};

	uint8* FrameBuffer = nullptr;
	WebPAnimDecoder* Decoder = nullptr;
};

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from WebP file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "WebpDecoder.h"
#include "AnimatedTextureModule.h"

namespace
{
	// 进程内仅记录一次 libwebp 版本号，便于现场排查 lib 版本不匹配问题。
	void LogWebpVersionsOnce()
	{
		static bool bLogged = false;
		if (bLogged) return;
		bLogged = true;

		const int DecVer = WebPGetDecoderVersion();
		const int DemuxVer = WebPGetDemuxVersion();
		UE_LOG(LogAnimTexture, Log,
			TEXT("FWebpDecoder: libwebp decoder=%d.%d.%d, demux=%d.%d.%d"),
			(DecVer >> 16) & 0xff, (DecVer >> 8) & 0xff, DecVer & 0xff,
			(DemuxVer >> 16) & 0xff, (DemuxVer >> 8) & 0xff, DemuxVer & 0xff);
	}
}

FWebpDecoder::~FWebpDecoder()
{
	Close();
}

bool FWebpDecoder::LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize)
{
	LogWebpVersionsOnce();

	// 支持重入：清掉上一次（成功或半成功）的状态，避免成员指针被覆写造成泄漏。
	Close();

	if (!InBuffer || InBufferSize == 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: empty input buffer."));
		return false;
	}

	// 提前读取 has_alpha 等比特流特征（虽然 WebPAnimDecoderNew 内部也会校验，
	// 但我们仍需要 has_alpha 给 SupportsTransparency() 用）。
	const VP8StatusCode FeatureRet = WebPGetFeatures(InBuffer, InBufferSize, &Features);
	if (FeatureRet != VP8_STATUS_OK)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Get Webp info failed, %d."), FeatureRet);
		Close();
		return false;
	}

	// 创建 WebPAnimDecoder
	WebPAnimDecoderOptions Opt;
	if (!WebPAnimDecoderOptionsInit(&Opt))
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: WebPAnimDecoderOptionsInit failed (ABI mismatch?)."));
		Close();
		return false;
	}
	// MODE_BGRA / MODE_bgrA 与 Unreal FColor (BGRA) 字节序匹配，可直接强转。
	Opt.color_mode = bPremultipliedAlpha ? MODE_bgrA : MODE_BGRA;
	Opt.use_threads = bUseThreads ? 1 : 0;

	const WebPData WData = { InBuffer, InBufferSize };
	Decoder = WebPAnimDecoderNew(&WData, &Opt);
	if (!Decoder)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error parsing image."));
		Close();
		return false;
	}

	// 读取动画全局信息
	if (!WebPAnimDecoderGetInfo(Decoder, &AnimInfo))
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error getting global info about the animation."));
		Close();
		return false;
	}

	// 通过 demuxer 直接拿到所有帧的 duration 求和，避免在初始化阶段
	// 解码所有帧造成的性能开销。
	// 这里复用 AnimDecoder 内部的 demuxer（不要 Delete），少做一次解析。
	{
		const WebPDemuxer* Demuxer = WebPAnimDecoderGetDemuxer(Decoder);
		if (Demuxer)
		{
			WebPIterator Iter;
			if (WebPDemuxGetFrame(Demuxer, 1, &Iter))
			{
				uint32 TotalDuration = 0;
				do {
					// 钳制单帧最小延时，避免 duration=0 的帧导致总时长偏小或为 0。
					const uint32 FrameMs = Iter.duration > 0
						? static_cast<uint32>(Iter.duration)
						: kMinFrameDelayMs;
					TotalDuration += FMath::Max(FrameMs, kMinFrameDelayMs);
				} while (WebPDemuxNextFrame(&Iter));
				Duration = TotalDuration;
				WebPDemuxReleaseIterator(&Iter);
			}
		}
	}

	return true;
}

void FWebpDecoder::Close()
{
	if (Decoder)
	{
		WebPAnimDecoderDelete(Decoder);
		Decoder = nullptr;
	}
	FrameBuffer = nullptr;

	// 重置所有标量状态以支持重入：再次 LoadFromMemory 时不会带入旧值。
	PrevFrameTimestamp = 0;
	Duration = 0;
	CompletedLoops = 0;
	AnimInfo = WebPAnimInfo{};
	Features = WebPBitstreamFeatures{};
}

uint32 FWebpDecoder::NextFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	if (Decoder == nullptr)
		return DefaultFrameDelay;

	// 走到末尾时若需要循环，先 Reset 让 libwebp 内部 prev_frame_timestamp 归零，
	// 同时把成员 PrevFrameTimestamp 也归零——否则下一帧返回的时间戳（小）减去
	// 上一轮终点时间戳（大）会得到一个超大无符号数，造成"卡帧很久才追上一轮"。
	if (!WebPAnimDecoderHasMoreFrames(Decoder))
	{
		if (bLooping)
		{
			WebPAnimDecoderReset(Decoder);
			PrevFrameTimestamp = 0;
			++CompletedLoops;
		}
		else
		{
			// 不循环：保持当前帧不动，按 DefaultFrameDelay 节奏继续被 Tick 调度。
			return DefaultFrameDelay;
		}
	}

	// 解码下一帧
	int Timestamp = 0;
	uint8* NewBuffer = nullptr;
	if (!WebPAnimDecoderGetNext(Decoder, &NewBuffer, &Timestamp))
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FWebpDecoder: Error decoding frame."));
		// 失败路径：保持 PrevFrameTimestamp 不变；FrameBuffer 置 null
		// 让上层 GetFrameBuffer() 拿到 null 并跳过本帧渲染（避免读到无效数据）。
		FrameBuffer = nullptr;
		return DefaultFrameDelay;
	}

	FrameBuffer = NewBuffer;

	// 单帧时长 = 当前帧 timestamp - 上一帧 timestamp。钳到 kMinFrameDelayMs
	// 以避免 duration=0 的合法 WebP 让上层 Tick 忙等。
	const int RawDuration = Timestamp - PrevFrameTimestamp;
	PrevFrameTimestamp = Timestamp;

	const uint32 FrameDuration = RawDuration > 0
		? static_cast<uint32>(RawDuration)
		: kMinFrameDelayMs;
	return FMath::Max(FrameDuration, kMinFrameDelayMs);
}

void FWebpDecoder::Reset()
{
	if (Decoder)
	{
		WebPAnimDecoderReset(Decoder);
	}
	FrameBuffer = nullptr;
	PrevFrameTimestamp = 0;
	CompletedLoops = 0;
}

const FColor* FWebpDecoder::GetFrameBuffer() const
{
	return reinterpret_cast<const FColor*>(FrameBuffer);
}

uint32 FWebpDecoder::GetDuration(uint32 DefaultFrameDelay) const
{
	// WebP 比特流中每帧自带 duration（毫秒）。LoadFromMemory 阶段已对每帧
	// 钳制最小值后求和；DefaultFrameDelay 在这里仅作为完全没有解码到任何帧
	// 时的最终兜底。
	if (Duration > 0) return Duration;
	const uint32 FrameCount = AnimInfo.frame_count > 0 ? AnimInfo.frame_count : 1;
	return DefaultFrameDelay * FrameCount;
}

bool FWebpDecoder::SupportsTransparency() const
{
	return Features.has_alpha != 0;
}

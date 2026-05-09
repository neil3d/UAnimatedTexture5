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
#include "Math/IntRect.h"
#include "AnimatedTextureDecoder.h"
#include "giflib/gif_lib.h"

/**
 * FGIFDecoder
 *
 * 流式 GIF 解码器：与 FWebpDecoder 的内存策略对齐——压缩态 FileBlob 由
 * 上层 UAnimatedTexture2D 持有，本类只在内存中保留：
 *   - 每帧元数据 FFrameMeta（约 32B/帧）；
 *   - 单行索引行缓冲 mLineBuffer（W bytes，跨帧复用）；
 *   - 当前 RGBA canvas mFrameBuffer；
 *   - 按需的 DISPOSE_PREVIOUS 快照 mSnapshotBuffer。
 *
 * 不再调用 DGifSlurp，也不再触碰 mGIF->SavedImages —— 全部帧像素按需在
 * NextFrame 阶段流式解出。循环回卷靠 DGifCloseFile + DGifOpen 重置 cursor
 * 并复位 LZW 状态机。
 */
class FGIFDecoder : public FAnimatedTextureDecoder
{
public:
	FGIFDecoder() = default;
	virtual ~FGIFDecoder() override;

	virtual bool LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize) override;
	virtual void Close() override;

	virtual uint32 NextFrame(uint32 DefaultFrameDelay, bool bLooping) override;
	virtual void Reset() override;

	virtual uint32 GetWidth() const override;
	virtual uint32 GetHeight() const override;
	virtual const FColor* GetFrameBuffer() const override;

	virtual uint32 GetDuration(uint32 DefaultFrameDelay) const override;
	virtual bool SupportsTransparency() const override;

	// NETSCAPE2.0 应用扩展中声明的循环次数（0 = 无限循环 / 文件未声明）。
	// 配合 UAnimatedTexture2D::bRespectFileLoopCount 使用。
	virtual uint32 GetLoopCount() const override { return mLoopCountFromFile; }

	// 已完成循环次数；NextFrame 跨过末尾时 mLoopCount 会自增。
	virtual uint32 GetCompletedLoops() const override { return static_cast<uint32>(mLoopCount); }

private:
	// 单帧元数据：LoadFromMemory 阶段一次性预扫缓存；运行期只读，不再触碰
	// mGIF->SavedImages（流式模式下它一直为空）。
	struct FFrameMeta
	{
		FIntRect Rect;          // 已 Clamp 到画布范围内
		bool bInterlace = false;
		int Disposal = DISPOSAL_UNSPECIFIED;
		int DelayMs = 0;        // GCB 的 DelayTime × 10（GIF 单位 1/100 秒）
		int Transparent = NO_TRANSPARENT_COLOR;
	};

	// 把矩形 [Left, Top, Width, Height] 裁剪到画布范围内（半开区间 [Min, Max)）。
	FIntRect ClampRectToCanvas(int Left, int Top, int Width, int Height) const;

	// 根据上一帧的 disposal 模式清理画布上对应区域。
	// DISPOSAL_UNSPECIFIED / DISPOSE_DO_NOT 不动；
	// DISPOSE_BACKGROUND 清为 mResolvedBgColor；
	// DISPOSE_PREVIOUS 从 mSnapshotBuffer 恢复对应区域。
	void DisposeFrame(int DisposalMode, const FIntRect& Rect);

	// 在绘制 disposal==PREVIOUS 的帧之前调用，把当前画布拷贝一份留作恢复点。
	void SaveSnapshot();
	void RestoreSnapshot(const FIntRect& Rect);

	// 计算"画布初始化 / DISPOSE_BACKGROUND 清理"应使用的填充颜色。
	// 启发式（兼容 Chrome/Firefox 行为同时尽量接近 GIF89a 规范）：
	//   - 若 SColorMap 为空 / SBackGroundColor 越界 → (0,0,0,0) 透明
	//   - 若 SBackGroundColor 被任何一帧用作 transparent index → (0,0,0,0) 透明
	//   - 否则按规范取 SColorMap->Colors[SBackGroundColor]，alpha=255
	// 流式版本：直接读 mFrames[i].Transparent，不再依赖 mGIF->SavedImages。
	FColor ResolveBgColorFromMetadata() const;

	// LoadFromMemory 阶段：预扫描整个文件，填充
	//   - mFrames（每帧 Rect/Disposal/Delay/Transparent/Interlace）
	//   - mLoopCountFromFile（NETSCAPE2.0）
	//   - mbHasTransparency
	// 不解任何 LZW 像素，跳过 image data sub-blocks（DGifGetCodeNext 即可，
	// 不要用 DGifGetCode—— 后者会再读一次 codesize 字节造成 cursor 偏移）。
	bool ScanMetadata();

	// 解析 APPLICATION ext。调用前已经 DGifGetExtension 拿到 ID sub-block；
	// 若识别出 "NETSCAPE2.0" 则继续读后续 sub-blocks 并提取 loop_count
	// 写入 mLoopCountFromFile。返回前 drain 完所有后续 sub-blocks。
	void ParseNetscapeApplicationExt(const GifByteType* IdSubBlock);

	// 关闭当前 mGIF 并重新 DGifOpen（cursor 回卷到 Begin），让 NextFrame
	// 从首帧开始；同时 LZW 状态机被全量重置。循环回卷与 Reset 都走这里。
	bool RewindToFirstRecord();

	// 流式解码 cursor 当前位置上的"下一帧"到 mFrameBuffer。
	// 失败时打 Warning（skip-and-continue 容错）。
	bool DecodeFrameAtCursor(int FrameIndex);

	// 把 mLineBuffer 一行索引位图按 transparent / 越界跳过 / palette 查表后
	// 写到 canvas 的 (dstTop+srcY, dstLeft+x) 位置。
	void CompositeRow(int srcY, int dstTop, int dstLeft, int Width, ColorMapObject* CM, int Transparent);

public:
	// 把外部 buffer 包成"游标"，供 giflib 输入回调使用：避免越读，并消除
	// UserData 字段直接保存裸指针带来的"指针即数据流位置"的二义性。
	// Begin 字段供 RewindToFirstRecord 用：循环回卷时直接复位 Cur 到 Begin。
	// public 只是为了让 .cpp 中的输入回调能引用，调用方不应直接使用此类型。
	struct FInputCursor
	{
		const uint8* Begin = nullptr;
		const uint8* Cur = nullptr;
		const uint8* End = nullptr;
	};

private:
	int mCurrentFrame = 0;
	int mLoopCount = 0;

	// 上一帧（即将影响"下一帧渲染前"的 disposal 行为）的状态。
	int mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	FIntRect mPrevRect = FIntRect();
	int mPrevTransparentColor = NO_TRANSPARENT_COLOR;

	// DISPOSE_PREVIOUS 用的画布快照；按需分配。
	TArray<FColor> mSnapshotBuffer;

	// LoadFromMemory 时由 ResolveBgColorFromMetadata 一次算出并缓存，
	// 供 LoadFromMemory / Reset / DisposeFrame(BACKGROUND) / RestoreSnapshot fallback 复用。
	FColor mResolvedBgColor = FColor(0, 0, 0, 0);

	GifFileType* mGIF = nullptr;
	TArray<FColor> mFrameBuffer;
	FInputCursor mInputCursor;

	// 单行索引位图缓冲（W bytes），跨帧复用，避免每帧 malloc。
	TArray<GifPixelType> mLineBuffer;

	// 每帧元数据；由 ScanMetadata 一次性填充，运行期只读。
	TArray<FFrameMeta> mFrames;

	// NETSCAPE2.0 应用扩展中声明的循环次数；0 = 无限循环 / 文件未声明。
	uint32 mLoopCountFromFile = 0;

	// 预扫描期间是否在任意一帧 GCB 见到 transparent 索引。
	bool mbHasTransparency = false;
};

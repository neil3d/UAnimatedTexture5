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

	// 已完成循环次数；NextFrame 跨过末尾时 mLoopCount 会自增。
	// 注意：当前 giflib 实现尚未解析 NETSCAPE2.0 应用扩展，因此 GetLoopCount()
	// 仍按基类默认返回 0（视为无限循环 / 未声明）。
	virtual uint32 GetCompletedLoops() const override { return static_cast<uint32>(mLoopCount); }

private:
	// 解析单帧的 GraphicsControlBlock：取出 disposal / 延时（毫秒） / 透明色索引。
	// 缺省值：DISPOSAL_UNSPECIFIED / 0 / NO_TRANSPARENT_COLOR。
	void ParseGCB(const SavedImage& Image, int& OutDisposalMode, int& OutDelayMs, int& OutTransparentColor) const;

	// 把矩形 [Left, Top, Width, Height] 裁剪到画布范围内（半开区间 [Min, Max)）。
	FIntRect ClampRectToCanvas(int Left, int Top, int Width, int Height) const;

	// 根据上一帧的 disposal 模式清理画布上对应区域。
	// DISPOSAL_UNSPECIFIED / DISPOSE_DO_NOT 不动；
	// DISPOSE_BACKGROUND 清为 mResolvedBgColor（按 GIF89a 规范，可能是 GCT[bg] 不透明
	// 或全透明，由 ResolveBgColor 启发式决定）；
	// DISPOSE_PREVIOUS 从 snapshot 恢复对应区域。
	void DisposeFrame(int DisposalMode, const FIntRect& Rect);

	// 在绘制 disposal==PREVIOUS 的帧之前调用，把当前画布拷贝一份留作恢复点。
	void SaveSnapshot();
	void RestoreSnapshot(const FIntRect& Rect);

	// 计算"画布初始化 / DISPOSE_BACKGROUND 清理"应使用的填充颜色。
	// 启发式（兼容 Chrome/Firefox 行为同时尽量接近 GIF89a 规范）：
	//   - 若 SColorMap 为空 / SBackGroundColor 越界 → (0,0,0,0) 透明
	//   - 若 SBackGroundColor 被任何一帧用作 transparent index（"bg 即透明"是
	//     GIF 作者意图） → (0,0,0,0) 透明
	//   - 否则按规范取 SColorMap->Colors[SBackGroundColor]，alpha=255
	FColor ResolveBgColor() const;

public:
	// 把外部 buffer 包成"游标"，供 giflib 输入回调使用：避免越读，并消除 UserData 字段
	// 直接保存裸指针带来的"指针即数据流位置"的二义性。public 只是为了让 .cpp 中的
	// 输入回调能引用，调用方不应直接使用此类型。
	struct FInputCursor
	{
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

	// LoadFromMemory 时由 ResolveBgColor 一次算出并缓存，
	// 供 LoadFromMemory / Reset / DisposeFrame(BACKGROUND) / RestoreSnapshot fallback 复用。
	FColor mResolvedBgColor = FColor(0, 0, 0, 0);

	GifFileType* mGIF = nullptr;
	TArray<FColor> mFrameBuffer;
	FInputCursor mInputCursor;
};

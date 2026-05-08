/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "GIFDecoder.h"
#include "AnimatedTextureModule.h"

FGIFDecoder::~FGIFDecoder()
{
	Close();
}

// 把 giflib 的读取请求映射到 [Cur, End) 区间内，避免越读。
static int _GIF_InputFunc(GifFileType* gifFile, GifByteType* buffer, int length)
{
	auto* Cursor = static_cast<FGIFDecoder::FInputCursor*>(gifFile->UserData);
	if (!Cursor || !Cursor->Cur || Cursor->Cur >= Cursor->End || length <= 0)
	{
		return 0;
	}

	const PTRINT Remaining = Cursor->End - Cursor->Cur;
	const int ToRead = static_cast<int>(FMath::Min<PTRINT>(length, Remaining));
	if (ToRead <= 0)
	{
		return 0;
	}

	FMemory::Memcpy(buffer, Cursor->Cur, ToRead);
	Cursor->Cur += ToRead;
	return ToRead;
}

bool FGIFDecoder::LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize)
{
	if (!InBuffer || InBufferSize == 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: empty input buffer."));
		return false;
	}

	mInputCursor.Cur = InBuffer;
	mInputCursor.End = InBuffer + InBufferSize;

	int gifError = 0;
	mGIF = DGifOpen(&mInputCursor, _GIF_InputFunc, &gifError);
	if (mGIF == nullptr)
	{
		FString Error(GifErrorString(gifError));
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: GIF file open failed, %s."), *Error);
		mInputCursor = FInputCursor();
		return false;
	}

	gifError = DGifSlurp(mGIF);
	if (gifError != GIF_OK)
	{
		FString Error(GifErrorString(mGIF->Error));
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: GIF file load failed, %s."), *Error);
		// 不在此处释放 mGIF，由 Close() 统一处理。
		return false;
	}

	if (mGIF->SWidth <= 0 || mGIF->SHeight <= 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: invalid canvas size %dx%d."), mGIF->SWidth, mGIF->SHeight);
		return false;
	}

	// 在 init 画布之前先解析 bg 颜色（依赖 mGIF->SavedImages 的 GCB 信息）。
	mResolvedBgColor = ResolveBgColor();

	const int32 PixelCount = mGIF->SWidth * mGIF->SHeight;
	mFrameBuffer.Init(mResolvedBgColor, PixelCount);

	mCurrentFrame = 0;
	mLoopCount = 0;
	mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	mPrevRect = FIntRect();
	mPrevTransparentColor = NO_TRANSPARENT_COLOR;
	mSnapshotBuffer.Empty();

	// DGifSlurp 已把全部数据读入内存，cursor 后续不再使用。
	mInputCursor = FInputCursor();

	// 诊断日志：dump GIF 元数据 + 每帧结构（默认 Verbose 级别，需 `Log LogAnimTexture Verbose`）
	if (UE_LOG_ACTIVE(LogAnimTexture, Verbose))
	{
		const int32 GctSize = mGIF->SColorMap ? mGIF->SColorMap->ColorCount : 0;
		FString BgDescription;
		if (mResolvedBgColor.A == 0)
		{
			BgDescription = FString::Printf(TEXT("transparent (bg=%d, gct=%d)"), mGIF->SBackGroundColor, GctSize);
		}
		else
		{
			BgDescription = FString::Printf(TEXT("(R=%u,G=%u,B=%u,A=255) from gct[%d]"),
				mResolvedBgColor.R, mResolvedBgColor.G, mResolvedBgColor.B, mGIF->SBackGroundColor);
		}

		UE_LOG(LogAnimTexture, Verbose,
			TEXT("FGIFDecoder: loaded canvas=%dx%d gct=%d bg-idx=%d resolved-bg=%s frames=%d"),
			mGIF->SWidth, mGIF->SHeight, GctSize, mGIF->SBackGroundColor, *BgDescription, mGIF->ImageCount);

		for (int i = 0; i < mGIF->ImageCount; i++)
		{
			const SavedImage& Img = mGIF->SavedImages[i];
			const GifImageDesc& Desc = Img.ImageDesc;
			int Disposal = DISPOSAL_UNSPECIFIED;
			int Delay = 0;
			int Trans = NO_TRANSPARENT_COLOR;
			ParseGCB(Img, Disposal, Delay, Trans);
			const int LctSize = Desc.ColorMap ? Desc.ColorMap->ColorCount : 0;
			UE_LOG(LogAnimTexture, Verbose,
				TEXT("  frame[%3d] rect=(%4d,%4d) %4dx%-4d lct=%3d %s disp=%d delay=%dms trans=%d"),
				i, Desc.Left, Desc.Top, Desc.Width, Desc.Height, LctSize,
				Desc.Interlace ? TEXT("I") : TEXT(" "), Disposal, Delay, Trans);
		}
	}

	return true;
}

void FGIFDecoder::Close()
{
	if (mGIF)
	{
		int error = 0;
		DGifCloseFile(mGIF, &error);
		mGIF = nullptr;
	}

	mFrameBuffer.Empty();
	mSnapshotBuffer.Empty();
	mInputCursor = FInputCursor();

	mCurrentFrame = 0;
	mLoopCount = 0;
	mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	mPrevRect = FIntRect();
	mPrevTransparentColor = NO_TRANSPARENT_COLOR;
	mResolvedBgColor = FColor(0, 0, 0, 0);
}

FColor FGIFDecoder::ResolveBgColor() const
{
	if (!mGIF || !mGIF->SColorMap)
	{
		return FColor(0, 0, 0, 0);
	}

	const int BgIdx = mGIF->SBackGroundColor;
	if (BgIdx < 0 || BgIdx >= mGIF->SColorMap->ColorCount)
	{
		return FColor(0, 0, 0, 0);
	}

	// 扫描所有帧的 GCB transparent index：若 bg 索引被任何一帧用作 transparent，
	// 则 GIF 作者意图是"bg 即透明"（welcome2 / x-trans / Eokxd 都是这种），用透明。
	for (int i = 0; i < mGIF->ImageCount; i++)
	{
		int Disposal = DISPOSAL_UNSPECIFIED;
		int Delay = 0;
		int Trans = NO_TRANSPARENT_COLOR;
		ParseGCB(mGIF->SavedImages[i], Disposal, Delay, Trans);
		if (Trans == BgIdx)
		{
			return FColor(0, 0, 0, 0);
		}
	}

	const GifColorType& Entry = mGIF->SColorMap->Colors[BgIdx];
	return FColor(Entry.Red, Entry.Green, Entry.Blue, 255);
}

void FGIFDecoder::ParseGCB(const SavedImage& Image, int& OutDisposalMode, int& OutDelayMs, int& OutTransparentColor) const
{
	OutDisposalMode = DISPOSAL_UNSPECIFIED;
	OutDelayMs = 0;
	OutTransparentColor = NO_TRANSPARENT_COLOR;

	for (int i = 0; i < Image.ExtensionBlockCount; i++)
	{
		const ExtensionBlock& eb = Image.ExtensionBlocks[i];
		if (eb.Function != GRAPHICS_EXT_FUNC_CODE)
		{
			continue;
		}
		GraphicsControlBlock gcb;
		if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) == GIF_ERROR)
		{
			continue;
		}
		// 同一帧理论上只有一个 GCB，若有多个则后面的覆盖前面的（与 DGifSavedExtensionToGCB 语义一致）。
		OutDisposalMode = gcb.DisposalMode;
		OutDelayMs = gcb.DelayTime * 10;	// GIF 的 DelayTime 单位是 1/100 秒
		OutTransparentColor = gcb.TransparentColor;	// 已包含 NO_TRANSPARENT_COLOR(-1)
	}
}

FIntRect FGIFDecoder::ClampRectToCanvas(int Left, int Top, int Width, int Height) const
{
	const int CanvasW = static_cast<int>(GetWidth());
	const int CanvasH = static_cast<int>(GetHeight());

	const int MinX = FMath::Clamp(Left, 0, CanvasW);
	const int MinY = FMath::Clamp(Top, 0, CanvasH);
	const int MaxX = FMath::Clamp(Left + Width, 0, CanvasW);
	const int MaxY = FMath::Clamp(Top + Height, 0, CanvasH);

	return FIntRect(MinX, MinY, MaxX, MaxY);
}

void FGIFDecoder::DisposeFrame(int DisposalMode, const FIntRect& Rect)
{
	if (Rect.Min.X >= Rect.Max.X || Rect.Min.Y >= Rect.Max.Y)
	{
		return;
	}

	const int CanvasW = static_cast<int>(GetWidth());

	if (DisposalMode == DISPOSE_BACKGROUND)
	{
		// 按 GIF89a 规范：清成 SBackGroundColor。当 bg 与 transparent 索引重合时
		// ResolveBgColor 已退化为 (0,0,0,0)，与 Chrome 行为兼容。
		const FColor Clear = mResolvedBgColor;
		for (int y = Rect.Min.Y; y < Rect.Max.Y; y++)
		{
			FColor* Row = mFrameBuffer.GetData() + y * CanvasW;
			for (int x = Rect.Min.X; x < Rect.Max.X; x++)
			{
				Row[x] = Clear;
			}
		}
	}
	else if (DisposalMode == DISPOSE_PREVIOUS)
	{
		RestoreSnapshot(Rect);
	}
	// DISPOSAL_UNSPECIFIED / DISPOSE_DO_NOT：保留画布。
}

void FGIFDecoder::SaveSnapshot()
{
	mSnapshotBuffer = mFrameBuffer;
}

void FGIFDecoder::RestoreSnapshot(const FIntRect& Rect)
{
	if (mSnapshotBuffer.Num() != mFrameBuffer.Num())
	{
		// 还没拍过有效快照（例如首帧之前），退化为清成 bg 色（与 LoadFromMemory 初始一致）。
		const int CanvasW = static_cast<int>(GetWidth());
		const FColor Clear = mResolvedBgColor;
		for (int y = Rect.Min.Y; y < Rect.Max.Y; y++)
		{
			FColor* Row = mFrameBuffer.GetData() + y * CanvasW;
			for (int x = Rect.Min.X; x < Rect.Max.X; x++)
			{
				Row[x] = Clear;
			}
		}
		return;
	}

	const int CanvasW = static_cast<int>(GetWidth());
	for (int y = Rect.Min.Y; y < Rect.Max.Y; y++)
	{
		FColor* DstRow = mFrameBuffer.GetData() + y * CanvasW;
		const FColor* SrcRow = mSnapshotBuffer.GetData() + y * CanvasW;
		for (int x = Rect.Min.X; x < Rect.Max.X; x++)
		{
			DstRow[x] = SrcRow[x];
		}
	}
}

uint32 FGIFDecoder::NextFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	if (!mGIF || mGIF->ImageCount <= 0)
	{
		return DefaultFrameDelay;
	}

	if (mCurrentFrame < 0 || mCurrentFrame >= mGIF->ImageCount)
	{
		mCurrentFrame = 0;
	}

	const int RenderingFrame = mCurrentFrame;
	const SavedImage& Image = mGIF->SavedImages[RenderingFrame];
	const GifImageDesc& id = Image.ImageDesc;

	// Step 1: 根据"上一帧"的 disposal 清理画布上对应区域。
	DisposeFrame(mPrevDisposalMode, mPrevRect);

	// Step 2: 解析当前帧 GCB。
	int curDisposalMode = DISPOSAL_UNSPECIFIED;
	int delayTime = 0;
	int transparentColor = NO_TRANSPARENT_COLOR;
	ParseGCB(Image, curDisposalMode, delayTime, transparentColor);

	// 把当前子图像区域裁剪到画布范围内。
	const FIntRect curRect = ClampRectToCanvas(id.Left, id.Top, id.Width, id.Height);

	// Step 3: 当前帧 disposal == PREVIOUS，则在绘制前先快照画布。
	if (curDisposalMode == DISPOSE_PREVIOUS)
	{
		SaveSnapshot();
	}

	// Step 4: 把当前子图像绘到画布。
	int32 PixelsWritten = 0;
	int32 PixelsSkippedTransparent = 0;
	int32 PixelsSkippedBadIndex = 0;
	ColorMapObject* colorMap = id.ColorMap ? id.ColorMap : mGIF->SColorMap;
	if (!colorMap)
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: Frame %d has no color map, skipping."), RenderingFrame);
	}
	else if (curRect.Min.X < curRect.Max.X && curRect.Min.Y < curRect.Max.Y)
	{
		const int CanvasW = static_cast<int>(GetWidth());
		const int ImgWidth = static_cast<int>(id.Width);
		const GifByteType* Raster = Image.RasterBits;

		for (int y = curRect.Min.Y; y < curRect.Max.Y; y++)
		{
			FColor* DstRow = mFrameBuffer.GetData() + y * CanvasW;
			const int srcY = y - id.Top;
			const GifByteType* SrcRow = Raster + srcY * ImgWidth;

			for (int x = curRect.Min.X; x < curRect.Max.X; x++)
			{
				const int srcX = x - id.Left;
				const int c = SrcRow[srcX];

				// 透明像素：跳过，保留画布既有内容（统一处理，无关 disposal）。
				if (c == transparentColor)
				{
					++PixelsSkippedTransparent;
					continue;
				}
				// 颜色索引越界：跳过。
				if (c < 0 || c >= colorMap->ColorCount)
				{
					++PixelsSkippedBadIndex;
					continue;
				}

				const GifColorType& colorEntry = colorMap->Colors[c];
				FColor& out = DstRow[x];
				out.R = colorEntry.Red;
				out.G = colorEntry.Green;
				out.B = colorEntry.Blue;
				out.A = 255;
				++PixelsWritten;
			}
		}
	}

	// 诊断日志：每帧渲染 trace（默认不开启，需 `Log LogAnimTexture VeryVerbose`）
	if (UE_LOG_ACTIVE(LogAnimTexture, VeryVerbose))
	{
		UE_LOG(LogAnimTexture, VeryVerbose,
			TEXT("FGIFDecoder::NextFrame frame=%d prevDisp=%d curDisp=%d trans=%d rect=(%d,%d)->(%d,%d) writ=%d skipTrans=%d skipBad=%d delay=%dms loop=%d"),
			RenderingFrame, mPrevDisposalMode, curDisposalMode, transparentColor,
			curRect.Min.X, curRect.Min.Y, curRect.Max.X, curRect.Max.Y,
			PixelsWritten, PixelsSkippedTransparent, PixelsSkippedBadIndex,
			delayTime, mLoopCount);
	}

	// Step 6: 记录"当前帧"为下一帧解码时使用的 prev 状态。
	mPrevDisposalMode = curDisposalMode;
	mPrevRect = curRect;
	mPrevTransparentColor = transparentColor;

	// 推进帧索引。
	mCurrentFrame++;
	if (mCurrentFrame >= mGIF->ImageCount)
	{
		mLoopCount++;
		if (bLooping)
		{
			// 回到起点；prev 状态保留，下一次 NextFrame 会基于上一帧的 disposal 正确清理画布。
			mCurrentFrame = 0;
		}
		else
		{
			// 不循环：停在最后一帧。
			mCurrentFrame = mGIF->ImageCount - 1;
		}
	}

	return delayTime == 0 ? DefaultFrameDelay : delayTime;
}

void FGIFDecoder::Reset()
{
	mCurrentFrame = 0;
	mLoopCount = 0;
	mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	mPrevRect = FIntRect();
	mPrevTransparentColor = NO_TRANSPARENT_COLOR;
	mSnapshotBuffer.Empty();

	// 重新把画布填回 mResolvedBgColor，与 LoadFromMemory 后的初始状态一致。
	if (mFrameBuffer.Num() > 0)
	{
		const FColor Clear = mResolvedBgColor;
		for (FColor& Pixel : mFrameBuffer)
		{
			Pixel = Clear;
		}
	}
}

uint32 FGIFDecoder::GetWidth() const
{
	if (mGIF)
		return mGIF->SWidth;
	else
		return 1;
}

uint32 FGIFDecoder::GetHeight() const
{
	if (mGIF)
		return mGIF->SHeight;
	else
		return 1;
}

const FColor* FGIFDecoder::GetFrameBuffer() const
{
	return mFrameBuffer.GetData();
}

uint32 FGIFDecoder::GetDuration(uint32 DefaultFrameDelay) const
{
	if (!mGIF)
		return 0;

	int duration = 0;
	for (int i = 0; i < mGIF->ImageCount; i++)
	{
		int disposal = DISPOSAL_UNSPECIFIED;
		int delayTime = 0;
		int transparent = NO_TRANSPARENT_COLOR;
		ParseGCB(mGIF->SavedImages[i], disposal, delayTime, transparent);
		duration += (delayTime == 0) ? DefaultFrameDelay : delayTime;
	}

	return duration;
}

bool FGIFDecoder::SupportsTransparency() const
{
	if (!mGIF)
		return false;

	for (int i = 0; i < mGIF->ImageCount; i++)
	{
		int disposal = DISPOSAL_UNSPECIFIED;
		int delayTime = 0;
		int transparent = NO_TRANSPARENT_COLOR;
		ParseGCB(mGIF->SavedImages[i], disposal, delayTime, transparent);
		if (transparent != NO_TRANSPARENT_COLOR)
		{
			return true;
		}
	}
	return false;
}

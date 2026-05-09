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
	// 重入保护：清掉上一次（成功或半成功）的状态。
	Close();

	if (!InBuffer || InBufferSize == 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: empty input buffer."));
		return false;
	}

	mInputCursor.Begin = InBuffer;
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

	if (mGIF->SWidth <= 0 || mGIF->SHeight <= 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: invalid canvas size %dx%d."), mGIF->SWidth, mGIF->SHeight);
		return false;
	}

	// 预扫描：走完整个文件，填好 mFrames / mLoopCountFromFile / mbHasTransparency。
	// 不解任何 LZW 像素；只跳过数据 sub-blocks 推进 cursor。
	if (!ScanMetadata())
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: metadata scan failed."));
		return false;
	}

	// 解析背景色（依赖 mFrames 已就绪）。
	mResolvedBgColor = ResolveBgColorFromMetadata();

	const int32 PixelCount = mGIF->SWidth * mGIF->SHeight;
	mFrameBuffer.Init(mResolvedBgColor, PixelCount);
	mLineBuffer.SetNumUninitialized(mGIF->SWidth);

	mCurrentFrame = 0;
	mLoopCount = 0;
	mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	mPrevRect = FIntRect();
	mPrevTransparentColor = NO_TRANSPARENT_COLOR;
	mSnapshotBuffer.Empty();

	// 诊断日志：dump GIF 元数据 + 每帧结构（默认 Verbose 级别，需 `Log LogAnimTexture Verbose`）。
	// 流式版本：所有信息从 mFrames 读，不再触碰 mGIF->SavedImages（流式模式下它一直为空）。
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
			TEXT("FGIFDecoder: loaded canvas=%dx%d gct=%d bg-idx=%d resolved-bg=%s frames=%d loop_count=%u"),
			mGIF->SWidth, mGIF->SHeight, GctSize, mGIF->SBackGroundColor, *BgDescription,
			mFrames.Num(), mLoopCountFromFile);

		for (int i = 0; i < mFrames.Num(); i++)
		{
			const FFrameMeta& M = mFrames[i];
			UE_LOG(LogAnimTexture, Verbose,
				TEXT("  frame[%3d] rect=(%4d,%4d)->(%4d,%4d) %s disp=%d delay=%dms trans=%d"),
				i, M.Rect.Min.X, M.Rect.Min.Y, M.Rect.Max.X, M.Rect.Max.Y,
				M.bInterlace ? TEXT("I") : TEXT(" "), M.Disposal, M.DelayMs, M.Transparent);
		}
	}

	// 关闭预扫的 mGIF 并重新 DGifOpen，让 cursor 回卷到 ScreenDesc 之后；
	// LZW 状态机随之全量重置。NextFrame 从首帧开始。
	return RewindToFirstRecord();
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
	mLineBuffer.Empty();
	mFrames.Empty();
	mInputCursor = FInputCursor();

	mCurrentFrame = 0;
	mLoopCount = 0;
	mPrevDisposalMode = DISPOSAL_UNSPECIFIED;
	mPrevRect = FIntRect();
	mPrevTransparentColor = NO_TRANSPARENT_COLOR;
	mResolvedBgColor = FColor(0, 0, 0, 0);
	mLoopCountFromFile = 0;
	mbHasTransparency = false;
}

bool FGIFDecoder::ScanMetadata()
{
	if (!mGIF)
	{
		return false;
	}

	GifRecordType RecordType;
	FFrameMeta Pending;
	bool bHasPendingGCB = false;

	while (true)
	{
		if (DGifGetRecordType(mGIF, &RecordType) == GIF_ERROR)
		{
			UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: ScanMetadata GetRecordType failed: %s"),
				*FString(GifErrorString(mGIF->Error)));
			return false;
		}

		if (RecordType == TERMINATE_RECORD_TYPE)
		{
			break;
		}

		if (RecordType == EXTENSION_RECORD_TYPE)
		{
			int ExtCode = 0;
			GifByteType* ExtData = nullptr;
			if (DGifGetExtension(mGIF, &ExtCode, &ExtData) == GIF_ERROR)
			{
				UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: ScanMetadata GetExtension failed."));
				return false;
			}

			if (ExtCode == GRAPHICS_EXT_FUNC_CODE && ExtData != nullptr)
			{
				// ExtData 是 Pascal-style：第 0 字节是长度，后续为内容（与 DGifSlurp 中
				// `GifAddExtensionBlock(..., ExtData[0], &ExtData[1])` 调用一致）。
				GraphicsControlBlock GCB;
				if (DGifExtensionToGCB(ExtData[0], ExtData + 1, &GCB) == GIF_OK)
				{
					Pending.Disposal = GCB.DisposalMode;
					Pending.DelayMs = GCB.DelayTime * 10;	// GIF DelayTime 单位 1/100 秒
					Pending.Transparent = GCB.TransparentColor;	// 已含 NO_TRANSPARENT_COLOR(-1)
					bHasPendingGCB = true;
					if (Pending.Transparent != NO_TRANSPARENT_COLOR)
					{
						mbHasTransparency = true;
					}
				}
				// drain 后续 sub-blocks（GCB 通常只有一段，但还是按规范读到 NULL）。
				while (true)
				{
					if (DGifGetExtensionNext(mGIF, &ExtData) == GIF_ERROR)
					{
						UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: drain GCB sub-blocks failed."));
						return false;
					}
					if (ExtData == nullptr)
					{
						break;
					}
				}
			}
			else if (ExtCode == APPLICATION_EXT_FUNC_CODE && ExtData != nullptr)
			{
				// ParseNetscapeApplicationExt 内部会接管后续 sub-blocks 直至 NULL。
				ParseNetscapeApplicationExt(ExtData);
			}
			else
			{
				// 其它扩展（COMMENT / PLAINTEXT / ...）：drain 全部 sub-blocks。
				while (true)
				{
					if (DGifGetExtensionNext(mGIF, &ExtData) == GIF_ERROR)
					{
						UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: drain ext sub-blocks failed."));
						return false;
					}
					if (ExtData == nullptr)
					{
						break;
					}
				}
			}
		}
		else if (RecordType == IMAGE_DESC_RECORD_TYPE)
		{
			if (DGifGetImageDesc(mGIF) == GIF_ERROR)
			{
				UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: ScanMetadata GetImageDesc failed: %s"),
					*FString(GifErrorString(mGIF->Error)));
				return false;
			}

			FFrameMeta Meta;
			Meta.Rect = ClampRectToCanvas(mGIF->Image.Left, mGIF->Image.Top, mGIF->Image.Width, mGIF->Image.Height);
			Meta.bInterlace = mGIF->Image.Interlace;
			if (bHasPendingGCB)
			{
				Meta.Disposal = Pending.Disposal;
				Meta.DelayMs = Pending.DelayMs;
				Meta.Transparent = Pending.Transparent;
			}
			// 否则保持 FFrameMeta 默认值（DISPOSAL_UNSPECIFIED / 0 / NO_TRANSPARENT_COLOR）。
			mFrames.Add(Meta);

			Pending = FFrameMeta();
			bHasPendingGCB = false;

			// 跳过 LZW 数据 sub-blocks（不解 LZW，仅消费字节流推进 cursor）。
			// 注意：DGifGetImageDesc 内部已经调用 DGifSetupDecompress 读了 codesize 字节，
			// 所以这里直接从首个 data sub-block 开始 drain。
			// **绝对不要改用 DGifGetCode** —— 它会再读一次 codesize 字节造成 cursor 偏移。
			GifByteType* CodeBlock = nullptr;
			do
			{
				if (DGifGetCodeNext(mGIF, &CodeBlock) == GIF_ERROR)
				{
					UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: ScanMetadata drain LZW failed."));
					return false;
				}
			} while (CodeBlock != nullptr);
		}
		// UNDEFINED_RECORD_TYPE / SCREEN_DESC_RECORD_TYPE 不应出现；DGifGetRecordType
		// 已经在 dgif_lib.c 中报错返回 ERROR。
	}

	if (mFrames.Num() <= 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: scan completed but no frames found."));
		return false;
	}
	return true;
}

void FGIFDecoder::ParseNetscapeApplicationExt(const GifByteType* IdSubBlock)
{
	// 调用前已经 DGifGetExtension 拿到第一段 ID sub-block；这里继续读后续 sub-blocks。
	// NETSCAPE2.0 格式：
	//   - 第一段 sub-block：长度=11，内容="NETSCAPE2.0" (11 bytes ASCII，无 NUL 结尾)
	//   - 后续 sub-block：长度=3，内容 [0x01, LoopCountLo, LoopCountHi]
	//   - 也可能有 0xFB Buffering Sub-block（罕见，本实现忽略）
	bool bIsNetscape = false;
	if (IdSubBlock != nullptr && IdSubBlock[0] >= 11)
	{
		// IdSubBlock[0] = 长度（Pascal style），IdSubBlock[1..1+11] = ASCII。
		// 用 Memcmp 而非 strcmp（数据非 NUL 结尾）。
		bIsNetscape = (FMemory::Memcmp(IdSubBlock + 1, "NETSCAPE2.0", 11) == 0);
	}

	GifByteType* SubBlock = nullptr;
	while (true)
	{
		if (DGifGetExtensionNext(mGIF, &SubBlock) == GIF_ERROR)
		{
			UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: drain APPLICATION ext sub-blocks failed."));
			return;
		}
		if (SubBlock == nullptr)
		{
			break;
		}

		// SubBlock[0] = 长度，SubBlock[1..] = data。
		// NETSCAPE2.0 期望：SubBlock[0]=3 / SubBlock[1]=0x01 / SubBlock[2-3]=loop count LE。
		if (bIsNetscape && SubBlock[0] >= 3 && SubBlock[1] == 0x01)
		{
			const uint16 Lo = SubBlock[2];
			const uint16 Hi = SubBlock[3];
			mLoopCountFromFile = static_cast<uint32>(Lo | (Hi << 8));
		}
	}
}

bool FGIFDecoder::RewindToFirstRecord()
{
	if (mGIF)
	{
		int err = 0;
		DGifCloseFile(mGIF, &err);
		mGIF = nullptr;
	}
	mInputCursor.Cur = mInputCursor.Begin;

	int gifError = 0;
	mGIF = DGifOpen(&mInputCursor, _GIF_InputFunc, &gifError);
	if (mGIF == nullptr)
	{
		FString Error(GifErrorString(gifError));
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: rewind DGifOpen failed: %s"), *Error);
		return false;
	}
	return true;
}

bool FGIFDecoder::DecodeFrameAtCursor(int FrameIndex)
{
	if (!mGIF)
	{
		return false;
	}

	// 跳过累积扩展（GCB / APPLICATION / COMMENT 等已被预扫缓存到 mFrames，
	// 运行时不再重复处理；只需消费字节流让 cursor 落在下一个 IMAGE_DESC 上）。
	GifRecordType Type = UNDEFINED_RECORD_TYPE;
	while (true)
	{
		if (DGifGetRecordType(mGIF, &Type) == GIF_ERROR)
		{
			return false;
		}
		if (Type == TERMINATE_RECORD_TYPE)
		{
			return false;
		}
		if (Type == IMAGE_DESC_RECORD_TYPE)
		{
			break;
		}
		// EXTENSION_RECORD_TYPE：drain 全部 sub-blocks。
		int ExtCode = 0;
		GifByteType* ExtData = nullptr;
		if (DGifGetExtension(mGIF, &ExtCode, &ExtData) == GIF_ERROR)
		{
			return false;
		}
		while (true)
		{
			if (DGifGetExtensionNext(mGIF, &ExtData) == GIF_ERROR)
			{
				return false;
			}
			if (ExtData == nullptr)
			{
				break;
			}
		}
	}

	if (DGifGetImageDesc(mGIF) == GIF_ERROR)
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: GetImageDesc failed for frame %d"), FrameIndex);
		return false;
	}

	const GifImageDesc& id = mGIF->Image;
	ColorMapObject* ColorMap = id.ColorMap ? id.ColorMap : mGIF->SColorMap;

	if (id.Width <= 0 || id.Height <= 0)
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: frame %d has invalid size %dx%d; draining LZW"),
			FrameIndex, id.Width, id.Height);
		// 仍要 drain 这一帧的 LZW 数据 sub-blocks，避免 cursor 卡住影响后续帧。
		GifByteType* CodeBlock = nullptr;
		do
		{
			if (DGifGetCodeNext(mGIF, &CodeBlock) == GIF_ERROR)
			{
				return false;
			}
		} while (CodeBlock != nullptr);
		return false;
	}

	if (!ColorMap)
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: frame %d has no color map; draining LZW"), FrameIndex);
		GifByteType* CodeBlock = nullptr;
		do
		{
			if (DGifGetCodeNext(mGIF, &CodeBlock) == GIF_ERROR)
			{
				return false;
			}
		} while (CodeBlock != nullptr);
		return false;
	}

	// 当前帧 transparent 索引：从预扫缓存读取。
	const int Transparent = mFrames.IsValidIndex(FrameIndex)
		? mFrames[FrameIndex].Transparent
		: NO_TRANSPARENT_COLOR;

	// 行缓冲：按当前帧子图宽度调整（GIF 子图宽度 ≤ 画布宽度，但保险起见取 max）。
	if (mLineBuffer.Num() < id.Width)
	{
		mLineBuffer.SetNumUninitialized(id.Width);
	}

	if (id.Interlace)
	{
		// 4-pass 隔行扫描；offsets/jumps 与 dgif_lib.c L1235-1255 (DGifSlurp) 保持一致。
		// DGifGetLine 按发送顺序返回：先全部 offset=0 行，再 offset=4 行，依此类推。
		static const int InterlacedOffset[] = { 0, 4, 2, 1 };
		static const int InterlacedJumps[]  = { 8, 8, 4, 2 };

		for (int p = 0; p < 4; ++p)
		{
			for (int j = InterlacedOffset[p]; j < id.Height; j += InterlacedJumps[p])
			{
				if (DGifGetLine(mGIF, mLineBuffer.GetData(), id.Width) == GIF_ERROR)
				{
					UE_LOG(LogAnimTexture, Warning,
						TEXT("FGIFDecoder: GetLine failed in interlaced frame %d (pass=%d row=%d)"),
						FrameIndex, p, j);
					return false;
				}
				CompositeRow(j, id.Top, id.Left, id.Width, ColorMap, Transparent);
			}
		}
	}
	else
	{
		for (int j = 0; j < id.Height; ++j)
		{
			if (DGifGetLine(mGIF, mLineBuffer.GetData(), id.Width) == GIF_ERROR)
			{
				UE_LOG(LogAnimTexture, Warning,
					TEXT("FGIFDecoder: GetLine failed at row %d of frame %d"), j, FrameIndex);
				return false;
			}
			CompositeRow(j, id.Top, id.Left, id.Width, ColorMap, Transparent);
		}
	}

	return true;
}

void FGIFDecoder::CompositeRow(int srcY, int dstTop, int dstLeft, int Width, ColorMapObject* CM, int Transparent)
{
	if (!mGIF || !CM || mFrameBuffer.Num() == 0)
	{
		return;
	}

	const int CanvasW = static_cast<int>(GetWidth());
	const int CanvasH = static_cast<int>(GetHeight());
	const int dstY = dstTop + srcY;
	if (dstY < 0 || dstY >= CanvasH)
	{
		return;
	}

	FColor* DstRow = mFrameBuffer.GetData() + dstY * CanvasW;
	const GifByteType* SrcRow = mLineBuffer.GetData();

	for (int x = 0; x < Width; ++x)
	{
		const int dstX = dstLeft + x;
		if (dstX < 0 || dstX >= CanvasW)
		{
			continue;
		}
		const int c = SrcRow[x];
		// 透明像素：跳过，保留画布既有内容（统一处理，无关 disposal）。
		if (c == Transparent)
		{
			continue;
		}
		// 颜色索引越界：跳过。
		if (c < 0 || c >= CM->ColorCount)
		{
			continue;
		}
		const GifColorType& E = CM->Colors[c];
		FColor& out = DstRow[dstX];
		out.R = E.Red;
		out.G = E.Green;
		out.B = E.Blue;
		out.A = 255;
	}
}

uint32 FGIFDecoder::NextFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	if (!mGIF || mFrames.Num() == 0)
	{
		return DefaultFrameDelay;
	}

	if (mCurrentFrame < 0 || mCurrentFrame >= mFrames.Num())
	{
		mCurrentFrame = 0;
	}

	const int RenderingFrame = mCurrentFrame;
	const FFrameMeta& Meta = mFrames[RenderingFrame];

	// Step 1: 根据"上一帧"的 disposal 清理画布上对应区域。
	DisposeFrame(mPrevDisposalMode, mPrevRect);

	// Step 2: 当前帧 disposal == PREVIOUS，则在绘制前先快照画布。
	if (Meta.Disposal == DISPOSE_PREVIOUS)
	{
		SaveSnapshot();
	}

	// Step 3: 流式解码当前帧到 canvas（skip-and-continue 容错）。
	if (!DecodeFrameAtCursor(RenderingFrame))
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("FGIFDecoder: skip frame %d (decode failed); canvas left unchanged"),
			RenderingFrame);
		// canvas 保留现状；上层会照常更新到 GPU。如果 cursor 已经跑到末尾，
		// 下一次 Tick 进来若 mCurrentFrame 跨过 mFrames.Num() 会触发 RewindToFirstRecord，
		// cursor 自动恢复。
	}

	// Step 4: 诊断 trace 日志（VeryVerbose 级别）
	if (UE_LOG_ACTIVE(LogAnimTexture, VeryVerbose))
	{
		UE_LOG(LogAnimTexture, VeryVerbose,
			TEXT("FGIFDecoder::NextFrame frame=%d prevDisp=%d curDisp=%d trans=%d rect=(%d,%d)->(%d,%d) delay=%dms loop=%d"),
			RenderingFrame, mPrevDisposalMode, Meta.Disposal, Meta.Transparent,
			Meta.Rect.Min.X, Meta.Rect.Min.Y, Meta.Rect.Max.X, Meta.Rect.Max.Y,
			Meta.DelayMs, mLoopCount);
	}

	// Step 5: 记录"当前帧"为下一帧解码时使用的 prev 状态。
	mPrevDisposalMode = Meta.Disposal;
	mPrevRect = Meta.Rect;
	mPrevTransparentColor = Meta.Transparent;

	// 推进帧索引。
	mCurrentFrame++;
	if (mCurrentFrame >= mFrames.Num())
	{
		mLoopCount++;
		if (bLooping)
		{
			// 回到起点；prev 状态保留，下一次 NextFrame 会基于上一帧的 disposal 正确清理画布。
			mCurrentFrame = 0;
			// 循环回卷：cursor 复位到 Begin、LZW 状态机全量重置。与 WebPAnimDecoderReset 等价。
			RewindToFirstRecord();
		}
		else
		{
			// 不循环：停在最后一帧。cursor 此时位于文件末尾；
			// 后续 NextFrame 调用会因为 cursor 没有可用 IMAGE_DESC 走 skip-and-continue
			// 路径——视觉上 canvas 维持在最末帧（与原 DGifSlurp 路径行为一致）。
			mCurrentFrame = mFrames.Num() - 1;
		}
	}

	return Meta.DelayMs == 0 ? DefaultFrameDelay : Meta.DelayMs;
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

	// cursor 回卷 + LZW 状态机重置。
	RewindToFirstRecord();
}

uint32 FGIFDecoder::GetWidth() const
{
	if (mGIF)
	{
		return mGIF->SWidth;
	}
	return 1;
}

uint32 FGIFDecoder::GetHeight() const
{
	if (mGIF)
	{
		return mGIF->SHeight;
	}
	return 1;
}

const FColor* FGIFDecoder::GetFrameBuffer() const
{
	return mFrameBuffer.GetData();
}

uint32 FGIFDecoder::GetDuration(uint32 DefaultFrameDelay) const
{
	int duration = 0;
	for (const FFrameMeta& M : mFrames)
	{
		duration += (M.DelayMs == 0) ? static_cast<int>(DefaultFrameDelay) : M.DelayMs;
	}
	return static_cast<uint32>(duration);
}

bool FGIFDecoder::SupportsTransparency() const
{
	return mbHasTransparency;
}

FColor FGIFDecoder::ResolveBgColorFromMetadata() const
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
	for (const FFrameMeta& M : mFrames)
	{
		if (M.Transparent == BgIdx)
		{
			return FColor(0, 0, 0, 0);
		}
	}

	const GifColorType& Entry = mGIF->SColorMap->Colors[BgIdx];
	return FColor(Entry.Red, Entry.Green, Entry.Blue, 255);
}

FIntRect FGIFDecoder::ClampRectToCanvas(int Left, int Top, int Width, int Height) const
{
	const int CanvasW = mGIF ? mGIF->SWidth : 0;
	const int CanvasH = mGIF ? mGIF->SHeight : 0;

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
		// ResolveBgColorFromMetadata 已退化为 (0,0,0,0)，与 Chrome 行为兼容。
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

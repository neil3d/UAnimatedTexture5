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

static int _GIF_InputFunc(GifFileType* gifFile, GifByteType* buffer, int length)
{
	uint8_t* fileData = (uint8_t*)(gifFile->UserData);
	memcpy(buffer, fileData, length);
	gifFile->UserData = fileData + length;
	return length;
}

bool FGIFDecoder::LoadFromMemory(const uint8* InBuffer, uint32 InBufferSize)
{
	int gifError = 0;
	mGIF = DGifOpen((void*)InBuffer, _GIF_InputFunc, &gifError);
	if (mGIF == nullptr)
	{
		FString Error(GifErrorString(gifError));
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: GIF file open failed, %s."), *Error);
		return false;
	}

	gifError = DGifSlurp(mGIF);
	if (gifError != GIF_OK)
	{
		FString Error(GifErrorString(gifError));
		UE_LOG(LogAnimTexture, Error, TEXT("FGIFDecoder: GIF file load failed, %s."), *Error);
		return false;
	}

	mFrameBuffer.SetNum(mGIF->SWidth * mGIF->SHeight);
	ClearFrameBuffer(mGIF->SColorMap, true);
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
}

uint32 FGIFDecoder::NextFrame(uint32 DefaultFrameDelay, bool bLooping)
{
	if (!mGIF) return DefaultFrameDelay;

	const SavedImage& image = mGIF->SavedImages[mCurrentFrame];
	const auto& id = image.ImageDesc;
	int frameWidth = GetWidth();
	ColorMapObject* colorMap =
		image.ImageDesc.ColorMap ? image.ImageDesc.ColorMap : mGIF->SColorMap;

	// handle GCB
	int delayTime = 0;
	int transparentColor = -1;

	for (int i = 0; i < image.ExtensionBlockCount; i++)
	{
		const ExtensionBlock& eb = image.ExtensionBlocks[i];
		if (eb.Function == GRAPHICS_EXT_FUNC_CODE)
		{
			GraphicsControlBlock gcb;
			if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) != GIF_ERROR)
			{
				delayTime = gcb.DelayTime * 10;  // 1/100 second
				if (gcb.TransparentColor != NO_TRANSPARENT_COLOR)
					transparentColor = gcb.TransparentColor;

				switch (gcb.DisposalMode)
				{
				case DISPOSAL_UNSPECIFIED:
					// No disposal specified. The decoder is not required to take any
					// action.
					break;
				case DISPOSE_DO_NOT:
					// Do not dispose. The graphic is to be left in place.
					mDoNotDispose = true;
					break;
				case DISPOSE_BACKGROUND:
					//  Restore to background color. The area used by the graphic must
					//  be restored to the background color.
					if (!mDoNotDispose)  // MY HACK!!!
						GCB_Background(id.Left, id.Top, id.Width, id.Height, colorMap,
							transparentColor != NO_TRANSPARENT_COLOR);
					break;
				case DISPOSE_PREVIOUS:
					// Restore to previous. The decoder is required to restore the area
					// overwritten by the graphic with what was there prior to rendering
					// the graphic.
					//std::cout << "DISPOSE_PREVIOUS" << std::endl;
					break;
				}// end of switch
			}
		}  // end of if
	}    // end of for

	// first frame -- draw the background
	if (mCurrentFrame == 0)
	{
		ClearFrameBuffer(mGIF->SColorMap,
			transparentColor != NO_TRANSPARENT_COLOR);
	}

	// decode current image to frame buffer
	for (int y = id.Top; y < id.Top + id.Height; y++)
	{
		for (int x = id.Left; x < id.Left + id.Width; x++)
		{
			int p = y * frameWidth + x;
			int i = (y - id.Top) * id.Width + x - id.Left;
			int c = image.RasterBits[i];
			FColor& out = mFrameBuffer[p];

			const GifColorType& colorEntry = colorMap->Colors[c];
			if (mDoNotDispose)
			{
				if (c != transparentColor)
				{
					out.R = colorEntry.Red;
					out.G = colorEntry.Green;
					out.B = colorEntry.Blue;
					out.A = 255;
				}
			}
			else
			{
				out.R = colorEntry.Red;
				out.G = colorEntry.Green;
				out.B = colorEntry.Blue;
				out.A = (c == transparentColor ? 0 : 255);
			}
		}// end of x
	}  // end of y

	// next frame
	mCurrentFrame++;
	if (mCurrentFrame >= mGIF->ImageCount) {
		mDoNotDispose = false;
		mCurrentFrame = bLooping ? 0 : mGIF->ImageCount - 1;
		mLoopCount++;
	}

	return delayTime == 0 ? DefaultFrameDelay : delayTime;
}

void FGIFDecoder::Reset()
{
	mCurrentFrame = 0;
	mLoopCount = 0;
	mDoNotDispose = false;
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
		return 0;
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
		const SavedImage& image = mGIF->SavedImages[i];
		int delayTime = 0;
		for (int j = 0; j < image.ExtensionBlockCount; j++)
		{
			const ExtensionBlock& eb = image.ExtensionBlocks[j];
			if (eb.Function == GRAPHICS_EXT_FUNC_CODE)
			{
				GraphicsControlBlock gcb;
				if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) != GIF_ERROR)
					delayTime = gcb.DelayTime * 10;  // 1/100 second
			}
		}// end of for(ext)
		duration += delayTime == 0 ? DefaultFrameDelay : delayTime;
	}

	return duration;
}

bool FGIFDecoder::SupportsTransparency() const
{
	if (!mGIF)
		return false;

	for (int i = 0; i < mGIF->ImageCount; i++)
	{
		const SavedImage& image = mGIF->SavedImages[i];
		for (int j = 0; j < image.ExtensionBlockCount; j++)
		{
			const ExtensionBlock& eb = image.ExtensionBlocks[j];
			if (eb.Function == GRAPHICS_EXT_FUNC_CODE)
			{
				GraphicsControlBlock gcb;
				if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) != GIF_ERROR)
				{
					if (gcb.TransparentColor != NO_TRANSPARENT_COLOR)
						return true;
				}
			}
		}// end of for(ext)
	}
	return false;
}

void FGIFDecoder::ClearFrameBuffer(ColorMapObject* ColorMap,
	bool bTransparent) 
{
	FColor bg = { 0, 0, 0, 255 };

	if (ColorMap && mGIF->SBackGroundColor >= 0 &&
		mGIF->SBackGroundColor < ColorMap->ColorCount) 
	{
		const GifColorType& colorEntry =
			mGIF->SColorMap->Colors[mGIF->SBackGroundColor];
		uint8_t alpha = bTransparent ? 0 : 255;
		bg = { colorEntry.Red, colorEntry.Green, colorEntry.Blue, alpha };
	}

	for (auto& pixel : mFrameBuffer) pixel = bg;
}

void FGIFDecoder::GCB_Background(int left, int top, int width, int height,
	ColorMapObject* colorMap,
	bool bTransparent)
{
	const GifColorType& colorEntry = colorMap->Colors[mGIF->SBackGroundColor];
	uint8_t alpha = static_cast<uint8_t>(bTransparent ? 0 : 255);
	FColor bg = { colorEntry.Red, colorEntry.Green, colorEntry.Blue, alpha };

	int frameWidth = GetWidth();
	for (int y = top; y < top + height; y++)
	{
		for (int x = left; x < left + width; x++)
		{
			int p = y * frameWidth + x;
			mFrameBuffer[p] = bg;
		}
	}  // end of y
}

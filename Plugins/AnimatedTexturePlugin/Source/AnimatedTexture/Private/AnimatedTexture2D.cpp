// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimatedTexture2D.h"

float UAnimatedTexture2D::GetSurfaceWidth() const
{
	return 0;
}

float UAnimatedTexture2D::GetSurfaceHeight() const
{
	return 0;
}

FTextureResource* UAnimatedTexture2D::CreateResource()
{
	return nullptr;
}

void UAnimatedTexture2D::ImportFile(EAnimatedTextureType InFileType, const uint8* InBuffer, uint32 InBufferSize)
{
	FileType = InFileType;
	FileBlob = TArray<uint8>(InBuffer, InBufferSize);
}
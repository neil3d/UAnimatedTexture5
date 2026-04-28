/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Runtime Load function library implementation.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AnimatedTextureFunctionLibrary.h"
#include "AnimatedTextureModule.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"

bool UAnimatedTextureFunctionLibrary::InitAnimatedTextureFromMemory(
	UAnimatedTexture2D* Target, const uint8* Buffer, int32 BufferSize, EAnimatedTextureType Type)
{
	if (!Target)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("InitAnimatedTextureFromMemory: Target is null."));
		return false;
	}
	if (!Buffer || BufferSize <= 0)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("InitAnimatedTextureFromMemory: Buffer is null or empty."));
		return false;
	}

	// 类型决定：若调用方未指定，则用 magic bytes 嗅探。
	EAnimatedTextureType ResolvedType = Type;
	if (ResolvedType == EAnimatedTextureType::None)
	{
		ResolvedType = UAnimatedTexture2D::DetectTypeFromMagic(Buffer, BufferSize);
	}

	if (ResolvedType == EAnimatedTextureType::None)
	{
		UE_LOG(LogAnimTexture, Error,
			TEXT("InitAnimatedTextureFromMemory: Cannot detect animated texture type from bytes (size=%d)."),
			BufferSize);
		return false;
	}

	Target->ImportFile(ResolvedType, Buffer, static_cast<uint32>(BufferSize));
	return true;
}

UAnimatedTexture2D* UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromMemory(
	const TArray<uint8>& Bytes, EAnimatedTextureType Type, UObject* Outer, FName Name)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogAnimTexture, Error,
			TEXT("LoadAnimatedTextureFromMemory must be called on the GameThread."));
		return nullptr;
	}

	if (Bytes.Num() <= 0)
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("LoadAnimatedTextureFromMemory: Bytes is empty."));
		return nullptr;
	}

	UObject* UseOuter = Outer ? Outer : (UObject*)GetTransientPackage();

	UAnimatedTexture2D* NewTexture = NewObject<UAnimatedTexture2D>(UseOuter, Name);
	if (!NewTexture)
	{
		UE_LOG(LogAnimTexture, Error, TEXT("LoadAnimatedTextureFromMemory: NewObject failed."));
		return nullptr;
	}

	const bool bOk = InitAnimatedTextureFromMemory(NewTexture, Bytes.GetData(), Bytes.Num(), Type);
	if (!bOk)
	{
		// 让刚创建的对象在本帧末被 GC 回收（没有任何强引用）。
		return nullptr;
	}

	// 触发 RHI 资源创建。CreateResource() 会据 FileType + FileBlob 构造 Decoder 并初始化 RHI。
	NewTexture->UpdateResource();

	// 若 CreateResource 内部返回 nullptr（例如 Decoder 解码失败），则 RHI Resource 不存在，
	// 我们视为整体加载失败并返回 nullptr。
	if (NewTexture->GetResource() == nullptr)
	{
		UE_LOG(LogAnimTexture, Error,
			TEXT("LoadAnimatedTextureFromMemory: UpdateResource produced no FTextureResource (decode failed)."));
		return nullptr;
	}

	return NewTexture;
}

UAnimatedTexture2D* UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromFile(
	const FString& FilePath, EAnimatedTextureLoadError& OutError)
{
	OutError = EAnimatedTextureLoadError::None;

	if (!IsInGameThread())
	{
		UE_LOG(LogAnimTexture, Error,
			TEXT("LoadAnimatedTextureFromFile must be called on the GameThread."));
		OutError = EAnimatedTextureLoadError::DecodeFailed;
		return nullptr;
	}

	if (FilePath.IsEmpty())
	{
		UE_LOG(LogAnimTexture, Warning, TEXT("LoadAnimatedTextureFromFile: FilePath is empty."));
		OutError = EAnimatedTextureLoadError::FileNotFound;
		return nullptr;
	}

	// 相对路径基于 FPaths::ProjectDir() 解析；绝对路径原样使用。
	FString ResolvedPath = FilePath;
	if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), ResolvedPath);
	}
	FPaths::NormalizeFilename(ResolvedPath);

	if (IFileManager::Get().FileSize(*ResolvedPath) <= 0)
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("LoadAnimatedTextureFromFile: file not found or empty: %s"), *ResolvedPath);
		OutError = EAnimatedTextureLoadError::FileNotFound;
		return nullptr;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *ResolvedPath))
	{
		UE_LOG(LogAnimTexture, Warning,
			TEXT("LoadAnimatedTextureFromFile: LoadFileToArray failed for %s"), *ResolvedPath);
		OutError = EAnimatedTextureLoadError::FileNotFound;
		return nullptr;
	}

	if (Bytes.Num() <= 0)
	{
		OutError = EAnimatedTextureLoadError::EmptyBody;
		return nullptr;
	}

	// 优先用扩展名推断类型；若扩展名无法识别，交给 LoadAnimatedTextureFromMemory 内部按 magic bytes 嗅探。
	const EAnimatedTextureType TypeByExt = UAnimatedTexture2D::DetectTypeFromExtension(ResolvedPath);

	UAnimatedTexture2D* Texture = LoadAnimatedTextureFromMemory(Bytes, TypeByExt, /*Outer=*/ nullptr, NAME_None);
	if (!Texture)
	{
		OutError = EAnimatedTextureLoadError::DecodeFailed;
		return nullptr;
	}

	return Texture;
}

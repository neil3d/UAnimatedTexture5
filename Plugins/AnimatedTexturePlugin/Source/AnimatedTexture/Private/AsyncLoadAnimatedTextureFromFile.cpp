/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Async blueprint node for loading UAnimatedTexture2D from local file.
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
*/

#include "AsyncLoadAnimatedTextureFromFile.h"
#include "AnimatedTextureFunctionLibrary.h"
#include "AnimatedTexture2D.h"
#include "AnimatedTextureModule.h"

#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

UAsyncLoadAnimatedTextureFromFile* UAsyncLoadAnimatedTextureFromFile::LoadAnimatedTextureFromFileAsync(
	UObject* WorldContextObject, const FString& FilePath)
{
	UAsyncLoadAnimatedTextureFromFile* Action = NewObject<UAsyncLoadAnimatedTextureFromFile>();
	Action->WorldContextWeak = WorldContextObject;
	Action->FilePathCached = FilePath;
	return Action;
}

void UAsyncLoadAnimatedTextureFromFile::Activate()
{
	// 使节点对象在异步期间不被 GC 回收；结束时 RemoveFromRoot。
	AddToRoot();

	TWeakObjectPtr<UAsyncLoadAnimatedTextureFromFile> WeakThis(this);
	const FString Path = FilePathCached;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, Path]()
	{
		// 后台线程：仅做路径解析与文件 IO，读出原始字节。
		FString ResolvedPath = Path;
		if (FPaths::IsRelative(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), ResolvedPath);
		}
		FPaths::NormalizeFilename(ResolvedPath);

		EAnimatedTextureLoadError Error = EAnimatedTextureLoadError::None;
		TArray<uint8> Bytes;

		if (ResolvedPath.IsEmpty() || IFileManager::Get().FileSize(*ResolvedPath) <= 0)
		{
			Error = EAnimatedTextureLoadError::FileNotFound;
		}
		else if (!FFileHelper::LoadFileToArray(Bytes, *ResolvedPath))
		{
			Error = EAnimatedTextureLoadError::FileNotFound;
		}
		else if (Bytes.Num() <= 0)
		{
			Error = EAnimatedTextureLoadError::EmptyBody;
		}

		// 先在后台线程根据扩展名推断类型（纯字符串运算，不涉及 UObject）。
		const EAnimatedTextureType TypeByExt =
			UAnimatedTexture2D::DetectTypeFromExtension(ResolvedPath);

		// 回到 GameThread：构造 UAnimatedTexture2D 并广播结果。
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, Error, Bytes = MoveTemp(Bytes), TypeByExt]() mutable
		{
			UAsyncLoadAnimatedTextureFromFile* Self = WeakThis.Get();
			if (!Self)
			{
				// 节点对象已被 GC（比如 WorldContext 提前结束）；静默退出。
				return;
			}

			// 保护 WorldContext 生命周期：一旦已失效则静默退出。
			if (Self->WorldContextWeak.IsStale())
			{
				Self->RemoveFromRoot();
				return;
			}

			if (Error != EAnimatedTextureLoadError::None)
			{
				Self->OnFailed.Broadcast(nullptr, Error);
				Self->RemoveFromRoot();
				return;
			}

			UAnimatedTexture2D* Texture = UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromMemory(
				Bytes, TypeByExt, /*Outer=*/ nullptr, NAME_None);
			if (!Texture)
			{
				Self->OnFailed.Broadcast(nullptr, EAnimatedTextureLoadError::DecodeFailed);
				Self->RemoveFromRoot();
				return;
			}

			Self->OnSuccess.Broadcast(Texture, EAnimatedTextureLoadError::None);
			Self->RemoveFromRoot();
		});
	});
}

/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AnimatedTextureFactory.h"
#include "AnimatedTextureEditorModule.h"
#include "AnimatedTexture2D.h"
#include "AnimatedTextureFunctionLibrary.h"
#include "TextureReferenceResolver.h"

#include "Subsystems/ImportSubsystem.h"	// UnrealEd
#include "EditorFramework/AssetImportData.h"	// Engine
#include "Editor.h"	// UnrealEd

UAnimatedTextureFactory::UAnimatedTextureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bEditAfterNew = true;
	SupportedClass = UAnimatedTexture2D::StaticClass();
	Formats.Add(TEXT("gif;GIF(Animation Supported)"));
	Formats.Add(TEXT("webp;Webp(Animation Supported)"));

	// Required to be checked before UReimportTextureFactory
	ImportPriority = DefaultImportPriority + 1;
}

bool UAnimatedTextureFactory::FactoryCanImport(const FString& Filename)
{
	FString Extension = FPaths::GetExtension(Filename, true);

	return Extension.Compare(TEXT(".gif"), ESearchCase::IgnoreCase) == 0
		|| Extension.Compare(TEXT(".webp"), ESearchCase::IgnoreCase) == 0;
}

UObject* UAnimatedTextureFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	UE_LOG(LogAnimTextureEditor, Log, TEXT("UAnimatedTextureFactory: Importing %s ..."), *(InName.ToString()));

	check(Type);
	check(InClass == UAnimatedTexture2D::StaticClass());

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

	// if the texture already exists, remember the user settings
	UAnimatedTexture2D* ExistingTexture = FindObject<UAnimatedTexture2D>(InParent, *InName.ToString());
	if (ExistingTexture) {
		// 注：首次导入覆盖已有同名资产时，目前不保留旧资产的 UPROPERTY —— 仅 Reimport()
		// 路径显式暂存/还原一份。如果将来要把两条路径合并，建议改成反射方式
		// （UEngine::CopyPropertiesForUnrelatedObjects 或 FObjectWriter/FObjectReader），
		// 避免每加一个 UPROPERTY 都要在两处同步手抄。
	}

	// FTextureReferenceReplacer 复制旧 UTexture::TextureReference 给新对象，让已有材质 /
	// Slate Brush / 蓝图引用在覆盖式导入后继续指向有效 RHI。这条路径对 UAnimatedTexture2D
	// （继承自 UTexture，非 UTexture2D）同样成立 —— 我们走的就是 UTexture::TextureReference
	// 通道。设计背景见 Docs/BaseClassChoice.md §6。
	FTextureReferenceReplacer RefReplacer(ExistingTexture);

	// call parent method to create/overwrite anim texture object
	UAnimatedTexture2D* AnimTexture = Cast<UAnimatedTexture2D>(
		CreateOrOverwriteAsset(InClass, InParent, InName, Flags)
		);
	if (AnimTexture == nullptr) {
		UE_LOG(LogAnimTextureEditor, Error, TEXT("Create Animated Texture FAILED, Name=%s."), *(InName.ToString()));
		return nullptr;
	}

	// 解析字节流并把内容填充到 AnimTexture；类型判断与 ImportFile 调用
	// 都通过 Runtime 模块提供的 UAnimatedTextureFunctionLibrary 完成，保持 Runtime/Editor 共用一处真源。
	// 先基于 Factory 已知的 Type 字符串（来自扩展名注册）得到枚举，解析不出再回落到 magic bytes 嗅探。
	const EAnimatedTextureType AnimTextureType = UAnimatedTexture2D::DetectTypeFromExtension(FString(Type));

	const bool bInitOk = UAnimatedTextureFunctionLibrary::InitAnimatedTextureFromMemory(
		AnimTexture, Buffer, static_cast<int32>(BufferEnd - Buffer), AnimTextureType);
	if (!bInitOk)
	{
		UE_LOG(LogAnimTextureEditor, Error,
			TEXT("UAnimatedTextureFactory: failed to init %s from buffer (type=%s)."),
			*(InName.ToString()), Type);

		// 清理 CreateOrOverwriteAsset 已经创建出来的半成品对象，避免它以 RF_Standalone | RF_Public
		// 形式残留到下次 GC，干扰 Content Browser 与 Reference Viewer。对照 UTextureFactory 的失败路径。
		AnimTexture->ClearFlags(RF_Standalone | RF_Public);
		AnimTexture->MarkAsGarbage();
		return nullptr;
	}

	//Replace the reference for the new texture with the existing one so that all current users still have valid references.
	RefReplacer.Replace(AnimTexture);

	// 写回 AssetImportData，让后续 CanReimport / Reimport 能拿到首次导入的源文件路径。
	// 引擎 UTextureFactory::FactoryCreateBinary 的等价做法。UE 5.3 中 UFactory::CurrentFilename
	// 是 public static FString，由 UFactory::ImportObject 在调用前设置好。
	if (AnimTexture->AssetImportData)
	{
		AnimTexture->AssetImportData->Update(UFactory::CurrentFilename);
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, AnimTexture);

	// Invalidate any materials using the newly imported texture. (occurs if you import over an existing texture)
	AnimTexture->PostEditChange();

	return AnimTexture;
}

bool UAnimatedTextureFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UAnimatedTexture2D* pTex = Cast<UAnimatedTexture2D>(Obj);
	if (pTex) {
		pTex->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UAnimatedTextureFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UAnimatedTexture2D* pTex = Cast<UAnimatedTexture2D>(Obj);
	if (pTex && ensure(NewReimportPaths.Num() == 1))
	{
		pTex->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UAnimatedTextureFactory::Reimport(UObject* Obj)
{
	if (!Obj || !Obj->IsA(UAnimatedTexture2D::StaticClass()))
	{
		return EReimportResult::Failed;
	}

	UAnimatedTexture2D* pTex = Cast<UAnimatedTexture2D>(Obj);

	const FString ResolvedSourceFilePath = pTex->AssetImportData->GetFirstFilename();
	if (!ResolvedSourceFilePath.Len())
	{
		return EReimportResult::Failed;
	}

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*ResolvedSourceFilePath) == INDEX_NONE)
	{
		UE_LOG(LogAnimTextureEditor, Warning, TEXT("cannot reimport: [%s] file cannot be found."), *ResolvedSourceFilePath);
		return EReimportResult::Failed;
	}

	// 暂存用户在 Details 里调过的所有 UPROPERTY，避免被 CreateOrOverwriteAsset 重置。
	// 注意：每新增一个 EditAnywhere 的 UPROPERTY 都要在此处同步加入；自动化方案（反射）
	// 留给后续 PR 评估，目前保持手抄风格以便阅读 diff。
	enum TextureAddress AddressX = pTex->AddressX;
	enum TextureAddress AddressY = pTex->AddressY;
	float PlayRate = pTex->PlayRate;
	bool SupportsTransparency = pTex->SupportsTransparency;
	float DefaultFrameDelay = pTex->DefaultFrameDelay;
	bool bLooping = pTex->bLooping;
	bool bRespectFileLoopCount = pTex->bRespectFileLoopCount;
	bool bUseMultithreadedDecode = pTex->bUseMultithreadedDecode;
	bool bPremultipliedAlpha = pTex->bPremultipliedAlpha;

	bool OutCanceled = false;
	if (ImportObject(pTex->GetClass(), pTex->GetOuter(), *pTex->GetName(), RF_Public | RF_Standalone, ResolvedSourceFilePath, nullptr, OutCanceled) != nullptr)
	{
		pTex->AssetImportData->Update(ResolvedSourceFilePath);

		pTex->AddressX = AddressX;
		pTex->AddressY = AddressY;
		pTex->PlayRate = PlayRate;
		pTex->SupportsTransparency = SupportsTransparency;
		pTex->DefaultFrameDelay = DefaultFrameDelay;
		pTex->bLooping = bLooping;
		pTex->bRespectFileLoopCount = bRespectFileLoopCount;
		pTex->bUseMultithreadedDecode = bUseMultithreadedDecode;
		pTex->bPremultipliedAlpha = bPremultipliedAlpha;

		// Try to find the outer package so we can dirty it up
		if (pTex->GetOuter())
		{
			pTex->GetOuter()->MarkPackageDirty();
		}
		else
		{
			pTex->MarkPackageDirty();
		}
	}
	else if (OutCanceled)
	{
		UE_LOG(LogAnimTextureEditor, Warning, TEXT("[%s] reimport canceled"), *(Obj->GetName()));
		return EReimportResult::Cancelled;
	}
	else
	{
		UE_LOG(LogAnimTextureEditor, Warning, TEXT("[%s] reimport failed"), *(Obj->GetName()));
		return EReimportResult::Failed;
	}

	return EReimportResult::Succeeded;
}

int32 UAnimatedTextureFactory::GetPriority() const
{
	return ImportPriority;
}

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

	// Required to checkbefore UReimportTextureFactory
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
		// TODO: use existing object settings
	}

	FTextureReferenceReplacer RefReplacer(ExistingTexture);

	// call parent method to create/overwrite anim texture object
	UAnimatedTexture2D* AnimTexture = Cast<UAnimatedTexture2D>(
		CreateOrOverwriteAsset(InClass, InParent, InName, Flags)
		);
	if (AnimTexture == nullptr) {
		UE_LOG(LogAnimTextureEditor, Error, TEXT("Create Animated Texture FAILED, Name=%s."), *(InName.ToString()));
		return nullptr;
	}

	// just copy file blob to AnimTexture object
	EAnimatedTextureType AnimTextureType = EAnimatedTextureType::None;
	FString FileType(Type);
	if (FileType.Compare(TEXT("gif"), ESearchCase::IgnoreCase) == 0)
		AnimTextureType = EAnimatedTextureType::Gif;
	else if (FileType.Compare(TEXT("webp"), ESearchCase::IgnoreCase) == 0)
		AnimTextureType = EAnimatedTextureType::Webp;

	AnimTexture->ImportFile(AnimTextureType, Buffer, BufferEnd - Buffer);

	//Replace the reference for the new texture with the existing one so that all current users still have valid references.
	RefReplacer.Replace(AnimTexture);

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
		UE_LOG(LogAnimTextureEditor, Warning, TEXT("cannot reimport: [] file cannot be found."), *ResolvedSourceFilePath);
		return EReimportResult::Failed;
	}

	enum TextureAddress AddressX = pTex->AddressX;
	enum TextureAddress AddressY = pTex->AddressY;
	float PlayRate = pTex->PlayRate;
	bool SupportsTransparency = pTex->SupportsTransparency;
	float DefaultFrameDelay = pTex->DefaultFrameDelay;
	bool bLooping = pTex->bLooping;

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

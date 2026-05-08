/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AnimatedTextureEditorModule.h"
#include "AnimatedTextureThumbnailRenderer.h"
#include "AnimatedTextureStyle.h"
#include "AnimatedTexture2D.h"
#include "Misc/CoreDelegates.h"	// Core
#include "HAL/PlatformProcess.h"	// Core
#include "EditorFramework/AssetImportData.h"	// Engine
#include "ThumbnailRendering/ThumbnailManager.h"	// UnrealEd
#include "ToolMenus.h"	// ToolMenus
#include "ContentBrowserMenuContexts.h"	// ContentBrowser

#define LOCTEXT_NAMESPACE "FAnimatedTextureEditorModule"

void FAnimatedTextureEditorModule::StartupModule()
{
	// 注册 ClassIcon / ClassThumbnail 资源；Slate 启动后必须 ASAP 注册，让 Content Browser
	// 第一次构建条目时就能拿到自定义图标。
	FAnimatedTextureStyle::Initialize();

	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FAnimatedTextureEditorModule::OnPostEngineInit);
}

void FAnimatedTextureEditorModule::OnPostEngineInit()
{
	UThumbnailManager::Get().RegisterCustomRenderer(UAnimatedTexture2D::StaticClass(), UAnimatedTextureThumbnailRenderer::StaticClass());

	RegisterAssetContextMenu();
}

void FAnimatedTextureEditorModule::RegisterAssetContextMenu()
{
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	// 用单独的 Owner 名称便于 ShutdownModule 一次性反注册所有挂上去的菜单项，
	// 防止热重载留下指向已析构 lambda 闭包的菜单项。
	AssetContextMenuOwnerName = FName(TEXT("AnimatedTextureEditorModule.AssetContextMenu"));
	FToolMenuOwnerScoped OwnerScope(AssetContextMenuOwnerName);

	// ContentBrowser 对每个资产类按"ContentBrowser.AssetContextMenu.<ClassFName>"自动派生菜单 ——
	// ExtendMenu 在不存在时会创建，故对未先注册的类型同样有效。
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu.AnimatedTexture2D"));
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
	Section.AddDynamicEntry(TEXT("AnimatedTextureActions"),
		FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context =
				InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context)
			{
				return;
			}

			// "Reveal Source File"：在系统文件管理器里高亮源 GIF/WebP 文件
			InSection.AddMenuEntry(
				TEXT("RevealSourceFile"),
				LOCTEXT("RevealSourceFile", "Reveal Source File"),
				LOCTEXT("RevealSourceFileTooltip", "Open the source GIF/WebP file in the OS file explorer."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Context]()
					{
						// 直接访问 UPROPERTY 字段：UE 5.3 上 GetSelectedAssets() 还未稳定到位
						for (const FAssetData& AssetData : Context->SelectedAssets)
						{
							UAnimatedTexture2D* Tex = Cast<UAnimatedTexture2D>(AssetData.GetAsset());
							if (Tex && Tex->AssetImportData)
							{
								const FString File = Tex->AssetImportData->GetFirstFilename();
								if (!File.IsEmpty())
								{
									FPlatformProcess::ExploreFolder(*File);
								}
							}
						}
					})));

			// "Open in External Player"：用系统默认程序打开源文件（例如浏览器 / 系统图片查看器）
			InSection.AddMenuEntry(
				TEXT("OpenInExternalPlayer"),
				LOCTEXT("OpenInExternalPlayer", "Open in External Player"),
				LOCTEXT("OpenInExternalPlayerTooltip", "Open the source GIF/WebP file in the OS default external application."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Context]()
					{
						for (const FAssetData& AssetData : Context->SelectedAssets)
						{
							UAnimatedTexture2D* Tex = Cast<UAnimatedTexture2D>(AssetData.GetAsset());
							if (Tex && Tex->AssetImportData)
							{
								const FString File = Tex->AssetImportData->GetFirstFilename();
								if (!File.IsEmpty())
								{
									FPlatformProcess::LaunchFileInDefaultExternalApplication(*File);
								}
							}
						}
					})));
		}));
}

void FAnimatedTextureEditorModule::ShutdownModule()
{
	// 反注册委托，避免热重载/插件 Reload 时回调指向已析构的 this。
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	// 反注册所有以本模块 OwnerName 添加的 ToolMenu 项（含动态 entry / section）。
	if (UObjectInitialized() && !AssetContextMenuOwnerName.IsNone())
	{
		if (UToolMenus* ToolMenus = UToolMenus::Get())
		{
			ToolMenus->UnregisterOwnerByName(AssetContextMenuOwnerName);
		}
		AssetContextMenuOwnerName = NAME_None;
	}

	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UAnimatedTexture2D::StaticClass());
	}

	FAnimatedTextureStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogAnimTextureEditor);
IMPLEMENT_MODULE(FAnimatedTextureEditorModule, AnimatedTextureEditor)
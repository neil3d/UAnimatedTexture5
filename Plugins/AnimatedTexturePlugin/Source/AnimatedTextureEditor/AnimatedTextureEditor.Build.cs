// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimatedTextureEditor : ModuleRules
{
    public AnimatedTextureEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
				// ... add private dependencies that you statically link with here ...
                // RHI / RenderCore 看似没有被本模块直接 #include，但 FTextureReferenceReplacer
                // (TextureReferenceResolver.h) 内联析构里会触发 TRefCountPtr<FRHITextureReference>::~TRefCountPtr，
                // 进而调用 FRHIResource::Destroy；不加这两个依赖会出 LNK2019。
                "RHI",
                "RenderCore",
                "UnrealEd",
                "AnimatedTexture",
                // UAssetDefinition_AnimatedTexture2D 走 UE5.1+ 的 AssetDefinition 体系
                "AssetDefinition",
                // 右键菜单注册（ContentBrowser.AssetContextMenu.AnimatedTexture2D）
                "ToolMenus",
                "ContentBrowser",
                // FSlateIcon / FAnimatedTextureStyle
                "Slate",
                "SlateCore",
                // FAnimatedTextureStyle 通过 IPluginManager 解析 Resources/ 路径
                "Projects",
                // OpenAssets 复用引擎纹理编辑器（含预览面板）
                "TextureEditor",
            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );
    }
}

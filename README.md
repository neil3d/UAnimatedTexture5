# Animated Texture Plugin for Unreal Engine 5

----
This plugin allows you to import animated pictures into your Unreal Engine 5 project as a new AnimatedTexture asset type.
- [x] Support .GIF files
- [x] Support .Webp files

![DEMO](./Docs/images/demo.png)

## Features

<<<<<<< Updated upstream
- [x] Import Animated GIF/Webp as a Texture, supports animation, transparency, interlace, etc
- [x] Editing in default Texture Editor
- [x] Supports UMG Image widget, Material and Material Instance
- [x] Animation playback APIs
- [ ] Runtime load GIF/Webp file from disk or download from web
=======
- Import animated GIF / WebP files as Texture assets, with full support for animation, transparency, interlacing, and more
- Editable through the built-in Texture Editor
- Works with UMG Image widgets, Materials, and Material Instances
- Blueprint-accessible playback API — Play, Stop, SetPlayRate, SetLooping, etc.
- **Runtime Load** — create `UAnimatedTexture2D` at runtime from a local file or an HTTP(S) URL, usable directly in UMG / Materials.
>>>>>>> Stashed changes

## Compatibility

The plugin should work on all platforms the Unreal Engine 5 supports, but only been tested on the following platform:
- [x] Windows 64
- [x] MacOS

## Screenshots

![Material DEMO](./Docs/images/mtl.png)

![UMG DEMO](./Docs/images/umg.png)

![Playback API DEMO](./Docs/images/api.png)

<<<<<<< Updated upstream
=======
## Runtime Load

The plugin can build `UAnimatedTexture2D` objects at runtime, without going through the editor asset pipeline. Two sources are supported: the local file system and HTTP(S) URLs.

### Module Dependencies

In your game module's `*.Build.cs`, add the `AnimatedTexture` module (and `HTTP` if you use the download API directly from C++):

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "AnimatedTexture", "HTTP" });
```

### C++ Example — Load From Local File (Sync)

```cpp
#include "AnimatedTextureFunctionLibrary.h"
#include "AnimatedTextureLoadTypes.h"
#include "AnimatedTexture2D.h"

void UMyUserWidget::LoadLocalGif()
{
    // Relative paths are resolved against FPaths::ProjectDir().
    const FString Path = TEXT("Content/Runtime/sample.gif");

    EAnimatedTextureLoadError Error = EAnimatedTextureLoadError::None;
    UAnimatedTexture2D* Texture =
        UAnimatedTextureFunctionLibrary::LoadAnimatedTextureFromFile(Path, Error);

    if (Texture)
    {
        // Assign to UMG Image's Brush.ResourceObject, or use as Texture parameter in a Material Instance.
        MyImage->SetBrushFromTexture(Cast<UTexture2D>(Texture), /*bMatchSize=*/ true);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load gif: %d"), (int32)Error);
    }
}
```

### Blueprint Nodes

- **Load Animated Texture From File Async** — reads file bytes on a background thread, then constructs the object on the GameThread. Two output pins: `On Success (Texture)` and `On Failed (Error)`.
- **Download Animated Texture** — downloads a `.gif` / `.webp` from an HTTP(S) URL. Inputs: `Url`, `Timeout` (default 30s), `Max Bytes` (default 32 MiB). Output pins: `On Success (Texture, Url, ETag)`, `On Canceled`, `On Failed (Error)`.
- **Cancel** — a Blueprint-callable method on the download node; will trigger `On Canceled` exactly once.
- **Load Animated Texture From Memory** — synchronous helper accepting a byte array (e.g. bytes read from Pak or a custom network protocol).

### Error Codes

`EAnimatedTextureLoadError`: `None`, `HttpFailed`, `HttpBadStatus`, `EmptyBody`, `DecodeFailed`, `FileNotFound`, `InvalidFormat`, `SizeLimitExceeded`, `InvalidUrl`.

### Notes

- `UObject` construction and RHI resource creation always happen on the GameThread.
- The returned `UAnimatedTexture2D` is created under `GetTransientPackage()` by default; keep a hard reference (e.g. a `UPROPERTY()` on your owning widget/actor) to prevent it from being garbage-collected.
- HTTP download only accepts `http://` and `https://` URLs; other schemes fail immediately with `InvalidUrl`.
- Runtime load reuses the exact same initialization path (`UAnimatedTextureFunctionLibrary::InitAnimatedTextureFromMemory`) as the editor import factory, so behavior stays consistent.

### Self-test Checklist (maintainer memo)

- [ ] Drag & drop `.gif` / `.webp` into Content Browser — import still works.
- [ ] Asset Reimport — still works.
- [ ] Thumbnail renders correctly.
- [ ] `LoadAnimatedTextureFromFile` with absolute path works.
- [ ] `LoadAnimatedTextureFromFile` with `ProjectDir`-relative path works.
- [ ] HTTP download: 200 OK → `OnSuccess`.
- [ ] HTTP download: 404 → `OnFailed(HttpBadStatus)`.
- [ ] HTTP download: timeout → `OnFailed(HttpFailed)`.
- [ ] HTTP download: `Cancel()` → exactly one `OnCanceled`; no `OnSuccess`/`OnFailed` afterwards.
- [ ] PIE end with pending downloads — no crash; requests canceled cleanly.

## License
>>>>>>> Stashed changes


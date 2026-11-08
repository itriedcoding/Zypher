# Roadmap and integration status

Zypher 0.1.0 ships a verified core. This page is the honest status of every
technology in the project brief, so nothing looks implemented that is not.

## Status legend

- **Implemented** — code exists, is built here, and is covered by tests.
- **Source only** — assets/sources are in the repository but are not compiled in
  the default build.
- **Planned** — the CMake option exists and fails fast; there is no implementation
  in 0.1.0.

## Languages

| Technology | Status | Where |
| --- | --- | --- |
| C++23 | Implemented | `engine/` (core, ECS, assets, scene, physics, nav, render, runtime) |
| C# | Implemented | `engine/scripting/csharp`, `samples/demo/scripts`, tested by `tools/Zypher.Scripting.Tests` |
| Rust | Implemented | `tools/assetcooker` (25 tests) |
| TypeScript | Implemented | `editor/ts` (strict typecheck, 18 tests) |
| QML | Source only | `editor/qml`; builds when Qt 6.5+ is installed |
| GLSL | Source only | `engine/shaders/glsl` |
| HLSL | Source only | `engine/shaders/hlsl` |
| MSL | Source only | `engine/shaders/msl` |
| Slang | Source only | `engine/shaders/slang` |
| Python | Implemented | `tools/scripts` (validation, packaging, verification) |
| Lua | Planned | embedded host not written |
| Objective-C++ | Implemented (macOS only) | `engine/src/platform/PlatformMac.mm`; needs a macOS machine to build |
| Swift | Planned | native Apple helpers not written |

## Graphics APIs

| Technology | Status | Notes |
| --- | --- | --- |
| Software rasteriser | Implemented | deterministic CI rendering, PNG output |
| Vulkan | Planned | `RenderDevice` seam and SPIR-V shaders are ready |
| DirectX 12 | Planned | HLSL sources ready |
| Metal | Planned | MSL sources ready |
| MoltenVK | Planned | follows the Vulkan backend |
| OpenGL | Planned | legacy path |
| SPIR-V | Source only | GLSL sources build with `glslangValidator` |
| Slang | Source only | cross-target source in `engine/shaders/slang` |
| DXC | Source only | HLSL compile commands documented in `docs/building.md` |

## Engine libraries

| Library | Status | Notes |
| --- | --- | --- |
| SDL3 | Planned | dependency-free platform layer used instead |
| EnTT | N/A | Zypher ships its own sparse-set ECS (no dependency) |
| Jolt Physics | Planned | internal AABB solver used instead |
| Box2D | Planned | internal solver used instead |
| Recast Navigation | Planned | internal A* nav grid used instead |
| miniaudio | Planned | `NullAudioDevice` used instead |
| Assimp | Planned | OBJ/glTF/GLB importers are native in the cooker |
| meshoptimizer | Planned | `MeshData::validate` only |
| Dear ImGui | Planned | editor uses Qt/QML; ImGui overlay pending |
| FreeType / HarfBuzz | Planned | font bytes can be packed as raw assets today |
| Tracy | Planned | internal `Profiler` timings used |
| Crashpad | Planned | in-process crash reporter writes `artifacts/crashes` |

## Editor

| Technology | Status | Notes |
| --- | --- | --- |
| Qt 6 | Source only | desktop shell, needs Qt to build |
| QML | Source only | panels in `editor/qml` |
| C++ editor backend | Implemented | `editor/core`, tested headlessly |
| Monaco | Planned | embed via Qt WebEngine/QML WebView |
| TypeScript panels | Implemented | protocol + logic, tested |

## Asset formats

| Format | Status |
| --- | --- |
| GLTF / GLB | Cooker support (JSON glTF validated; accessor import is next) |
| FBX | Planned (requires Assimp) |
| OBJ | Implemented |
| PNG / JPEG / WebP / EXR / KTX2 | Validated and packed (codec upload next) |
| WAV / OGG / FLAC | Validated and packed |
| JSON | Implemented (parser, scenes, projects) |
| SQLite | Planned for the asset database |
| Protobuf | Planned for IPC between editor and runtime |

## Next milestones

1. **GPU renderer**: Vulkan backend first (works on Windows/macOS via MoltenVK),
   then D3D12 and native Metal. Shaders compile in CI with glslang/DXC/Slang.
2. **Windowing/input**: SDL3 layer behind the existing `Platform` interface.
3. **Physics**: Jolt behind `PhysicsWorld`, Box2D for 2D.
4. **Content**: Assimp import (FBX), meshoptimizer, KTX2/Basis texture upload,
   miniaudio playback, FreeType/HarfBuzz text.
5. **Editor**: GPU viewport, Monaco integration, asset database (SQLite).
6. **Diagnostics**: Tracy and Crashpad as optional, behind existing seams.

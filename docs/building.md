# Building Zypher

## Prerequisites

| Tool | Version | Used by |
| --- | --- | --- |
| CMake | 3.24+ | everything |
| Ninja | any recent | recommended generator |
| C++ compiler | Apple Clang 16+ / MSVC 19.38+ / GCC 13+ | engine, runtime, editor core |
| Rust | stable | asset cooker |
| Node.js | 20+ | editor TypeScript panels |
| .NET SDK | 8.0+ | C# gameplay scripting and its tests |
| Qt | 6.5+ | optional desktop editor shell |
| Python | 3.10+ | validation, packaging, verification scripts |

macOS: `xcode-select --install`, `brew install cmake ninja rust node`.
Windows: Visual Studio 2022 (Desktop C++ workload), CMake, Rust (MSVC toolchain), Node, .NET 8.

## Configure and build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DZYPHER_BUILD_TESTS=ON -DZYPHER_BUILD_EDITOR=ON
cmake --build build
```

Windows with Visual Studio:

```powershell
cmake -S . -B build -A x64 -DZYPHER_BUILD_TESTS=ON -DZYPHER_BUILD_EDITOR=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `ZYPHER_BUILD_RUNTIME` | ON | build `zypher_runtime` |
| `ZYPHER_BUILD_TESTS` | ON | build engine/editor tests |
| `ZYPHER_BUILD_EDITOR` | OFF | build editor core (and QML shell if Qt 6.5+ is found) |
| `ZYPHER_BUILD_SAMPLES` | OFF | build sample executables |
| `ZYPHER_ENABLE_SANITIZERS` | OFF | ASan + UBSan |
| `ZYPHER_WARNINGS_AS_ERRORS` | OFF | `-Werror` / `/WX` |

The remaining `ZYPHER_WITH_*` switches are integration points that are not wired
up in 0.1.0. Configuring with one of them ON fails with an explanation instead of
silently ignoring it — see [roadmap](roadmap.md).

### macOS universal binary

```bash
cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Qt editor

Qt is only needed for the desktop shell:

```bash
cmake -S . -B build-editor -DZYPHER_BUILD_EDITOR=ON \
      -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/macos"
cmake --build build-editor
./build-editor/bin/zypher_editor
```

Without Qt the editor core and its tests still build; the configure step reports
`building the editor core only`.

## Rust asset cooker

```bash
cargo test --manifest-path tools/assetcooker/Cargo.toml
cargo run --manifest-path tools/assetcooker/Cargo.toml -- --help
```

The cooker has no external crates, so it builds offline.

## C# gameplay assembly

```bash
dotnet run  --project tools/Zypher.Scripting.Tests/Zypher.Scripting.Tests.csproj
dotnet publish samples/demo/scripts/Zypher.Demo.Scripts.csproj \
       -c Release -r osx-arm64 -p:PublishAot=true -p:NativeLib=Shared
```

NativeAOT publishing requires the platform C toolchain; replace `osx-arm64` with
`win-x64` (or `win-arm64`) on Windows.

## TypeScript editor panels

```bash
cd editor/ts
npm install
npm run typecheck
npm test
```

## Shaders

Sources are in `engine/shaders` (GLSL, HLSL, MSL, Slang) and described by
`shader_manifest.json`. They are not compiled by the default build. Compile them
with the platform toolchain:

```bash
glslangValidator -V engine/shaders/glsl/pbr.vert -o build/shaders/pbr.vert.spv
glslangValidator -V engine/shaders/glsl/pbr.frag -o build/shaders/pbr.frag.spv
dxc -T vs_6_6 -E VSMain engine/shaders/hlsl/pbr.hlsl -Fo build/shaders/pbr.vs.dxil
dxc -T ps_6_6 -E PSMain engine/shaders/hlsl/pbr.hlsl -Fo build/shaders/pbr.ps.dxil
xcrun -sdk macosx metal -c engine/shaders/msl/pbr.metal -o build/shaders/pbr.air
xcrun -sdk macosx metallib build/shaders/pbr.air -o build/shaders/pbr.metallib
slangc engine/shaders/slang/pbr.slang -target spirv -o build/shaders/pbr.spv
```

## Verification

```bash
python3 tools/scripts/run_checks.py          # everything available on this host
python3 tools/scripts/run_checks.py --quick  # engine + cooker only
```

Steps that need a missing toolchain are reported as skipped, never as passing.

## Troubleshooting

- **`std::expected` / `<print>` errors** — the compiler is not in C++23 mode.
  Check `CMAKE_CXX_COMPILER`; the standard is set in `cmake/ZypherCompiler.cmake`.
- **`Qt6 not found`** — expected without Qt; the editor core still builds.
- **`error: linking with 'cc' failed` when publishing C#** — install the platform
  C toolchain (Xcode command line tools or Visual Studio C++ build tools).
- **Cooker cannot find the project** — pass `--project` an absolute path, or run
  it from the repository root.

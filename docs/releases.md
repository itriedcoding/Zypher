# Releases

Zypher publishes a **macOS disk image** and a **Windows installer** on the GitHub
Releases page. Builds are produced by `.github/workflows/release.yml` when a
version tag (`v*`) is pushed, or manually through `workflow_dispatch`.

## Downloads

| File | Platform | Format |
| --- | --- | --- |
| `zypher-<version>-macos-universal.dmg` | macOS (arm64 + x86_64) | disk image — open it and run `bin/zypher_runtime` |
| `zypher-<version>-windows-x64.exe` | Windows x64 | self-extracting installer (7-Zip SFX) — run it and pick a folder |
| `zypher-<version>-macos-universal.zip` | macOS (arm64 + x86_64) | portable archive |
| `zypher-<version>-windows-x64.zip` | Windows x64 | portable archive |
| `*.sha256` | all | checksum for each download |

Every download contains the `zypher_runtime` binary, the engine library, the
public headers, shader sources, the cooked demo project and the documentation.

## Verifying a download

macOS:

```bash
shasum -a 256 -c zypher-0.1.0-macos-universal.dmg.sha256
hdiutil attach zypher-0.1.0-macos-universal.dmg
/Volumes/Zypher\ 0.1.0/bin/zypher_runtime --help
```

Windows:

```powershell
Get-FileHash .\zypher-0.1.0-windows-x64.exe -Algorithm SHA256
.\zypher-0.1.0-windows-x64.exe          # extracts into a folder you choose
.\zypher-0.1.0-windows-x64\bin\zypher_runtime.exe --help
```

## Running the packaged demo

macOS (from the mounted image):

```bash
cd /Volumes/Zypher*
./bin/zypher_runtime \
   --scene share/zypher/samples/demo/scene.zscene \
   --pack  share/zypher/samples/demo/dist/demo.zpkg \
   --output /tmp/zypher-artifacts --width 640 --height 360 \
   --frames 2 --steps 120 --dump-frame
```

The report prints deterministic simulation and render checksums; the frame is
written to `/tmp/zypher-artifacts/frame.png`.

## Publishing automatically

`.github/workflows/auto-release.yml` runs on every push to `main`. It reads the
version from `CMakeLists.txt`; if no GitHub Release exists for `v<version>`, or
an existing release is missing the `.dmg` or the Windows `.exe` (and their
checksums), it dispatches the Release workflow, which builds both platforms and
creates or updates the release (and the tag, if it is missing). Pushes that do
not bump the version publish nothing.

You can also run **Release** manually from the Actions tab with a tag input, or
push a tag:

```bash
./tools/scripts/release.sh 0.1.1   # bumps nothing, creates and pushes the tag
```

## What the release job does

1. Checks out the tag and verifies the tag matches `project(Zypher VERSION ...)`.
2. Installs Rust (cooker) and configures CMake:
   - macOS: `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
   - Windows: Visual Studio x64
3. Builds the engine, runtime, editor core and tests.
4. Cooks `samples/demo` and runs `ctest` (including the runtime-with-pack test).
5. Stages the artifacts explicitly and **asserts** the runtime and engine library
   are present, then packages them:
   - macOS: `package_release.py` zip plus `hdiutil create -format UDZO` dmg
   - Windows: `package_release.py` zip plus a 7-Zip self-extracting `.exe`
   - SHA-256 sums are written for every download
6. Creates or updates the GitHub Release, uploading all downloads and
   refreshing the notes from this file.

The zip archives are reproducible (sorted files, fixed 1980 timestamps,
normalised permissions); disk images and self-extracting installers are not, so
rely on the published checksums for those.

## Local packaging

```bash
cmake --install build --prefix stage
mkdir -p stage/share/zypher && cp -R samples docs README.md LICENSE stage/share/zypher/

python3 tools/scripts/package_release.py --version 0.1.0 \
        --platform macos-universal --input stage --output dist
hdiutil create -volname "Zypher 0.1.0" -srcfolder stage -ov -format UDZO \
        dist/zypher-0.1.0-macos-universal.dmg     # macOS
7z a -mx=9 -sfx dist/zypher-0.1.0-windows-x64.exe stage\*    # Windows
```

## Versioning

`CMakeLists.txt` (`project(Zypher VERSION ...)`) is the source of truth for the
engine version; C# projects, the TypeScript package and the tag must match it.
The release workflow refuses to run if the tag and `project()` version differ.

# Releases

Zypher publishes a **Windows installer** and a **macOS disk image** on the
GitHub Releases page. Builds are produced by `.github/workflows/release.yml`
when a version tag (`v*`) is pushed, or manually through `workflow_dispatch`.

## Downloads

| File | Platform | What it is |
| --- | --- | --- |
| `zypher-<version>-windows-x64.exe` | Windows 10/11 x64 | Inno Setup installer: installs Zypher, adds **Zypher** to the Start Menu (and optionally the desktop), registers an uninstaller and offers to launch it |
| `zypher-<version>-macos-universal.dmg` | macOS (arm64 + x86_64) | disk image with `Zypher.command` (double-click to run) and an `Applications` shortcut for dragging it into place |
| `zypher-<version>-windows-x64.zip` | Windows x64 | portable archive, no install (run `Zypher.cmd` inside) |
| `zypher-<version>-macos-universal.zip` | macOS (arm64 + x86_64) | portable archive, no install (run `Zypher.command` inside) |
| `*.sha256` | all | checksum for each download |

Every download contains the `zypher_runtime` binary, the engine library, the
public headers, shader sources, the cooked demo project and the documentation.

## Running Zypher

**Windows.** Run `zypher-<version>-windows-x64.exe`. The wizard installs to
`%LOCALAPPDATA%\Programs\Zypher` for the current user without an administrator
prompt (it offers an all-users install if you are an administrator) and, when
"Launch Zypher" is left checked, opens the **Zypher** console. That console
renders the demo scene, prints the report and stays open so you can read it,
writing `frame.png` and `report.json` to `%LOCALAPPDATA%\Zypher\artifacts`.
The Start Menu shortcut runs the same launcher; to pass runtime options directly
use `Zypher.cmd --help`.

**macOS.** Open the disk image and double-click `Zypher.command`. Terminal opens,
renders the demo, prints the report and waits for Return before closing; frames
and the report go to `~/Zypher/artifacts`. Drag `Zypher.command` (or the whole
volume contents) into `Applications` to keep it. macOS may warn that the build is
not signed: right-click the file, choose **Open**, then confirm.

**Portable archives.** Unzip and run `Zypher.cmd` (Windows) or `Zypher.command`
(macOS). Nothing is registered with the system.

> Releases before 0.1.1 shipped a 7-Zip self-extracting archive named `.exe`. It
> only unpacked to a temporary folder and exited, so it looked like a console
> window opening and closing. Use 0.1.1 or newer for the real installer.

## Verifying a download

```bash
# macOS or Linux
shasum -a 256 -c zypher-0.1.1-macos-universal.dmg.sha256
```

```powershell
# Windows
Get-FileHash .\zypher-0.1.1-windows-x64.exe -Algorithm SHA256
# compare with the value in zypher-0.1.1-windows-x64.exe.sha256
```

## Publishing automatically

`.github/workflows/auto-release.yml` runs on every push to `main`. It reads the
version from `CMakeLists.txt`; if no GitHub Release exists for `v<version>`, or
an existing release is missing the `.dmg`, the Windows `.exe` or their
checksums, it dispatches the Release workflow, which builds both platforms and
creates or updates the release (and the tag, if it is missing). Pushes that do
not bump the version publish nothing.

You can also run **Release** manually from the Actions tab with a tag input, or
push a tag:

```bash
./tools/scripts/release.sh 0.1.2   # bumps nothing, creates and pushes the tag
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
   - macOS: `package_release.py` zip, then a `.dmg` built with `hdiutil` that
     contains `Zypher.command` and an `Applications` shortcut
   - Windows: `package_release.py` zip, then an Inno Setup installer
   - SHA-256 sums are written for every download
6. Verifies the packaged artifacts before anything is uploaded:
   - the `.dmg` is mounted, its launcher is checked, and `Zypher.command
     --version` is run from the mounted volume
   - the installer is run with `/VERYSILENT` into a scratch directory, the
     installed `bin\zypher_runtime.exe` and `Zypher.cmd` are executed, the demo
     is rendered from the installed copy, and the uninstaller is run to confirm
     it removes what it installed
7. Creates or updates the GitHub Release, uploading all downloads and refreshing
   the notes from this file.

The zip archives are reproducible (sorted files, fixed 1980 timestamps,
normalised permissions); installers and disk images are not, so rely on the
published checksums for those.

## Local packaging

```bash
cmake --install build --prefix stage        # or copy bin/lib/include by hand
mkdir -p stage/share/zypher && cp -R samples docs README.md LICENSE stage/share/zypher/
cp tools/installer/macos/Zypher.command stage/     # macOS launcher
cp tools/installer/windows/Zypher.cmd stage/       # Windows launcher

python3 tools/scripts/package_release.py --version 0.1.1 \
        --platform macos-universal --input stage --output dist

# macOS disk image
cp -R stage /tmp/dmg-src && ln -s /Applications /tmp/dmg-src/Applications
hdiutil create -volname "Zypher 0.1.1" -srcfolder /tmp/dmg-src -ov -format UDZO \
        dist/zypher-0.1.1-macos-universal.dmg

# Windows installer (Inno Setup 6)
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyVersion=0.1.1 ^
        /DStageDir=stage /DOutputDir=dist tools\installer\zypher.iss
```

The installer definition lives in [`tools/installer/zypher.iss`](../tools/installer/zypher.iss).

## Versioning

`CMakeLists.txt` (`project(Zypher VERSION ...)`) is the source of truth for the
engine version; C# projects, the TypeScript package and the tag must match it.
The release workflow refuses to run if the tag and `project()` version differ.

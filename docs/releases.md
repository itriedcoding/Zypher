# Releases

Zypher publishes a **Windows installer** and a **macOS disk image** on the
GitHub Releases page. Both contain the **Zypher Editor**, a Qt 6/QML desktop
application, so starting Zypher opens a window rather than a terminal. Builds
are produced by `.github/workflows/release.yml` when a version tag (`v*`) is
pushed, or manually through `workflow_dispatch`.

## Downloads

| File | Platform | What it is |
| --- | --- | --- |
| `zypher-<version>-windows-x64.exe` | Windows 10/11 x64 | Inno Setup installer: installs the editor with its Qt runtime, adds **Zypher Editor** to the Start Menu (and optionally the desktop), registers an uninstaller and offers to launch the editor |
| `zypher-<version>-macos-universal.dmg` | macOS (arm64 + x86_64) | disk image containing `Zypher Editor.app`, the command line runtime, `Zypher.command` and an `Applications` shortcut |
| `zypher-<version>-windows-x64.zip` | Windows x64 | portable archive with the editor, no install |
| `zypher-<version>-macos-universal.zip` | macOS (arm64 + x86_64) | portable command line payload (the `.app` bundle is in the disk image) |
| `*.sha256` | all | checksum for each download |

Every download also contains the `zypher_runtime` command line tool, the engine
library, the public headers, shader sources, the cooked demo project and the
documentation. The editor needs no separate runtime: Qt is deployed next to it
(`windeployqt` on Windows, inside the bundle with `macdeployqt` on macOS).

## Starting Zypher

**Windows.** Run `zypher-<version>-windows-x64.exe`. The wizard installs to
`%LOCALAPPDATA%\Programs\Zypher` for the current user without an administrator
prompt (it offers an all-users install if you are an administrator) and, when
"Launch Zypher Editor" is left checked, opens the editor. The editor loads the
bundled demo project, so the hierarchy, inspector, asset browser and console are
ready to use. The Start Menu and desktop shortcuts open the same window; no
console appears because `zypher_editor.exe` is a GUI subsystem executable.

**macOS.** Open the disk image and double-click `Zypher Editor.app` (or drag it
into `Applications` first). `Zypher.command`, included for scripted use, opens
the editor when run without arguments. macOS may report that the build is not
signed: right-click the app, choose **Open**, then confirm.

**Command line.** The runtime is still there for automation:

```bash
zypher_runtime --scene <scene> --pack <pack> --output <dir> --dump-frame
```

> Releases before 0.1.1 shipped a 7-Zip self-extracting archive named `.exe`,
> which only unpacked to a temporary folder, and releases before 0.1.2 shipped
> the runtime without the editor, so opening the download showed a console
> window. Use 0.1.2 or newer.

## Verifying a download

```bash
# macOS or Linux
shasum -a 256 -c zypher-0.1.2-macos-universal.dmg.sha256
```

```powershell
# Windows
Get-FileHash .\zypher-0.1.2-windows-x64.exe -Algorithm SHA256
# compare with the value in zypher-0.1.2-windows-x64.exe.sha256
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
./tools/scripts/release.sh 0.1.3   # bumps nothing, creates and pushes the tag
```

## What the release job does

1. Checks out the tag and verifies the tag matches `project(Zypher VERSION ...)`.
2. Installs Rust (cooker) and Qt 6.8 (aqtinstall, cached between runs), then
   configures CMake:
   - macOS: `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
   - Windows: Visual Studio x64
3. Builds the engine, runtime, editor shell and tests.
4. Cooks `samples/demo` and runs `ctest` (including the runtime-with-pack test).
5. Stages the artifacts explicitly and **asserts** the runtime, engine library
   and editor are present, then packages them:
   - macOS: `package_release.py` zip, `macdeployqt` on the editor app, then a
     `.dmg` built with `hdiutil` that holds `Zypher Editor.app`, `Zypher.command`
     and an `Applications` shortcut
   - Windows: `windeployqt` beside `zypher_editor.exe`, `package_release.py` zip,
     then an Inno Setup installer
   - SHA-256 sums are written for every download
6. Verifies the packaged artifacts before anything is uploaded:
   - the `.dmg` is mounted; the launcher is run, and the editor under
     `Zypher Editor.app` is started (offscreen) and must stay running
   - the installer is run with `/VERYSILENT` into a scratch directory; the
     installed runtime, `Zypher.cmd`, PE subsystem (GUI, so no console) and the
     editor's startup are checked, the demo is rendered from the installed copy,
     and the uninstaller is run to confirm it removes what it installed
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

python3 tools/scripts/package_release.py --version 0.1.2 \
        --platform macos-universal --input stage --output dist

# macOS disk image (editor app included)
cp -R build/bin/zypher_editor.app "stage/Zypher Editor.app"
"$QT_DIR/bin/macdeployqt" "stage/Zypher Editor.app" -always-overwrite -qmldir=editor/qml
cp -R stage /tmp/dmg-src && ln -s /Applications /tmp/dmg-src/Applications
hdiutil create -volname "Zypher 0.1.2" -srcfolder /tmp/dmg-src -ov -format UDZO \
        dist/zypher-0.1.2-macos-universal.dmg

# Windows installer (Inno Setup 6)
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyVersion=0.1.2 ^
        /DStageDir=stage /DOutputDir=dist tools\installer\zypher.iss
```

The installer definition lives in [`tools/installer/zypher.iss`](../tools/installer/zypher.iss).

## Licensing

Zypher is MIT licensed. The Windows and macOS builds bundle Qt 6 under the
LGPLv3: the Qt libraries ship as separate, dynamically linked files next to
`zypher_editor.exe` (and as frameworks inside the macOS bundle), so they can be
replaced with a compatible build. Qt's licence text is available at
<https://www.qt.io/licensing/open-source-lgpl-obligations>.

## Versioning

`CMakeLists.txt` (`project(Zypher VERSION ...)`) is the source of truth for the
engine version; C# projects, the TypeScript package and the tag must match it.
The release workflow refuses to run if the tag and `project()` version differ.

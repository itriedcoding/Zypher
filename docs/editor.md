# Editor

The editor is split into a dependency-free core, a Qt/QML desktop shell, and
TypeScript panel logic. Only the core is required to build headlessly; Qt is
optional for development but is what the released applications use.

## Shipped builds

The Windows installer and the macOS disk image contain the compiled editor, so
starting Zypher opens a window rather than a terminal:

- **Windows**: the `Zypher Editor` Start Menu / desktop shortcut runs
  `bin\zypher_editor.exe`. It is a GUI subsystem executable, so no console window
  appears. `Zypher.cmd` with no arguments opens the same editor.
- **macOS**: `Zypher Editor.app` opens from the disk image, `Applications`, or by
  double-clicking `Zypher.command` with no arguments.

On startup the editor opens the bundled demo project
(`share/zypher/samples/demo/zypher.project.json`, or `Contents/Resources/...`
inside the macOS bundle) so the hierarchy, inspector, assets and console are
populated immediately. `zypher_editor path/to/zypher.project.json` opens a
specific project instead.

The viewport shows scene status; rendering backends arrive with the GPU
milestone. `zypher_runtime --dump-frame` still produces reference frames from the
software rasteriser.

## Layers

| Layer | Location | Notes |
| --- | --- | --- |
| Session core | `editor/core` | `EditorSession`: open project, mutate scene, undo/redo, save |
| Qt adapter | `editor/qt` | `EditorController` exposes the session to QML as `editor` |
| QML shell | `editor/qml` | `Main.qml`, hierarchy, inspector, asset browser, console |
| Panel logic | `editor/ts` | Project model, scene document, undo stack, command bridge |

The QML files are compiled into the executable as a Qt resource module
(`Zypher.Editor`), so the shipped app never looks at a source directory.

## Building the shell

Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2) is needed:

```bash
# Any Qt 6.8 installation works, for example one installed with aqtinstall:
python3 -m venv .qt-venv && .qt-venv/bin/pip install aqtinstall
.qt-venv/bin/aqt install-qt linux desktop 6.8.3 linux_gcc_64 --outputdir "$HOME/Qt"

cmake -S . -B build-qt -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64" \
      -DZYPHER_BUILD_EDITOR=ON
cmake --build build-qt --target zypher_editor
./build-qt/bin/zypher_editor            # Linux, for development
```

`windows-release` builds use `win64_msvc2022_64` and `mac-release` builds use
`clang_64` (universal) from aqtinstall; the Release workflow deploys Qt with
`windeployqt` / `macdeployqt` and starts the packaged editor once before
uploading.

## Session core

```cpp
zy::editor::EditorSession session;
session.openProject("samples/demo/zypher.project.json");
session.setEntityPosition(1, {1.0f, 2.0f, 3.0f});
session.undo();
session.saveScene();
```

Editing is undoable: every mutation stores a labelled `SceneDesc` snapshot
(128 deep). `saveScene` writes `sceneToJson(...).dump(true)` back to the
`.zscene` file. The core has no Qt dependency, so `zypher_editor_tests` runs in
CI on every platform.

## QML shell

The QML panels bind directly to `EditorController` properties (`entityNames`,
`selection`, `selectedPosition`, `canUndo`, ...) and call its `Q_INVOKABLE`
methods. `File > Open Project` (`Ctrl+O`), `File > Open Demo Project`,
`File > Save Scene` (`Ctrl+S`) and `Edit > Undo/Redo` are wired.

## TypeScript panels

`editor/ts` owns the logic that does not need Qt: manifest parsing and validation,
asset ids, scene document editing with undo, and the JSON command protocol used
between QML/Monaco panels and the editor.

```bash
cd editor/ts
npm install
npm run typecheck   # tsc --noEmit, strict
npm test            # builds and runs node --test (18 tests)
```

The package is ESM with `NodeNext` resolution, so imports use `.js` extensions.

### Command protocol

Requests and responses are JSON strings:

```json
{"id": 12, "command": {"type": "setTransform", "index": 0, "position": [1, 2, 3]}}
```

```json
{"id": 12, "ok": true, "event": {"kind": "sceneChanged", "revision": 4, "dirty": true, "entityNames": ["Ground", "Box"], "selected": 0}}
```

Commands: `select`, `addEntity`, `removeEntity`, `renameEntity`, `setTransform`,
`moveEntity`, `undo`, `redo`, `save`. Every payload is validated
(`decodeRequest`), failures come back as `{ok: false, error}` and never throw
across the boundary.

## Monaco

The Monaco editor is embedded by the Qt shell for C#/JSON editing. The TypeScript
panels are designed to also run inside Monaco's webview: the bridge input is a
plain JSON string, so no Qt types leak into panel code.

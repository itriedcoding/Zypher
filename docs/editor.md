# Editor

The editor is split into a dependency-free core, a Qt/QML desktop shell, and
TypeScript panel logic. Only the core is required to build headlessly; Qt is
optional.

## Layers

| Layer | Location | Notes |
| --- | --- | --- |
| Session core | `editor/core` | `EditorSession`: open project, mutate scene, undo/redo, save |
| Qt adapter | `editor/qt` | `EditorController` exposes the session to QML as `editor` |
| QML shell | `editor/qml` | `Main.qml`, hierarchy, inspector, asset browser, console |
| Panel logic | `editor/ts` | Project model, scene document, undo stack, command bridge |

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

Requires Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2):

```bash
cmake -S . -B build-editor -DZYPHER_BUILD_EDITOR=ON -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.0/macos
cmake --build build-editor
./build-editor/bin/zypher_editor
```

The QML panels bind directly to `EditorController` properties (`entityNames`,
`selection`, `selectedPosition`, `canUndo`, ...) and call its `Q_INVOKABLE`
methods. `File > Open Project`, `File > Save Scene` (`Ctrl+S`) and
`Edit > Undo/Redo` are wired.

The viewport currently shows scene status; the GPU viewport arrives with the
rendering backends. The software renderer can still produce reference frames via
`zypher_runtime --dump-frame`.

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

# Formats

All multi-byte integers are little-endian. Every format starts with an 8-byte
magic string so a mismatched file fails immediately.

## Asset ids

```
id = FNV-1a64(utf8(normalizedProjectRelativePath))
```

- Offset basis `0xcbf29ce484222325`, prime `1099511628211`.
- Normalised path: forward slashes, no leading `./`, no duplicate separators.
- Case sensitive: `models/Cube.obj` and `models/cube.obj` are different assets.

The C++ engine (`engine/include/zypher/assets/AssetId.h`), the Rust cooker
(`tools/assetcooker/src/lib.rs`) and the TypeScript editor
(`editor/ts/src/projectModel.ts`) all implement the same formula and assert the
same known value in their tests.

## `.zpkg` pack file

Header, 56 bytes:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | magic `ZYPKG001` |
| 8 | 4 | version (`1`) |
| 12 | 4 | flags (reserved, 0) |
| 16 | 4 | asset count |
| 20 | 4 | reserved |
| 24 | 8 | index offset |
| 32 | 8 | string table offset |
| 40 | 8 | string table size |
| 48 | 8 | total file size |

Index entry, 32 bytes each, sorted by asset id:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | asset id |
| 8 | 4 | name offset into the string table |
| 12 | 4 | name length |
| 16 | 4 | data offset |
| 20 | 4 | data size |
| 24 | 1 | asset type (`AssetType` enum) |
| 25 | 3 | padding |
| 28 | 4 | CRC32 of the data |

Layout: header, index, string table, then payloads. The reader validates magic,
version, bounds, counts and per-asset CRCs (`PackFile::open`). Entries are sorted
by id so lookup is a binary search.

Asset types: `0 unknown, 1 mesh, 2 texture, 3 audio, 4 scene, 5 shader, 6 json,
7 font, 8 raw`.

## Mesh blob (`ZYMESH01`)

Cooked meshes are stored as a compact blob: magic `ZYMESH01`, vertex and index
counts, then interleaved vertices (position, normal, uv) and 32-bit indices.
`encodeMeshBlob` / `decodeMeshBlob` implement it; the decoder rejects truncated
or inconsistent data.

## Texture / audio payloads

Cooked textures keep `ZYTEX001` and cooked audio keeps `ZYAUD001` headers until
they are uploaded or decoded. The cooker validates the source container first
(PNG/JPEG/WebP/EXR/KTX2, WAV/OGG/FLAC) and stores either the original bytes
(pass-through, `textureCompression: none`) or a codec payload.

## `.zscene` (JSON)

Hand-editable and comment tolerant (`//`, `/* */`).

```json
{
  "name": "Demo",
  "environment": {
    "clearColor": [0.05, 0.06, 0.09, 1.0],
    "light": { "direction": [-0.4, -1, -0.55], "color": [1, 0.97, 0.92], "intensity": 1.25 }
  },
  "camera": { "position": [6.5, 3.6, 8.5], "target": [0, 1.2, 0], "fov": 52, "near": 0.1, "far": 200 },
  "entities": [
    {
      "name": "Box",
      "mesh": "models/cube.obj",
      "transform": { "position": [0, 3, 0], "rotation": [0, 12, 0], "scale": [1, 1, 1] },
      "material": { "baseColor": [0.9, 0.35, 0.28, 1], "roughness": 0.4, "metallic": 0 },
      "rigidbody": { "mass": 1.0, "restitution": 0.25, "halfExtents": [0.5, 0.5, 0.5] },
      "script": "PlayerController"
    }
  ]
}
```

- `rotation` is Euler degrees, applied X then Y then Z.
- `rigidbody.static: true` makes a static collider; otherwise the body is dynamic
  with `mass` (default 1) and `restitution` (default 0.2).
- `mesh` is a project-relative path; its asset id resolves into the pack.
- Unknown fields are ignored, so scenes stay forward compatible.

## `zypher.project.json` (JSON)

```json
{
  "name": "Zypher Demo",
  "version": "0.1.1",
  "assetRoot": "assets",
  "output": "dist/demo.zpkg",
  "scene": "scene.zscene",
  "textureCompression": "none",
  "assets": [
    { "path": "models/cube.obj", "type": "mesh" },
    { "path": "textures/grid.png", "type": "texture" }
  ]
}
```

Paths are relative to the manifest. `assets[].type` selects the cooker path.
The scene is cooked into the pack as a `Scene` asset in addition to the listed
assets.

## Pack sidecar

The cooker writes `<pack>.json` next to the `.zpkg` with the asset table (id,
path, type, size, crc32) so build tools and the editor can inspect a pack without
parsing it.

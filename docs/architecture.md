# Architecture

Zypher is a set of small libraries around one runtime loop. Nothing in the core
depends on a windowing system, a GPU API or a scripting runtime, which is why the
whole engine can run headlessly in CI.

## Module map

| Layer | Location | Responsibility |
| --- | --- | --- |
| Base | `engine/include/zypher/core` | Fixed-width types, `Result<T>`/`Status`, assertions, logging, time, jobs, arenas, file system, config, platform abstraction |
| Math | `engine/include/zypher/math` | `Vec2/3/4`, `Mat4` (column-major, right-handed, 0..1 clip depth), `Quat`, `Transform`, geometry helpers |
| ECS | `engine/include/zypher/ecs` | Sparse-set `Registry`; components in `Components.h` |
| Assets | `engine/include/zypher/assets` | Asset ids, `.zpkg` reader (`PackFile`), mesh blob codec, `AssetRegistry` cache |
| Scene | `engine/include/zypher/scene` | `.zscene` JSON → `SceneDesc` → ECS entities, and back |
| Simulation | `engine/src/physics`, `engine/src/nav` | AABB rigid bodies, fixed-step integration, A* navigation grid |
| Rendering | `engine/src/render` | `RenderDevice` interface, `SoftwareDevice` rasteriser, PNG writer, primitive meshes |
| Scripting | `engine/src/scripting` | Loads a native module exporting `zypher_script_module()` and drives instances |
| Runtime | `engine/src/app`, `engine/src/main.cpp` | CLI, frame loop, report/checksum generation |
| Editor | `editor/core`, `editor/qt`, `editor/qml`, `editor/ts` | Session/undo model, Qt adapter, QML panels, TypeScript panel logic |
| Tooling | `tools/assetcooker`, `tools/scripts` | Rust cooker, Python validation/packaging/verification |

## Frame loop

`Application` owns the loop:

1. `Clock` produces a fixed `deltaSeconds` (default 1/60). The loop never uses
   wall-clock deltas, so a run is reproducible.
2. `PhysicsWorld::step` integrates dynamic bodies, resolves AABB contacts against
   static colliders and grounds stacked boxes with a small resting bias.
3. Each `ScriptHost` instance receives `on_update(instance, deltaSeconds)`.
   Scripts can read/write transforms and velocities through C function pointers;
   they never touch engine containers directly.
4. `SoftwareDevice` renders the scene into an RGBA buffer (depth test, back-face
   culling, Lambert lighting, ambient term), then `PngWriter` writes a frame when
   `--dump-frame` is set.
5. The runtime hashes the simulation state and the framebuffer (FNV-1a 64) and
   writes `report.json`. Two runs of the same scene/pack produce identical
   checksums.

## Determinism

- Fixed timestep and deterministic iteration order (sparse-set storage iterated
  by index, not hash map order).
- Asset ids are content-path hashes, not load order.
- The software rasteriser sorts draw calls by asset id and uses integer
  rasterisation rules.
- Reports contain `simulation` and `render` checksums; the tests and CI compare
  them across runs.

## Memory

`Arena` provides bump allocation for frame-scoped data; engine subsystems use
standard containers with explicit ownership (`Scope`, `Ref`). `Memory.h` exposes
allocation counters used by the profiler stub.

## Threads

`Jobs` is a small work-stealing pool used by the cooker-side and renderer-side
batch loops. Scripts and physics run on the main thread; the engine never calls
into a script from a worker thread, which keeps the C# ABI single-threaded.

## Rendering abstraction

`RenderDevice` is the seam for new backends:

```cpp
class RenderDevice {
public:
    virtual bool initialize(const RenderDeviceDesc& desc, std::string& error) = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(const FrameDesc& frame) = 0;
    virtual void submitMesh(const MeshDraw& draw) = 0;
    virtual void endFrame() = 0;
};
```

`SoftwareDevice` implements it today and is used by the tests and the runtime.
A Vulkan/D3D12/Metal backend implements the same interface; shader sources live
in `engine/shaders` and are described by `shader_manifest.json`.

## Coordinate system

Right-handed, +Y up, -Z forward, column-major matrices with a 0..1 depth range
(matching Vulkan/D3D12/Metal conventions). `Mat4::perspective` and `Mat4::lookAt`
document the exact conventions.

## Error handling

Fallible operations return `Result<T>` or `Status`; `ZY_TRY`/`ZY_TRY_ASSIGN`
propagate errors. The runtime maps errors to exit codes and writes them to the
crash log (`artifacts/crashes/`). Assertions are for programming errors only.

# Gameplay scripting (C#)

Gameplay code is C#. The engine never hosts the CLR: a game assembly is published
with .NET **NativeAOT** into a native shared library that exports one symbol,
`zypher_script_module()`. The runtime loads that library with `dlopen` /
`LoadLibrary` and calls its function table once per frame. No GC pauses inside
the frame, no reflection, no managed runtime to configure.

## ABI (version 1)

The C side is `engine/include/zypher/scripting/ScriptApi.h`; the C# mirror is
`engine/scripting/csharp/Zypher.Scripting/NativeApi.cs`. Both sides check
`struct_size` / `abi_version` and refuse to run on a mismatch.

```c
typedef struct ZypherScriptModule {
    uint32_t abi_version;
    const char* name;
    void* (*create_instance)(const char* class_name, ZypherEntityHandle entity,
                             const ZypherScriptContext* context);
    void (*destroy_instance)(void* instance);
    void (*on_start)(void* instance);
    void (*on_update)(void* instance, float delta_seconds);
    void (*on_stop)(void* instance);
} ZypherScriptModule;
```

The context carries engine callbacks: `log_info`, `log_warn`, `get_transform`,
`set_transform`, `get_velocity`, `set_velocity`, `set_velocity_y`,
`find_entity`. Handles are `{index, generation}` pairs, so a stale handle can be
detected instead of touching recycled storage.

## Writing a behaviour

```csharp
public sealed class PlayerController : Behaviour
{
    public override void OnStart() => Log($"spawned at {Position}");

    public override void OnUpdate()
    {
        Vector3 position = Position;
        Position = new Vector3(position.X + 1.2f * DeltaSeconds, position.Y, position.Z);
        Velocity = new Vector3(Velocity.X, 3.4f, Velocity.Z);
    }
}
```

`Behaviour` exposes `Self`, `Position`, `Velocity`, `DeltaSeconds`, `Log`,
`Warn` and `FindEntity`. The lifecycle is `OnStart` once, `OnUpdate` per fixed
step, `OnStop` before destruction.

## Registering behaviours

The scene's `"script"` field is the class name string. A game assembly exports:

```csharp
[UnmanagedCallersOnly(EntryPoint = "zypher_script_module")]
public static IntPtr ZypherScriptModule()
{
    var registry = new ScriptRegistry();
    registry.Add<PlayerController>("PlayerController");
    registry.Add<Spinner>("Spinner");
    return ScriptHostBridge.CreateModule("My Game", registry);
}
```

`CreateModule` allocates the native module table once and keeps the registry
alive; instances are `GCHandle`s owned by the engine.

## Publishing

```bash
dotnet publish MyGame.csproj -c Release -r osx-arm64 -p:PublishAot=true -p:NativeLib=Shared
dotnet publish MyGame.csproj -c Release -r win-x64    -p:PublishAot=true -p:NativeLib=Shared
```

Output: `MyGame.dylib` on macOS, `MyGame.dll` on Windows. Pass it to the runtime:

```bash
zypher_runtime --scene game.zscene --pack game.zpkg \
               --script-module path/to/MyGame.dylib
```

If the module is missing or rejects the ABI version, the runtime logs a warning
and keeps running with the entities' built-in behaviour.

## Testing without the engine

`tools/Zypher.Scripting.Tests` builds a fake native context (delegates backed by a
dictionary) and drives the real behaviours through it:

```bash
dotnet run --project tools/Zypher.Scripting.Tests/Zypher.Scripting.Tests.csproj
```

This catches gameplay logic errors on machines without a GPU or the runtime.

## Lua (planned)

`ZYPHER_WITH_LUA` is reserved for an embedded Lua host for tooling and modding.
It is not implemented in 0.1.0 and the option fails fast; C# is the supported
gameplay language.

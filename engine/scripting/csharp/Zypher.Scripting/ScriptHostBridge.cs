using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Zypher.Scripting;

/// <summary>
/// Maps behaviour class names from the scene file to managed instances.
/// </summary>
public sealed class ScriptRegistry
{
    private readonly Dictionary<string, Func<Behaviour>> _factories = new(StringComparer.Ordinal);

    public ScriptRegistry Add<T>(string className) where T : Behaviour, new()
    {
        if (string.IsNullOrWhiteSpace(className))
        {
            throw new ArgumentException("class name must not be empty", nameof(className));
        }
        _factories[className] = static () => new T();
        return this;
    }

    public ScriptRegistry Add(string className, Func<Behaviour> factory)
    {
        _factories[className] = factory ?? throw new ArgumentNullException(nameof(factory));
        return this;
    }

    public IReadOnlyCollection<string> ClassNames => _factories.Keys;

    internal Behaviour? Create(string className)
    {
        if (!_factories.TryGetValue(className, out Func<Behaviour>? factory))
        {
            return null;
        }
        return factory();
    }
}

/// <summary>
/// Creates the native module table exported from the game assembly. A game
/// project includes a tiny entry file such as:
///
/// <code>
/// [UnmanagedCallersOnly(EntryPoint = "zypher_script_module")]
/// public static IntPtr ZypherScriptModule()
/// {
///     var registry = new ScriptRegistry();
///     registry.Add&lt;PlayerController&gt;("PlayerController");
///     return ScriptHostBridge.CreateModule("My Game", registry);
/// }
/// </code>
/// </summary>
public static class ScriptHostBridge
{
    private static ScriptRegistry? s_registry;
    private static IntPtr s_modulePointer;

    public static IntPtr CreateModule(string name, ScriptRegistry registry)
    {
        ArgumentNullException.ThrowIfNull(registry);
        s_registry = registry;

        unsafe
        {
            var module = new NativeApi.Module
            {
                AbiVersion = NativeApi.AbiVersion,
                Name = Marshal.StringToHGlobalAnsi(name),
                CreateInstance = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, NativeApi.EntityHandle, IntPtr, IntPtr>)&CreateInstance,
                DestroyInstance = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, void>)&DestroyInstance,
                OnStart = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, void>)&OnStart,
                OnUpdate = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, float, void>)&OnUpdate,
                OnStop = (IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, void>)&OnStop,
            };
            s_modulePointer = Marshal.AllocHGlobal(Marshal.SizeOf<NativeApi.Module>());
            Marshal.StructureToPtr(module, s_modulePointer, false);
        }
        return s_modulePointer;
    }

    private static string ReadUtf8(IntPtr text)
    {
        if (text == IntPtr.Zero)
        {
            return string.Empty;
        }
        int length = 0;
        while (Marshal.ReadByte(text, length) != 0)
        {
            ++length;
        }
        byte[] buffer = new byte[length];
        Marshal.Copy(text, buffer, 0, length);
        return Encoding.UTF8.GetString(buffer);
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static IntPtr CreateInstance(IntPtr className, NativeApi.EntityHandle entity, IntPtr context)
    {
        if (s_registry is null || context == IntPtr.Zero)
        {
            return IntPtr.Zero;
        }
        Behaviour? behaviour = s_registry.Create(ReadUtf8(className).Trim());
        if (behaviour is null)
        {
            return IntPtr.Zero;
        }
        try
        {
            var nativeContext = Marshal.PtrToStructure<NativeApi.Context>(context);
            behaviour.Attach(new ScriptContext(nativeContext), ScriptContext.FromNative(entity));
        }
        catch (Exception error)
        {
            UnityEngine_Shim.Unused(error);
            return IntPtr.Zero;
        }
        return GCHandle.ToIntPtr(GCHandle.Alloc(behaviour, GCHandleType.Normal));
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void DestroyInstance(IntPtr instance)
    {
        if (instance == IntPtr.Zero)
        {
            return;
        }
        GCHandle handle = GCHandle.FromIntPtr(instance);
        if (handle.Target is Behaviour behaviour)
        {
            try
            {
                behaviour.OnStop();
            }
            catch (Exception error)
            {
                UnityEngine_Shim.Unused(error);
            }
        }
        handle.Free();
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnStart(IntPtr instance)
    {
        if (GCHandle.FromIntPtr(instance).Target is Behaviour behaviour)
        {
            behaviour.OnStart();
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnUpdate(IntPtr instance, float deltaSeconds)
    {
        if (GCHandle.FromIntPtr(instance).Target is Behaviour behaviour)
        {
            behaviour.OnUpdate();
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void OnStop(IntPtr instance)
    {
        if (GCHandle.FromIntPtr(instance).Target is Behaviour behaviour)
        {
            behaviour.OnStop();
        }
    }
}

internal static class UnityEngine_Shim
{
    // Keeps exception filters explicit without pulling in a logging dependency.
    public static void Unused(Exception error) => _ = error.Message;
}

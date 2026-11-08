using System.Runtime.InteropServices;
using System.Text;

namespace Zypher.Scripting;

internal readonly struct EntityHandle
{
    public static readonly EntityHandle Invalid = new(uint.MaxValue, 0);

    public EntityHandle(uint index, uint generation)
    {
        Index = index;
        Generation = generation;
    }

    public uint Index { get; }
    public uint Generation { get; }

    public bool IsValid => Index != uint.MaxValue;
}

/// <summary>Three component float vector used by the scripting API.</summary>
public readonly struct Vector3
{
    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public float X { get; }
    public float Y { get; }
    public float Z { get; }

    public static Vector3 Zero => new(0f, 0f, 0f);

    public override string ToString() => $"({X:0.###}, {Y:0.###}, {Z:0.###})";
}

/// <summary>
/// Managed view over the engine callbacks. Every behaviour instance shares the
/// same context for the duration of a frame.
/// </summary>
internal sealed class ScriptContext
{
    private readonly IntPtr _host;
    private readonly NativeApi.LogCallback _logInfo;
    private readonly NativeApi.LogCallback _logWarn;
    private readonly NativeApi.GetTransformCallback _getTransform;
    private readonly NativeApi.SetTransformCallback _setTransform;
    private readonly NativeApi.GetVelocityCallback _getVelocity;
    private readonly NativeApi.SetVelocityCallback _setVelocity;
    private readonly NativeApi.SetVelocityYCallback _setVelocityY;
    private readonly NativeApi.FindEntityCallback _findEntity;

    public ScriptContext(NativeApi.Context native)
    {
        if (native.StructSize != (uint)Marshal.SizeOf<NativeApi.Context>())
        {
            throw new InvalidOperationException(
                $"Script context size mismatch: engine sent {native.StructSize}, runtime expects {Marshal.SizeOf<NativeApi.Context>()}");
        }

        _host = native.Host;
        DeltaSeconds = native.DeltaSeconds;
        _logInfo = Marshal.GetDelegateForFunctionPointer<NativeApi.LogCallback>(native.LogInfo);
        _logWarn = Marshal.GetDelegateForFunctionPointer<NativeApi.LogCallback>(native.LogWarn);
        _getTransform = Marshal.GetDelegateForFunctionPointer<NativeApi.GetTransformCallback>(native.GetTransform);
        _setTransform = Marshal.GetDelegateForFunctionPointer<NativeApi.SetTransformCallback>(native.SetTransform);
        _getVelocity = Marshal.GetDelegateForFunctionPointer<NativeApi.GetVelocityCallback>(native.GetVelocity);
        _setVelocity = Marshal.GetDelegateForFunctionPointer<NativeApi.SetVelocityCallback>(native.SetVelocity);
        _setVelocityY = Marshal.GetDelegateForFunctionPointer<NativeApi.SetVelocityYCallback>(native.SetVelocityY);
        _findEntity = Marshal.GetDelegateForFunctionPointer<NativeApi.FindEntityCallback>(native.FindEntity);
    }

    public float DeltaSeconds { get; }

    public void Log(string message)
    {
        IntPtr text = Marshal.StringToHGlobalAnsi(message);
        try
        {
            _logInfo(_host, text);
        }
        finally
        {
            Marshal.FreeHGlobal(text);
        }
    }

    public void Warn(string message)
    {
        IntPtr text = Marshal.StringToHGlobalAnsi(message);
        try
        {
            _logWarn(_host, text);
        }
        finally
        {
            Marshal.FreeHGlobal(text);
        }
    }

    public Vector3 GetPosition(EntityHandle entity) => ToVector(_getTransform(_host, ToNative(entity)).Position);

    public Vector3 GetScale(EntityHandle entity) => ToVector(_getTransform(_host, ToNative(entity)).Scale);

    public void SetPosition(EntityHandle entity, Vector3 position)
    {
        NativeApi.Transform transform = _getTransform(_host, ToNative(entity));
        transform.Position = ToNative(position);
        _setTransform(_host, ToNative(entity), transform);
    }

    public void SetScale(EntityHandle entity, Vector3 scale)
    {
        NativeApi.Transform transform = _getTransform(_host, ToNative(entity));
        transform.Scale = ToNative(scale);
        _setTransform(_host, ToNative(entity), transform);
    }

    public void Translate(EntityHandle entity, Vector3 delta)
    {
        NativeApi.Transform transform = _getTransform(_host, ToNative(entity));
        transform.Position = new NativeApi.Vec3
        {
            X = transform.Position.X + delta.X,
            Y = transform.Position.Y + delta.Y,
            Z = transform.Position.Z + delta.Z,
        };
        _setTransform(_host, ToNative(entity), transform);
    }

    public Vector3 GetVelocity(EntityHandle entity) => ToVector(_getVelocity(_host, ToNative(entity)));

    public void SetVelocity(EntityHandle entity, Vector3 velocity) => _setVelocity(_host, ToNative(entity), ToNative(velocity));

    public void SetVelocityY(EntityHandle entity, float value) => _setVelocityY(_host, ToNative(entity), value);

    public EntityHandle FindEntity(string name)
    {
        IntPtr text = Marshal.StringToHGlobalAnsi(name);
        try
        {
            NativeApi.EntityHandle handle = _findEntity(_host, text);
            return new EntityHandle(handle.Index, handle.Generation);
        }
        finally
        {
            Marshal.FreeHGlobal(text);
        }
    }

    public static NativeApi.EntityHandle ToNative(EntityHandle entity) =>
        new() { Index = entity.Index, Generation = entity.Generation };

    public static EntityHandle FromNative(NativeApi.EntityHandle handle) => new(handle.Index, handle.Generation);

    public static Vector3 ToVector(NativeApi.Vec3 value) => new(value.X, value.Y, value.Z);

    public static NativeApi.Vec3 ToNative(Vector3 value) => new() { X = value.X, Y = value.Y, Z = value.Z };
}

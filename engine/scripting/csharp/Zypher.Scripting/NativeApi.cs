using System.Runtime.InteropServices;

namespace Zypher.Scripting;

/// <summary>
/// Blittable mirrors of the structs in <c>engine/include/zypher/scripting/ScriptApi.h</c>.
/// Keep these in sync with the C header; the ABI version guards mismatches.
/// </summary>
internal static class NativeApi
{
    public const uint AbiVersion = 1;

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec3
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vec3 Position;
        public Vec3 RotationEulerDegrees;
        public Vec3 Scale;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EntityHandle
    {
        public uint Index;
        public uint Generation;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Context
    {
        public uint StructSize;
        public IntPtr Host;
        public float DeltaSeconds;
        public IntPtr LogInfo;
        public IntPtr LogWarn;
        public IntPtr GetTransform;
        public IntPtr SetTransform;
        public IntPtr GetVelocity;
        public IntPtr SetVelocity;
        public IntPtr SetVelocityY;
        public IntPtr FindEntity;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Module
    {
        public uint AbiVersion;
        public IntPtr Name;
        public IntPtr CreateInstance;
        public IntPtr DestroyInstance;
        public IntPtr OnStart;
        public IntPtr OnUpdate;
        public IntPtr OnStop;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LogCallback(IntPtr host, IntPtr message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate Transform GetTransformCallback(IntPtr host, EntityHandle entity);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetTransformCallback(IntPtr host, EntityHandle entity, Transform transform);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate Vec3 GetVelocityCallback(IntPtr host, EntityHandle entity);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetVelocityCallback(IntPtr host, EntityHandle entity, Vec3 velocity);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void SetVelocityYCallback(IntPtr host, EntityHandle entity, float value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate EntityHandle FindEntityCallback(IntPtr host, IntPtr name);
}

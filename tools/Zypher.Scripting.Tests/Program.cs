// Exercises the C# gameplay layer without the engine: a fake native context
// stands in for the engine's C callbacks so behaviour logic can be verified on
// any machine with the .NET SDK installed.

using System.Runtime.InteropServices;
using System.Text;
using Zypher.Demo.Scripts;
using Zypher.Scripting;

namespace Zypher.Scripting.Tests;

internal static class Program
{
    private static int s_failures;
    private static int s_checks;

    private static void Check(bool condition, string description)
    {
        ++s_checks;
        if (!condition)
        {
            ++s_failures;
            Console.Error.WriteLine($"  FAIL {description}");
        }
    }

    private sealed class FakeWorld
    {
        public readonly Dictionary<(uint, uint), Vector> Transform = new();
        public readonly Dictionary<(uint, uint), Vector> Velocity = new();
        public readonly List<string> Logs = new();
        public float DeltaSeconds = 1f / 60f;

        public readonly struct Vector
        {
            public float X { get; init; }
            public float Y { get; init; }
            public float Z { get; init; }
        }
    }

    // Keeps the fake state alive while native-style function pointers are alive.
    private static FakeWorld s_world = new();

    private static void OnLog(IntPtr host, IntPtr message)
    {
        string text = Marshal.PtrToStringUTF8(message) ?? string.Empty;
        s_world.Logs.Add(text);
    }

    private static NativeApi.Transform GetTransform(IntPtr host, NativeApi.EntityHandle entity)
    {
        var key = (entity.Index, entity.Generation);
        FakeWorld.Vector value = s_world.Transform.TryGetValue(key, out var found)
            ? found
            : new FakeWorld.Vector { X = 0, Y = 0, Z = 0 };
        return new NativeApi.Transform
        {
            Position = new NativeApi.Vec3 { X = value.X, Y = value.Y, Z = value.Z },
            Scale = new NativeApi.Vec3 { X = 1, Y = 1, Z = 1 },
        };
    }

    private static void SetTransform(IntPtr host, NativeApi.EntityHandle entity, NativeApi.Transform transform)
    {
        s_world.Transform[(entity.Index, entity.Generation)] = new FakeWorld.Vector
        {
            X = transform.Position.X,
            Y = transform.Position.Y,
            Z = transform.Position.Z,
        };
    }

    private static NativeApi.Vec3 GetVelocity(IntPtr host, NativeApi.EntityHandle entity)
    {
        var key = (entity.Index, entity.Generation);
        if (!s_world.Velocity.TryGetValue(key, out var value))
        {
            return new NativeApi.Vec3();
        }
        return new NativeApi.Vec3 { X = value.X, Y = value.Y, Z = value.Z };
    }

    private static void SetVelocity(IntPtr host, NativeApi.EntityHandle entity, NativeApi.Vec3 velocity)
    {
        s_world.Velocity[(entity.Index, entity.Generation)] = new FakeWorld.Vector
        {
            X = velocity.X,
            Y = velocity.Y,
            Z = velocity.Z,
        };
    }

    private static void SetVelocityY(IntPtr host, NativeApi.EntityHandle entity, float y)
    {
        var key = (entity.Index, entity.Generation);
        s_world.Velocity.TryGetValue(key, out var value);
        s_world.Velocity[key] = new FakeWorld.Vector { X = value.X, Y = y, Z = value.Z };
    }

    private static NativeApi.EntityHandle FindEntity(IntPtr host, IntPtr name)
    {
        // Only one entity exists in this harness.
        return new NativeApi.EntityHandle { Index = 0, Generation = 0 };
    }

    private static readonly NativeApi.LogCallback s_log = OnLog;
    private static readonly NativeApi.LogCallback s_warn = OnLog;
    private static readonly NativeApi.GetTransformCallback s_getTransform = GetTransform;
    private static readonly NativeApi.SetTransformCallback s_setTransform = SetTransform;
    private static readonly NativeApi.GetVelocityCallback s_getVelocity = GetVelocity;
    private static readonly NativeApi.SetVelocityCallback s_setVelocity = SetVelocity;
    private static readonly NativeApi.SetVelocityYCallback s_setVelocityY = SetVelocityY;
    private static readonly NativeApi.FindEntityCallback s_findEntity = FindEntity;

    private static IntPtr AllocateContext(FakeWorld world)
    {
        var context = new NativeApi.Context
        {
            StructSize = (uint)Marshal.SizeOf<NativeApi.Context>(),
            Host = IntPtr.Zero,
            DeltaSeconds = world.DeltaSeconds,
            LogInfo = Marshal.GetFunctionPointerForDelegate(s_log),
            LogWarn = Marshal.GetFunctionPointerForDelegate(s_warn),
            GetTransform = Marshal.GetFunctionPointerForDelegate(s_getTransform),
            SetTransform = Marshal.GetFunctionPointerForDelegate(s_setTransform),
            GetVelocity = Marshal.GetFunctionPointerForDelegate(s_getVelocity),
            SetVelocity = Marshal.GetFunctionPointerForDelegate(s_setVelocity),
            SetVelocityY = Marshal.GetFunctionPointerForDelegate(s_setVelocityY),
            FindEntity = Marshal.GetFunctionPointerForDelegate(s_findEntity),
        };
        IntPtr pointer = Marshal.AllocHGlobal(Marshal.SizeOf<NativeApi.Context>());
        Marshal.StructureToPtr(context, pointer, false);
        return pointer;
    }

    private static int Main()
    {
        RunRegistryTests();
        RunPlayerControllerTest();
        RunSpinnerTest();
        RunMissingBehaviourTest();

        Console.WriteLine($"{s_checks} check(s), {s_failures} failure(s)");
        return s_failures == 0 ? 0 : 1;
    }

    private static void RunRegistryTests()
    {
        var registry = new ScriptRegistry();
        registry.Add<PlayerController>("PlayerController");
        registry.Add<Spinner>("Spinner");
        Check(registry.ClassNames.Count == 2, "registry holds two behaviours");
        Check(registry.ClassNames.Contains("PlayerController"), "registry contains PlayerController");
    }

    private static Behaviour CreateBehaviour(Type type, FakeWorld world, uint index = 0)
    {
        var behaviour = (Behaviour)Activator.CreateInstance(type)!;
        IntPtr contextPointer = AllocateContext(world);
        try
        {
            var nativeContext = Marshal.PtrToStructure<NativeApi.Context>(contextPointer);
            var context = new ScriptContext(nativeContext);
            typeof(Behaviour)
                .GetMethod("Attach", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!
                .Invoke(behaviour, new object[] { context, new EntityHandle(index, 0) });
        }
        finally
        {
            Marshal.FreeHGlobal(contextPointer);
        }
        return behaviour;
    }

    private static void RunPlayerControllerTest()
    {
        s_world = new FakeWorld();
        s_world.Transform[(0u, 0u)] = new FakeWorld.Vector { X = 0, Y = 0.5f, Z = 0 };
        var controller = (PlayerController)CreateBehaviour(typeof(PlayerController), s_world);
        controller.OnStart();
        Check(s_world.Logs.Count == 1, "PlayerController logs on start");

        for (int step = 0; step < 60; ++step)
        {
            controller.OnUpdate();
        }
        var position = s_world.Transform[(0u, 0u)];
        Check(position.X > 0.5f, "PlayerController walks along +X");
        Check(MathF.Abs(position.Y - 0.5f) < 1e-4f, "PlayerController does not teleport on Y");

        controller.OnStop();
        Check(s_world.Logs.Count == 2, "PlayerController logs on stop");
    }

    private static void RunSpinnerTest()
    {
        s_world = new FakeWorld();
        s_world.Transform[(0u, 0u)] = new FakeWorld.Vector { X = 1, Y = 2, Z = 3 };
        var spinner = (Spinner)CreateBehaviour(typeof(Spinner), s_world);
        spinner.OnUpdate();
        var moved = s_world.Transform[(0u, 0u)];
        Check(MathF.Abs(moved.X - 1f) > 1e-5f || MathF.Abs(moved.Z - 3f) > 1e-5f, "Spinner moves the entity");
        Check(MathF.Abs(moved.Y - 2f) < 1e-5f, "Spinner keeps Y constant");
    }

    private static void RunMissingBehaviourTest()
    {
        var registry = new ScriptRegistry();
        var method = typeof(ScriptRegistry).GetMethod("Create", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!;
        Check(method.Invoke(registry, new object[] { "Nope" }) == null, "unknown behaviour returns null");
    }
}

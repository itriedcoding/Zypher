namespace Zypher.Scripting;

/// <summary>
/// Handle to an entity owned by the engine.
/// </summary>
public readonly struct Entity
{
    internal Entity(ScriptContext context, EntityHandle handle)
    {
        Context = context;
        Handle = handle;
    }

    internal ScriptContext Context { get; }
    internal EntityHandle Handle { get; }

    public bool IsValid => Handle.IsValid;
    public uint Index => Handle.Index;

    public Vector3 Position
    {
        get => Context.GetPosition(Handle);
        set => Context.SetPosition(Handle, value);
    }

    public Vector3 Velocity
    {
        get => Context.GetVelocity(Handle);
        set => Context.SetVelocity(Handle, velocity: value);
    }

    public void Translate(Vector3 delta) => Context.Translate(Handle, delta);
}

/// <summary>
/// Base class for gameplay behaviours. Override the lifecycle methods and
/// register the concrete type with a <see cref="ScriptRegistry"/>.
/// </summary>
public abstract class Behaviour
{
    internal ScriptContext Context = null!;

    /// <summary>Entity this behaviour is attached to.</summary>
    public Entity Self => new(Context, Handle);

    internal EntityHandle Handle { get; private set; }

    /// <summary>Fixed timestep of the current simulation step.</summary>
    public float DeltaSeconds => Context.DeltaSeconds;

    public Vector3 Position
    {
        get => Context.GetPosition(Handle);
        set => Context.SetPosition(Handle, value);
    }

    public Vector3 Velocity
    {
        get => Context.GetVelocity(Handle);
        set => Context.SetVelocity(Handle, value);
    }

    // Lifecycle ------------------------------------------------------------

    public virtual void OnStart()
    {
    }

    public virtual void OnUpdate()
    {
    }

    public virtual void OnStop()
    {
    }

    // Helpers --------------------------------------------------------------

    protected void Log(string message) => Context.Log(message);

    protected void Warn(string message) => Context.Warn(message);

    /// <summary>Finds another entity by its scene name.</summary>
    protected Entity FindEntity(string name)
    {
        EntityHandle handle = Context.FindEntity(name);
        return new Entity(Context, handle);
    }

    internal void Attach(ScriptContext context, EntityHandle handle)
    {
        Context = context;
        Handle = handle;
    }
}

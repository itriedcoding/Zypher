using Zypher.Scripting;

namespace Zypher.Demo.Scripts;

/// <summary>
/// Moves the entity in a small circle. Used to verify script attach/update/stop
/// behaviour without needing player input.
/// </summary>
public sealed class Spinner : Behaviour
{
    private float _angle;

    public override void OnStart()
    {
        Log("Spinner started");
    }

    public override void OnUpdate()
    {
        _angle += DeltaSeconds * 2.0f;
        Vector3 basePosition = Position;
        Position = new Vector3(
            basePosition.X + MathF.Cos(_angle) * DeltaSeconds,
            basePosition.Y,
            basePosition.Z + MathF.Sin(_angle) * DeltaSeconds);
    }
}

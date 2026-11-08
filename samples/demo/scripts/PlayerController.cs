using Zypher.Scripting;

namespace Zypher.Demo.Scripts;

/// <summary>
/// Walks the player back and forth along X and hops whenever it is grounded.
/// Deterministic and input free, so it doubles as a runtime smoke test: the
/// engine report checksum changes with every simulation step.
/// </summary>
public sealed class PlayerController : Behaviour
{
    private const float Speed = 1.2f;
    private const float HopSpeed = 3.4f;
    private const float Range = 3.4f;

    private int _direction = 1;
    private float _groundedTime;
    private bool _started;

    public override void OnStart()
    {
        Log($"PlayerController started at {Position}");
        _started = true;
    }

    public override void OnUpdate()
    {
        if (!_started)
        {
            return;
        }

        Vector3 position = Position;
        float nextX = position.X + Speed * _direction * DeltaSeconds;
        if (nextX > Range)
        {
            nextX = Range;
            _direction = -1;
        }
        else if (nextX < -Range)
        {
            nextX = -Range;
            _direction = 1;
        }

        position = new Vector3(nextX, position.Y, position.Z);
        Position = position;

        // Hop every half second when standing on the ground.
        Vector3 velocity = Velocity;
        bool grounded = MathF.Abs(velocity.Y) < 0.1f && position.Y < 1.2f;
        if (grounded)
        {
            _groundedTime += DeltaSeconds;
            if (_groundedTime > 0.5f)
            {
                _groundedTime = 0f;
                var hop = new Vector3(velocity.X, HopSpeed, velocity.Z);
                Velocity = hop;
            }
        }
    }

    public override void OnStop()
    {
        Log("PlayerController stopped");
    }
}

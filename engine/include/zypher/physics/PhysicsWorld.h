// Deterministic rigid body simulation for axis aligned boxes.
//
// This is the built-in solver used when ZYPHER_WITH_JOLT is OFF (the default).
// It is intentionally narrow - boxes, gravity, restitution, static geometry -
// but it is exact and reproducible, which is what the runtime and the tests
// rely on. Enabling Jolt swaps the backend while keeping this interface.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/ecs/Registry.h"
#include "zypher/math/Geometry.h"

#include <vector>

namespace zy {

struct PhysicsSettings {
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    u32 solverIterations = 2;
    f32 fixedDelta = 1.0f / 60.0f;
    f32 linearDamping = 0.02f;
    f32 sleepSpeed = 0.02f;
};

class PhysicsWorld final : NonCopyable {
public:
    PhysicsWorld() = default;

    void configure(const PhysicsSettings& settings) { m_settings = settings; }
    [[nodiscard]] const PhysicsSettings& settings() const { return m_settings; }

    void reset();

    void addStaticBox(Entity entity, const Aabb& worldBounds);
    void addDynamicBody(Entity entity, const Vec3& halfExtents);

    // Advances the simulation and writes transforms back into the registry.
    void step(f32 deltaSeconds, Registry& registry);

    [[nodiscard]] usize bodyCount() const { return m_bodies.size(); }
    [[nodiscard]] usize staticCount() const { return m_staticBoxes.size(); }
    [[nodiscard]] u32 lastContactCount() const { return m_lastContacts; }
    [[nodiscard]] const std::vector<Entity>& bodyEntities() const;
    [[nodiscard]] Vec3 bodyVelocity(Entity entity) const;

private:
    struct StaticBox {
        Entity entity{};
        Aabb bounds{};
    };

    struct Body {
        Entity entity{};
        Vec3 halfExtents{0.5f, 0.5f, 0.5f};
        Vec3 velocity{};
        bool grounded = false;
    };

    PhysicsSettings m_settings{};
    std::vector<StaticBox> m_staticBoxes;
    std::vector<Body> m_bodies;
    u32 m_lastContacts = 0;
};

} // namespace zy

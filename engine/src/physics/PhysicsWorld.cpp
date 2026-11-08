#include "zypher/physics/PhysicsWorld.h"

#include "zypher/core/Assert.h"
#include "zypher/ecs/Components.h"

#include <algorithm>
#include <cmath>

namespace zy {
namespace {

struct Resolution {
    bool hit = false;
    usize axis = 0;
    f32 penetration = 0.0f;
    f32 direction = 0.0f; // +1 when the body is above/right/front of the obstacle
};

// Minimum translation vector between two overlapping AABBs.
[[nodiscard]] Resolution resolveAabb(const Aabb& body, const Aabb& obstacle) {
    Resolution result;
    if (!body.overlaps(obstacle)) {
        return result;
    }
    const Vec3 overlap = zy::min(body.max, obstacle.max) - zy::max(body.min, obstacle.min);
    usize axis = 0;
    if (overlap.y < overlap[axis]) {
        axis = 1;
    }
    if (overlap.z < overlap[axis]) {
        axis = 2;
    }
    result.hit = true;
    result.axis = axis;
    result.penetration = overlap[axis];
    result.direction = (body.center()[axis] >= obstacle.center()[axis]) ? 1.0f : -1.0f;
    return result;
}

} // namespace

const std::vector<Entity>& PhysicsWorld::bodyEntities() const {
    static thread_local std::vector<Entity> entities;
    entities.clear();
    entities.reserve(m_bodies.size());
    for (const auto& body : m_bodies) {
        entities.push_back(body.entity);
    }
    return entities;
}

Vec3 PhysicsWorld::bodyVelocity(Entity entity) const {
    for (const auto& body : m_bodies) {
        if (body.entity == entity) {
            return body.velocity;
        }
    }
    return Vec3::zero();
}

void PhysicsWorld::reset() {
    m_staticBoxes.clear();
    m_bodies.clear();
    m_lastContacts = 0;
}

void PhysicsWorld::addStaticBox(Entity entity, const Aabb& worldBounds) {
    m_staticBoxes.push_back(StaticBox{entity, worldBounds});
}

void PhysicsWorld::addDynamicBody(Entity entity, const Vec3& halfExtents) {
    Body body;
    body.entity = entity;
    body.halfExtents = halfExtents;
    m_bodies.push_back(body);
}

void PhysicsWorld::step(f32 deltaSeconds, Registry& registry) {
    if (deltaSeconds <= 0.0f || m_bodies.empty()) {
        m_lastContacts = 0;
        return;
    }
    const f32 delta = deltaSeconds;
    const f32 damping = std::max(0.0f, 1.0f - m_settings.linearDamping * delta);
    m_lastContacts = 0;

    for (auto& body : m_bodies) {
        if (!registry.alive(body.entity)) {
            continue;
        }
        auto* transform = registry.tryGet<Transform>(body.entity);
        const auto* rigidBody = registry.tryGet<RigidBody>(body.entity);
        if (transform == nullptr || rigidBody == nullptr) {
            continue;
        }

        body.velocity = rigidBody->velocity;
        body.grounded = false;

        // Integrate velocity and position.
        body.velocity += m_settings.gravity * delta;
        body.velocity *= damping;
        transform->position += body.velocity * delta;

        Aabb bounds = Aabb::fromCenterExtents(transform->position, body.halfExtents);

        for (u32 iteration = 0; iteration < std::max(1u, m_settings.solverIterations); ++iteration) {
            for (const auto& obstacle : m_staticBoxes) {
                const Resolution resolution = resolveAabb(bounds, obstacle.bounds);
                if (!resolution.hit) {
                    continue;
                }
                Vec3 correction{};
                correction[resolution.axis] = resolution.direction * resolution.penetration;
                bounds.min += correction;
                bounds.max += correction;
                transform->position += correction;
                ++m_lastContacts;

                if (resolution.axis == 1 && resolution.direction > 0.0f) {
                    body.grounded = true;
                }
                const f32 normalVelocity = body.velocity[resolution.axis];
                const bool movingInto = normalVelocity * resolution.direction < 0.0f;
                if (movingInto) {
                    body.velocity[resolution.axis] =
                        -normalVelocity * rigidBody->restitution;
                }
            }
        }

        // Dynamic vs dynamic: simple equal resolution using inverse masses.
        for (auto& other : m_bodies) {
            if (&other == &body || !registry.alive(other.entity)) {
                continue;
            }
            if (other.entity.index < body.entity.index) {
                continue; // resolve each pair once, in a stable order
            }
            auto* otherTransform = registry.tryGet<Transform>(other.entity);
            const auto* otherRigidBody = registry.tryGet<RigidBody>(other.entity);
            if (otherTransform == nullptr || otherRigidBody == nullptr) {
                continue;
            }
            const Aabb otherBounds = Aabb::fromCenterExtents(otherTransform->position, other.halfExtents);
            const Resolution resolution = resolveAabb(bounds, otherBounds);
            if (!resolution.hit) {
                continue;
            }
            ++m_lastContacts;
            const f32 totalInverseMass = rigidBody->inverseMass + otherRigidBody->inverseMass;
            if (totalInverseMass <= 0.0f) {
                continue;
            }
            const f32 bodyShare = rigidBody->inverseMass / totalInverseMass;
            const f32 otherShare = otherRigidBody->inverseMass / totalInverseMass;
            Vec3 correction{};
            correction[resolution.axis] = resolution.direction * resolution.penetration;
            transform->position -= correction * bodyShare;
            otherTransform->position += correction * otherShare;
            bounds = Aabb::fromCenterExtents(transform->position, body.halfExtents);

            if (body.velocity[resolution.axis] < other.velocity[resolution.axis]) {
                const f32 average = (body.velocity[resolution.axis] + other.velocity[resolution.axis]) * 0.5f;
                body.velocity[resolution.axis] = average;
                other.velocity[resolution.axis] = average;
            }
        }

        if (body.velocity.lengthSquared() < m_settings.sleepSpeed * m_settings.sleepSpeed &&
            body.grounded) {
            body.velocity = Vec3::zero();
        }

        if (auto* storedBody = registry.tryGet<RigidBody>(body.entity); storedBody != nullptr) {
            storedBody->velocity = body.velocity;
            storedBody->grounded = body.grounded;
        }
    }
}

} // namespace zy

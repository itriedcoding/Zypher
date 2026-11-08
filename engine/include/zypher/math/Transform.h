// Scene transform: position / rotation / scale with a cached matrix.
#pragma once

#include "zypher/math/Matrix.h"

namespace zy {

struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] static constexpr Transform identity() { return Transform{}; }

    [[nodiscard]] Mat4 matrix() const { return fromTRS(position, rotation, scale); }

    [[nodiscard]] Vec3 applyPoint(const Vec3& point) const {
        return position + rotation.rotate(Vec3{point.x * scale.x, point.y * scale.y, point.z * scale.z});
    }

    [[nodiscard]] Vec3 applyDirection(const Vec3& direction) const { return rotation.rotate(direction); }

    [[nodiscard]] Vec3 forward() const { return rotation.rotate(Vec3::forward()); }
    [[nodiscard]] Vec3 up() const { return rotation.rotate(Vec3::up()); }
    [[nodiscard]] Vec3 right() const { return rotation.rotate(Vec3::right()); }

    // Compose child after parent.
    [[nodiscard]] Transform combined(const Transform& child) const {
        Transform result;
        result.position = applyPoint(child.position);
        result.rotation = (rotation * child.rotation).normalized();
        result.scale = scale * child.scale;
        return result;
    }

    [[nodiscard]] Mat4 inverseMatrix() const { return matrix().affineInverted(); }
};

} // namespace zy

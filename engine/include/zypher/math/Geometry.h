// Axis aligned bounding boxes, rays and frustum helpers.
#pragma once

#include "zypher/math/Matrix.h"

#include <cmath>
#include <limits>

namespace zy {

struct Aabb {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};

    constexpr Aabb() = default;
    constexpr Aabb(const Vec3& minPoint, const Vec3& maxPoint) : min(minPoint), max(maxPoint) {}

    [[nodiscard]] static constexpr Aabb fromCenterExtents(const Vec3& center, const Vec3& halfExtents) {
        return Aabb{center - halfExtents, center + halfExtents};
    }

    [[nodiscard]] static Aabb inverted() {
        const f32 highest = std::numeric_limits<f32>::max();
        const f32 lowest = std::numeric_limits<f32>::lowest();
        return Aabb{Vec3{highest, highest, highest}, Vec3{lowest, lowest, lowest}};
    }

    [[nodiscard]] constexpr Vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] constexpr Vec3 halfExtents() const { return (max - min) * 0.5f; }
    [[nodiscard]] constexpr Vec3 size() const { return max - min; }
    [[nodiscard]] constexpr f32 volume() const {
        const Vec3 extents = size();
        return extents.x * extents.y * extents.z;
    }

    [[nodiscard]] constexpr bool isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    void expand(const Vec3& point) {
        min = zy::min(min, point);
        max = zy::max(max, point);
    }

    void expand(const Aabb& other) {
        min = zy::min(min, other.min);
        max = zy::max(max, other.max);
    }

    void expandBy(f32 margin) {
        min -= Vec3{margin, margin, margin};
        max += Vec3{margin, margin, margin};
    }

    [[nodiscard]] constexpr bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] constexpr bool overlaps(const Aabb& other) const {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y &&
               max.y >= other.min.y && min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] constexpr Aabb translated(const Vec3& offset) const {
        return Aabb{min + offset, max + offset};
    }

    [[nodiscard]] constexpr Aabb transformed(const Mat4& matrix) const {
        Aabb result = Aabb::inverted();
        const Vec3 corners[8] = {
            Vec3{min.x, min.y, min.z}, Vec3{max.x, min.y, min.z}, Vec3{min.x, max.y, min.z},
            Vec3{max.x, max.y, min.z}, Vec3{min.x, min.y, max.z}, Vec3{max.x, min.y, max.z},
            Vec3{min.x, max.y, max.z}, Vec3{max.x, max.y, max.z},
        };
        for (const auto& corner : corners) {
            result.expand(matrix.transformPoint(corner));
        }
        return result;
    }
};

struct Ray {
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    [[nodiscard]] constexpr Vec3 at(f32 distance) const { return origin + direction * distance; }
};

// Slab test. Returns true and writes the entry distance when the ray hits.
[[nodiscard]] inline bool intersectRayAabb(const Ray& ray, const Aabb& box, f32& distance,
                                           f32 maxDistance = std::numeric_limits<f32>::max()) {
    f32 tMinimum = 0.0f;
    f32 tMaximum = maxDistance;
    for (usize axis = 0; axis < 3; ++axis) {
        const f32 origin = ray.origin[axis];
        const f32 direction = ray.direction[axis];
        if (std::fabs(direction) < 1.0e-8f) {
            if (origin < box.min[axis] || origin > box.max[axis]) {
                return false;
            }
            continue;
        }
        const f32 inverse = 1.0f / direction;
        f32 near = (box.min[axis] - origin) * inverse;
        f32 far = (box.max[axis] - origin) * inverse;
        if (near > far) {
            const f32 temporary = near;
            near = far;
            far = temporary;
        }
        tMinimum = near > tMinimum ? near : tMinimum;
        tMaximum = far < tMaximum ? far : tMaximum;
        if (tMinimum > tMaximum) {
            return false;
        }
    }
    distance = tMinimum;
    return true;
}

} // namespace zy

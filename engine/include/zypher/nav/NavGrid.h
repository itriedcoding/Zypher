// Grid navigation with A* path finding.
//
// The built-in implementation covers the runtime's needs (walkable grid, A*
// with 8 neighbours, string pulling). Recast/Detour can be enabled with
// ZYPHER_WITH_RECAST for full navmesh baking.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/math/Geometry.h"
#include "zypher/math/Vec.h"

#include <vector>

namespace zy {

class NavGrid final : NonCopyable {
public:
    NavGrid(u32 width, u32 height, f32 cellSize);

    [[nodiscard]] u32 width() const { return m_width; }
    [[nodiscard]] u32 height() const { return m_height; }
    [[nodiscard]] f32 cellSize() const { return m_cellSize; }

    [[nodiscard]] bool walkable(u32 x, u32 y) const;
    void setWalkable(u32 x, u32 y, bool walkable);
    void blockRect(u32 x, u32 y, u32 width, u32 height);
    void blockWorldAabb(const Aabb& bounds);
    void clear();

    [[nodiscard]] Vec2 cellToWorld(u32 x, u32 y) const;
    [[nodiscard]] Vec2 worldToCell(const Vec2& world) const;

    // Returns way points (world space, smoothed) from start to end. Empty when
    // no path exists.
    [[nodiscard]] std::vector<Vec2> findPath(const Vec2& start, const Vec2& end) const;

    [[nodiscard]] bool hasLineOfSight(const Vec2& from, const Vec2& to) const;
    [[nodiscard]] u32 lastExpandedNodes() const { return m_lastExpandedNodes; }

private:
    [[nodiscard]] u32 indexOf(u32 x, u32 y) const { return y * m_width + x; }
    [[nodiscard]] bool inBounds(i32 x, i32 y) const {
        return x >= 0 && y >= 0 && static_cast<u32>(x) < m_width && static_cast<u32>(y) < m_height;
    }

    u32 m_width = 0;
    u32 m_height = 0;
    f32 m_cellSize = 1.0f;
    std::vector<u8> m_walkable;
    mutable u32 m_lastExpandedNodes = 0;
};

} // namespace zy

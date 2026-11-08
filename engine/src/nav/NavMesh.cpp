#include "zypher/nav/NavGrid.h"

#include "zypher/core/Assert.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

namespace zy {

NavGrid::NavGrid(u32 width, u32 height, f32 cellSize)
    : m_width(std::max(1u, width)), m_height(std::max(1u, height)),
      m_cellSize(cellSize > 0.0f ? cellSize : 1.0f),
      m_walkable(static_cast<usize>(m_width) * m_height, 1) {}

bool NavGrid::walkable(u32 x, u32 y) const {
    if (x >= m_width || y >= m_height) {
        return false;
    }
    return m_walkable[indexOf(x, y)] != 0;
}

void NavGrid::setWalkable(u32 x, u32 y, bool walkableValue) {
    if (x >= m_width || y >= m_height) {
        return;
    }
    m_walkable[indexOf(x, y)] = walkableValue ? 1 : 0;
}

void NavGrid::blockRect(u32 x, u32 y, u32 width, u32 height) {
    for (u32 row = y; row < y + height && row < m_height; ++row) {
        for (u32 column = x; column < x + width && column < m_width; ++column) {
            m_walkable[indexOf(column, row)] = 0;
        }
    }
}

void NavGrid::blockWorldAabb(const Aabb& bounds) {
    const Vec2 minimum = worldToCell(Vec2{bounds.min.x, bounds.min.z});
    const Vec2 maximum = worldToCell(Vec2{bounds.max.x, bounds.max.z});
    const i32 minX = std::max(0, static_cast<i32>(std::floor(minimum.x)));
    const i32 minY = std::max(0, static_cast<i32>(std::floor(minimum.y)));
    const i32 maxX = std::min(static_cast<i32>(m_width) - 1, static_cast<i32>(std::ceil(maximum.x)));
    const i32 maxY = std::min(static_cast<i32>(m_height) - 1, static_cast<i32>(std::ceil(maximum.y)));
    for (i32 row = minY; row <= maxY; ++row) {
        for (i32 column = minX; column <= maxX; ++column) {
            m_walkable[indexOf(static_cast<u32>(column), static_cast<u32>(row))] = 0;
        }
    }
}

void NavGrid::clear() {
    std::fill(m_walkable.begin(), m_walkable.end(), static_cast<u8>(1));
}

Vec2 NavGrid::cellToWorld(u32 x, u32 y) const {
    return Vec2{(static_cast<f32>(x) + 0.5f) * m_cellSize, (static_cast<f32>(y) + 0.5f) * m_cellSize};
}

Vec2 NavGrid::worldToCell(const Vec2& world) const {
    return Vec2{world.x / m_cellSize, world.y / m_cellSize};
}

bool NavGrid::hasLineOfSight(const Vec2& from, const Vec2& to) const {
    const Vec2 start = worldToCell(from);
    const Vec2 end = worldToCell(to);
    i32 x0 = static_cast<i32>(std::floor(start.x));
    i32 y0 = static_cast<i32>(std::floor(start.y));
    const i32 x1 = static_cast<i32>(std::floor(end.x));
    const i32 y1 = static_cast<i32>(std::floor(end.y));

    const i32 dx = std::abs(x1 - x0);
    const i32 dy = std::abs(y1 - y0);
    const i32 stepX = x0 < x1 ? 1 : -1;
    const i32 stepY = y0 < y1 ? 1 : -1;
    i32 error = dx - dy;

    for (;;) {
        if (!inBounds(x0, y0) || !walkable(static_cast<u32>(x0), static_cast<u32>(y0))) {
            return false;
        }
        if (x0 == x1 && y0 == y1) {
            return true;
        }
        const i32 doubled = error * 2;
        if (doubled > -dy) {
            error -= dy;
            x0 += stepX;
        }
        if (doubled < dx) {
            error += dx;
            y0 += stepY;
        }
    }
}

std::vector<Vec2> NavGrid::findPath(const Vec2& start, const Vec2& end) const {
    m_lastExpandedNodes = 0;
    const Vec2 startCell = worldToCell(start);
    const Vec2 endCell = worldToCell(end);
    const i32 startX = static_cast<i32>(std::floor(startCell.x));
    const i32 startY = static_cast<i32>(std::floor(startCell.y));
    const i32 goalX = static_cast<i32>(std::floor(endCell.x));
    const i32 goalY = static_cast<i32>(std::floor(endCell.y));

    if (!inBounds(startX, startY) || !inBounds(goalX, goalY)) {
        return {};
    }
    if (!walkable(static_cast<u32>(goalX), static_cast<u32>(goalY))) {
        return {};
    }

    const usize cellCount = static_cast<usize>(m_width) * m_height;
    constexpr f32 kInfinity = std::numeric_limits<f32>::max();
    std::vector<f32> cost(cellCount, kInfinity);
    std::vector<i32> previous(cellCount, -1);
    std::vector<u8> closed(cellCount, 0);

    struct Node {
        f32 estimatedTotal = 0.0f;
        u32 index = 0;
        bool operator>(const Node& other) const { return estimatedTotal > other.estimatedTotal; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;

    const auto heuristic = [goalX, goalY](i32 x, i32 y) {
        const f32 dx = static_cast<f32>(std::abs(x - goalX));
        const f32 dy = static_cast<f32>(std::abs(y - goalY));
        // Octile distance, admissible for 8-connected grids.
        return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
    };

    const u32 startIndex = indexOf(static_cast<u32>(startX), static_cast<u32>(startY));
    cost[startIndex] = 0.0f;
    open.push(Node{heuristic(startX, startY), startIndex});

    constexpr i32 kOffsets[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    while (!open.empty()) {
        const Node current = open.top();
        open.pop();
        if (closed[current.index] != 0) {
            continue;
        }
        closed[current.index] = 1;
        ++m_lastExpandedNodes;

        const i32 currentX = static_cast<i32>(current.index % m_width);
        const i32 currentY = static_cast<i32>(current.index / m_width);
        if (currentX == goalX && currentY == goalY) {
            break;
        }

        for (const auto& offset : kOffsets) {
            const i32 nextX = currentX + offset[0];
            const i32 nextY = currentY + offset[1];
            if (!inBounds(nextX, nextY)) {
                continue;
            }
            if (!walkable(static_cast<u32>(nextX), static_cast<u32>(nextY))) {
                continue;
            }
            const bool diagonal = offset[0] != 0 && offset[1] != 0;
            if (diagonal) {
                // Prevent cutting through blocked corners.
                if (!walkable(static_cast<u32>(currentX + offset[0]), static_cast<u32>(currentY)) ||
                    !walkable(static_cast<u32>(currentX), static_cast<u32>(currentY + offset[1]))) {
                    continue;
                }
            }
            const u32 nextIndex = indexOf(static_cast<u32>(nextX), static_cast<u32>(nextY));
            if (closed[nextIndex] != 0) {
                continue;
            }
            const f32 stepCost = diagonal ? 1.41421356f : 1.0f;
            const f32 candidate = cost[current.index] + stepCost;
            if (candidate < cost[nextIndex]) {
                cost[nextIndex] = candidate;
                previous[nextIndex] = static_cast<i32>(current.index);
                open.push(Node{candidate + heuristic(nextX, nextY), nextIndex});
            }
        }
    }

    const u32 goalIndex = indexOf(static_cast<u32>(goalX), static_cast<u32>(goalY));
    if (previous[goalIndex] < 0 && goalIndex != startIndex) {
        return {};
    }

    std::vector<u32> cells;
    for (i32 index = static_cast<i32>(goalIndex); index >= 0; index = previous[static_cast<usize>(index)]) {
        cells.push_back(static_cast<u32>(index));
        if (static_cast<u32>(index) == startIndex) {
            break;
        }
    }
    std::reverse(cells.begin(), cells.end());

    std::vector<Vec2> points;
    points.reserve(cells.size());
    for (const u32 index : cells) {
        points.push_back(cellToWorld(index % m_width, index / m_width));
    }
    if (points.empty()) {
        return {};
    }
    points.front() = start;
    points.back() = end;

    // String pulling: skip way points when the segment is unobstructed.
    std::vector<Vec2> smoothed;
    smoothed.push_back(points.front());
    usize cursor = 0;
    while (cursor + 1 < points.size()) {
        usize furthest = cursor + 1;
        for (usize candidate = points.size() - 1; candidate > cursor + 1; --candidate) {
            if (hasLineOfSight(points[cursor], points[candidate])) {
                furthest = candidate;
                break;
            }
        }
        smoothed.push_back(points[furthest]);
        cursor = furthest;
    }
    return smoothed;
}

} // namespace zy

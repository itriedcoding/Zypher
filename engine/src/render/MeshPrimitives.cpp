#include "zypher/render/MeshPrimitives.h"

#include "zypher/math/Math.h"

#include <cmath>

namespace zy {
namespace {

void addQuad(MeshData& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
             const Vec3& normal, const Vec2& uvA, const Vec2& uvB, const Vec2& uvC, const Vec2& uvD) {
    const u32 base = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{a, normal, uvA});
    mesh.vertices.push_back(Vertex{b, normal, uvB});
    mesh.vertices.push_back(Vertex{c, normal, uvC});
    mesh.vertices.push_back(Vertex{d, normal, uvD});
    mesh.indices.insert(mesh.indices.end(),
                        {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

MeshData makeCubeMesh(f32 size) {
    const f32 half = size * 0.5f;
    MeshData mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    const Vec2 uv0{0.0f, 0.0f};
    const Vec2 uv1{1.0f, 0.0f};
    const Vec2 uv2{1.0f, 1.0f};
    const Vec2 uv3{0.0f, 1.0f};

    // +X
    addQuad(mesh, {half, -half, half}, {half, -half, -half}, {half, half, -half}, {half, half, half},
            {1.0f, 0.0f, 0.0f}, uv0, uv1, uv2, uv3);
    // -X
    addQuad(mesh, {-half, -half, -half}, {-half, -half, half}, {-half, half, half}, {-half, half, -half},
            {-1.0f, 0.0f, 0.0f}, uv0, uv1, uv2, uv3);
    // +Y
    addQuad(mesh, {-half, half, half}, {half, half, half}, {half, half, -half}, {-half, half, -half},
            {0.0f, 1.0f, 0.0f}, uv0, uv1, uv2, uv3);
    // -Y
    addQuad(mesh, {-half, -half, -half}, {half, -half, -half}, {half, -half, half}, {-half, -half, half},
            {0.0f, -1.0f, 0.0f}, uv0, uv1, uv2, uv3);
    // +Z
    addQuad(mesh, {-half, -half, half}, {half, -half, half}, {half, half, half}, {-half, half, half},
            {0.0f, 0.0f, 1.0f}, uv0, uv1, uv2, uv3);
    // -Z
    addQuad(mesh, {half, -half, -half}, {-half, -half, -half}, {-half, half, -half}, {half, half, -half},
            {0.0f, 0.0f, -1.0f}, uv0, uv1, uv2, uv3);
    return mesh;
}

MeshData makePlaneMesh(f32 size, u32 subdivisions) {
    const u32 steps = subdivisions < 1 ? 1 : subdivisions;
    MeshData mesh;
    mesh.vertices.reserve(static_cast<usize>(steps + 1) * (steps + 1));
    mesh.indices.reserve(static_cast<usize>(steps) * steps * 6);

    const f32 half = size * 0.5f;
    for (u32 row = 0; row <= steps; ++row) {
        const f32 v = static_cast<f32>(row) / static_cast<f32>(steps);
        for (u32 column = 0; column <= steps; ++column) {
            const f32 u = static_cast<f32>(column) / static_cast<f32>(steps);
            mesh.vertices.push_back(Vertex{
                Vec3{-half + u * size, 0.0f, -half + v * size}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{u, v}});
        }
    }
    const u32 stride = steps + 1;
    for (u32 row = 0; row < steps; ++row) {
        for (u32 column = 0; column < steps; ++column) {
            const u32 a = row * stride + column;
            const u32 b = a + 1;
            const u32 c = a + stride;
            const u32 d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }
    return mesh;
}

MeshData makeUvSphereMesh(f32 radius, u32 segments, u32 rings) {
    const u32 segmentCount = segments < 3 ? 3 : segments;
    const u32 ringCount = rings < 2 ? 2 : rings;

    MeshData mesh;
    mesh.vertices.reserve(static_cast<usize>(ringCount + 1) * (segmentCount + 1));
    for (u32 ring = 0; ring <= ringCount; ++ring) {
        const f32 v = static_cast<f32>(ring) / static_cast<f32>(ringCount);
        const f32 phi = v * math::Pi;
        for (u32 segment = 0; segment <= segmentCount; ++segment) {
            const f32 u = static_cast<f32>(segment) / static_cast<f32>(segmentCount);
            const f32 theta = u * math::Tau;
            const Vec3 normal{std::sin(phi) * std::cos(theta), std::cos(phi),
                              std::sin(phi) * std::sin(theta)};
            mesh.vertices.push_back(Vertex{normal * radius, normal, Vec2{u, v}});
        }
    }
    const u32 stride = segmentCount + 1;
    for (u32 ring = 0; ring < ringCount; ++ring) {
        for (u32 segment = 0; segment < segmentCount; ++segment) {
            const u32 a = ring * stride + segment;
            const u32 b = a + 1;
            const u32 c = a + stride;
            const u32 d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    return mesh;
}

} // namespace zy

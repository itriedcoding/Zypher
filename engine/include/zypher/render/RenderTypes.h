// Types shared by every rendering backend.
#pragma once

#include "zypher/math/Geometry.h"
#include "zypher/math/Vec.h"

#include <string>
#include <string_view>
#include <vector>

namespace zy {

struct Vertex {
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec2 uv{};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    [[nodiscard]] usize triangleCount() const { return indices.size() / 3; }
    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }

    [[nodiscard]] Aabb bounds() const {
        Aabb box = Aabb::inverted();
        for (const auto& vertex : vertices) {
            box.expand(vertex.position);
        }
        return box;
    }
};

struct Material {
    Vec4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    Vec3 emissive{0.0f, 0.0f, 0.0f};
};

struct CameraDesc {
    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    Vec3 position{0.0f, 2.0f, 6.0f};
};

struct ClearColor {
    f32 r = 0.06f;
    f32 g = 0.07f;
    f32 b = 0.09f;
    f32 a = 1.0f;
};

struct MeshHandle {
    u32 id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
    friend bool operator==(const MeshHandle&, const MeshHandle&) = default;
};

struct DeviceStats {
    u32 drawCalls = 0;
    u32 triangles = 0;
    u32 meshes = 0;
    f64 cpuFrameMilliseconds = 0.0;
};

enum class RenderBackend : u8 {
    Software = 0,
    Vulkan,
    Direct3D12,
    Metal,
    OpenGL,
};

[[nodiscard]] constexpr std::string_view renderBackendName(RenderBackend backend) {
    switch (backend) {
    case RenderBackend::Software: return "software";
    case RenderBackend::Vulkan: return "vulkan";
    case RenderBackend::Direct3D12: return "d3d12";
    case RenderBackend::Metal: return "metal";
    case RenderBackend::OpenGL: return "opengl";
    }
    return "unknown";
}

[[nodiscard]] std::optional<RenderBackend> renderBackendFromString(std::string_view name);

struct RenderDeviceDesc {
    u32 width = 1280;
    u32 height = 720;
    bool vsync = true;
    bool enableValidation = false;
    std::string applicationName = "Zypher";
};

struct DirectionalLight {
    Vec3 direction{-0.35f, -1.0f, -0.45f};
    Vec3 color{1.0f, 0.98f, 0.94f};
    f32 intensity = 1.15f;
};

} // namespace zy

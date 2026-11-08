#include "zypher/render/SoftwareDevice.h"

#include "zypher/core/Log.h"
#include "zypher/core/Platform.h"

#include <algorithm>
#include <cmath>

namespace zy {
namespace {

constexpr u32 kMaxDimension = 8192;
constexpr f32 kAmbient = 0.20f;

[[nodiscard]] f32 edgeFunction(f32 ax, f32 ay, f32 bx, f32 by, f32 px, f32 py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

[[nodiscard]] f32 gammaEncode(f32 value) {
    const f32 clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return std::pow(clamped, 1.0f / 2.2f);
}

[[nodiscard]] u8 toByte(f32 value) {
    const f32 scaled = value * 255.0f + 0.5f;
    return static_cast<u8>(scaled <= 0.0f ? 0.0f : (scaled >= 255.0f ? 255.0f : scaled));
}

} // namespace

Status SoftwareDevice::initialize(const RenderDeviceDesc& desc) {
    if (m_initialized) {
        return Error{ErrorCode::InvalidArgument, "software device already initialised"};
    }
    m_width = std::clamp(desc.width, 1u, kMaxDimension);
    m_height = std::clamp(desc.height, 1u, kMaxDimension);
    m_color.assign(static_cast<usize>(m_width) * m_height * 4, 0);
    m_depth.assign(static_cast<usize>(m_width) * m_height, 1.0f);
    m_initialized = true;
    ZY_LOG_INFO("render", "software rasteriser ready at ", m_width, "x", m_height);
    return ok();
}

void SoftwareDevice::shutdown() {
    m_meshes.clear();
    m_freeMeshSlots.clear();
    m_commands.clear();
    m_color.clear();
    m_depth.clear();
    m_initialized = false;
    m_frameActive = false;
}

Status SoftwareDevice::createMesh(const MeshData& mesh, MeshHandle& outHandle) {
    if (!m_initialized) {
        return Error{ErrorCode::InvalidArgument, "createMesh() before initialize()"};
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return Error{ErrorCode::InvalidArgument, "mesh has no geometry"};
    }
    if (mesh.indices.size() % 3 != 0) {
        return Error{ErrorCode::InvalidArgument, "mesh index count is not a multiple of three"};
    }
    const u32 vertexCount = static_cast<u32>(mesh.vertices.size());
    for (const u32 index : mesh.indices) {
        if (index >= vertexCount) {
            return Error{ErrorCode::InvalidArgument, "mesh index out of range"};
        }
    }

    u32 slot = 0;
    if (!m_freeMeshSlots.empty()) {
        slot = m_freeMeshSlots.back();
        m_freeMeshSlots.pop_back();
        m_meshes[slot] = mesh;
    } else {
        slot = static_cast<u32>(m_meshes.size());
        m_meshes.push_back(mesh);
    }
    outHandle = MeshHandle{slot + 1};
    return ok();
}

void SoftwareDevice::destroyMesh(MeshHandle handle) {
    if (!handle.valid() || handle.id > m_meshes.size()) {
        return;
    }
    const u32 slot = handle.id - 1;
    m_meshes[slot] = MeshData{};
    m_freeMeshSlots.push_back(slot);
}

void SoftwareDevice::beginFrame(const ClearColor& clearColor) {
    if (!m_initialized) {
        return;
    }
    m_clear = clearColor;
    m_commands.clear();
    m_stats = DeviceStats{};
    m_frameActive = true;

    const u8 red = toByte(gammaEncode(clearColor.r));
    const u8 green = toByte(gammaEncode(clearColor.g));
    const u8 blue = toByte(gammaEncode(clearColor.b));
    const u8 alpha = toByte(clearColor.a);
    for (usize index = 0; index < m_color.size(); index += 4) {
        m_color[index] = red;
        m_color[index + 1] = green;
        m_color[index + 2] = blue;
        m_color[index + 3] = alpha;
    }
    std::fill(m_depth.begin(), m_depth.end(), 1.0f);
}

void SoftwareDevice::setCamera(const CameraDesc& camera) { m_camera = camera; }

void SoftwareDevice::setLight(const DirectionalLight& light) { m_light = light; }

void SoftwareDevice::drawMesh(MeshHandle handle, const Mat4& model, const Material& material) {
    if (!m_frameActive || !handle.valid() || handle.id > m_meshes.size()) {
        return;
    }
    if (m_meshes[handle.id - 1].vertices.empty()) {
        return;
    }
    m_commands.push_back(DrawCommand{handle, model, material});
}

void SoftwareDevice::endFrame() {
    if (!m_frameActive) {
        return;
    }
    const CameraDesc camera = m_camera;
    const Mat4 viewProjection = camera.projection * camera.view;
    for (const auto& command : m_commands) {
        rasterizeCommand(command);
        m_stats.drawCalls += 1;
        m_stats.triangles +=
            static_cast<u32>(m_meshes[command.mesh.id - 1].indices.size() / 3);
    }
    m_stats.meshes = static_cast<u32>(m_meshes.size() - m_freeMeshSlots.size());
    m_frameActive = false;
    (void)viewProjection;
}

void SoftwareDevice::resize(u32 newWidth, u32 newHeight) {
    const u32 clampedWidth = std::clamp(newWidth, 1u, kMaxDimension);
    const u32 clampedHeight = std::clamp(newHeight, 1u, kMaxDimension);
    if (clampedWidth == m_width && clampedHeight == m_height) {
        return;
    }
    m_width = clampedWidth;
    m_height = clampedHeight;
    m_color.assign(static_cast<usize>(m_width) * m_height * 4, 0);
    m_depth.assign(static_cast<usize>(m_width) * m_height, 1.0f);
}

std::optional<std::vector<u8>> SoftwareDevice::readbackRgba() const {
    if (!m_initialized || m_color.empty()) {
        return std::nullopt;
    }
    return m_color;
}

void SoftwareDevice::rasterizeCommand(const DrawCommand& command) {
    const MeshData& mesh = m_meshes[command.mesh.id - 1];
    const Mat4 viewProjection = m_camera.projection * m_camera.view;
    const Mat4 mvp = viewProjection * command.model;
    const Mat4 normalMatrix = command.model.affineInverted().transposed();

    for (usize index = 0; index + 2 < mesh.indices.size(); index += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[index]];
        const Vertex& b = mesh.vertices[mesh.indices[index + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[index + 2]];
        drawTriangle(a, b, c, mvp, normalMatrix, command.material);
    }
}

void SoftwareDevice::drawTriangle(const Vertex& a, const Vertex& b, const Vertex& c, const Mat4& mvp,
                                  const Mat4& normalMatrix, const Material& material) {
    const Vec4 clipA = mvp.transform(Vec4{a.position, 1.0f});
    const Vec4 clipB = mvp.transform(Vec4{b.position, 1.0f});
    const Vec4 clipC = mvp.transform(Vec4{c.position, 1.0f});

    // Simple near plane rejection: a full clipper would split triangles. The
    // software device is used for verification and offline rendering where the
    // camera is authored to keep geometry in front of the near plane.
    constexpr f32 kNearEpsilon = 1.0e-4f;
    if (clipA.w <= kNearEpsilon || clipB.w <= kNearEpsilon || clipC.w <= kNearEpsilon) {
        return;
    }

    const f32 inverseWa = 1.0f / clipA.w;
    const f32 inverseWb = 1.0f / clipB.w;
    const f32 inverseWc = 1.0f / clipC.w;

    const f32 halfWidth = static_cast<f32>(m_width) * 0.5f;
    const f32 halfHeight = static_cast<f32>(m_height) * 0.5f;
    const auto toScreenX = [halfWidth](const Vec4& clip, f32 inverseW) {
        return (clip.x * inverseW * 0.5f + 0.5f) * halfWidth * 2.0f;
    };
    const auto toScreenY = [halfHeight](const Vec4& clip, f32 inverseW) {
        return (1.0f - (clip.y * inverseW * 0.5f + 0.5f)) * halfHeight * 2.0f;
    };

    const f32 ax = toScreenX(clipA, inverseWa);
    const f32 ay = toScreenY(clipA, inverseWa);
    const f32 bx = toScreenX(clipB, inverseWb);
    const f32 by = toScreenY(clipB, inverseWb);
    const f32 cx = toScreenX(clipC, inverseWc);
    const f32 cy = toScreenY(clipC, inverseWc);

    const f32 area = edgeFunction(ax, ay, bx, by, cx, cy);
    if (std::fabs(area) < 1.0e-6f) {
        return;
    }
    const f32 inverseArea = 1.0f / area;

    const f32 minXf = std::min({ax, bx, cx});
    const f32 maxXf = std::max({ax, bx, cx});
    const f32 minYf = std::min({ay, by, cy});
    const f32 maxYf = std::max({ay, by, cy});
    if (maxXf < 0.0f || maxYf < 0.0f || minXf >= static_cast<f32>(m_width) ||
        minYf >= static_cast<f32>(m_height)) {
        return;
    }
    const i32 minX = std::max(0, static_cast<i32>(std::floor(minXf)));
    const i32 maxX = std::min(static_cast<i32>(m_width) - 1, static_cast<i32>(std::ceil(maxXf)));
    const i32 minY = std::max(0, static_cast<i32>(std::floor(minYf)));
    const i32 maxY = std::min(static_cast<i32>(m_height) - 1, static_cast<i32>(std::ceil(maxYf)));

    const Vec3 lightDirection = normalize(-m_light.direction);
    const Vec3 normalA = normalize(normalMatrix.transformDirection(a.normal));
    const Vec3 normalB = normalize(normalMatrix.transformDirection(b.normal));
    const Vec3 normalC = normalize(normalMatrix.transformDirection(c.normal));

    const Vec3 viewDirection = normalize(m_camera.position);

    for (i32 y = minY; y <= maxY; ++y) {
        for (i32 x = minX; x <= maxX; ++x) {
            const f32 px = static_cast<f32>(x) + 0.5f;
            const f32 py = static_cast<f32>(y) + 0.5f;
            const f32 weightA = edgeFunction(bx, by, cx, cy, px, py) * inverseArea;
            const f32 weightB = edgeFunction(cx, cy, ax, ay, px, py) * inverseArea;
            const f32 weightC = edgeFunction(ax, ay, bx, by, px, py) * inverseArea;
            // Accept both winding orders; back faces are lit two sided.
            const bool inside = (weightA >= 0.0f && weightB >= 0.0f && weightC >= 0.0f) ||
                                (weightA <= 0.0f && weightB <= 0.0f && weightC <= 0.0f);
            if (!inside) {
                continue;
            }

            const f32 depth = weightA * (clipA.z * inverseWa) + weightB * (clipB.z * inverseWb) +
                              weightC * (clipC.z * inverseWc);
            if (depth < 0.0f || depth > 1.0f) {
                continue;
            }
            const usize pixelIndex = static_cast<usize>(y) * m_width + static_cast<usize>(x);
            if (depth >= m_depth[pixelIndex]) {
                continue;
            }

            const f32 interpolatedInverseW =
                weightA * inverseWa + weightB * inverseWb + weightC * inverseWc;
            f32 normalX = 0.0f;
            f32 normalY = 0.0f;
            f32 normalZ = 0.0f;
            if (std::fabs(interpolatedInverseW) > 1.0e-8f) {
                normalX = (weightA * normalA.x * inverseWa + weightB * normalB.x * inverseWb +
                           weightC * normalC.x * inverseWc) /
                          interpolatedInverseW;
                normalY = (weightA * normalA.y * inverseWa + weightB * normalB.y * inverseWb +
                           weightC * normalC.y * inverseWc) /
                          interpolatedInverseW;
                normalZ = (weightA * normalA.z * inverseWa + weightB * normalB.z * inverseWb +
                           weightC * normalC.z * inverseWc) /
                          interpolatedInverseW;
            }
            Vec3 normal = normalize(Vec3{normalX, normalY, normalZ});
            if (dot(normal, viewDirection) < 0.0f) {
                normal = -normal;
            }

            const f32 diffuse = std::max(0.0f, dot(normal, lightDirection)) * m_light.intensity;
            const f32 red = material.baseColor.x * (kAmbient + diffuse) * m_light.color.x +
                            material.emissive.x;
            const f32 green = material.baseColor.y * (kAmbient + diffuse) * m_light.color.y +
                              material.emissive.y;
            const f32 blue = material.baseColor.z * (kAmbient + diffuse) * m_light.color.z +
                             material.emissive.z;
            writePixel(static_cast<u32>(x), static_cast<u32>(y), depth, red, green, blue);
        }
    }
}

void SoftwareDevice::writePixel(u32 x, u32 y, f32 depth, f32 red, f32 green, f32 blue) {
    const usize pixelIndex = static_cast<usize>(y) * m_width + x;
    m_depth[pixelIndex] = depth;
    m_color[pixelIndex * 4 + 0] = toByte(gammaEncode(red));
    m_color[pixelIndex * 4 + 1] = toByte(gammaEncode(green));
    m_color[pixelIndex * 4 + 2] = toByte(gammaEncode(blue));
    m_color[pixelIndex * 4 + 3] = 255;
}

} // namespace zy

// Software rasteriser.
//
// A complete, deterministic rendering backend used for headless verification,
// servers, the editor's thumbnail service and machines without a usable GPU.
// It implements the same RenderDevice contract as the hardware backends:
// deferred draw list, depth buffer, perspective correct interpolation and
// directional lambert lighting.
#pragma once

#include "zypher/render/RenderDevice.h"

#include <vector>

namespace zy {

class SoftwareDevice final : public RenderDevice {
public:
    SoftwareDevice() = default;
    ~SoftwareDevice() override = default;

    [[nodiscard]] std::string_view name() const override { return "Zypher Software Rasterizer"; }
    [[nodiscard]] RenderBackend backend() const override { return RenderBackend::Software; }

    [[nodiscard]] Status initialize(const RenderDeviceDesc& desc) override;
    void shutdown() override;

    [[nodiscard]] Status createMesh(const MeshData& mesh, MeshHandle& outHandle) override;
    void destroyMesh(MeshHandle handle) override;

    void beginFrame(const ClearColor& clearColor) override;
    void setCamera(const CameraDesc& camera) override;
    void setLight(const DirectionalLight& light) override;
    void drawMesh(MeshHandle handle, const Mat4& model, const Material& material) override;
    void endFrame() override;

    void resize(u32 width, u32 height) override;
    [[nodiscard]] u32 width() const override { return m_width; }
    [[nodiscard]] u32 height() const override { return m_height; }

    [[nodiscard]] std::optional<std::vector<u8>> readbackRgba() const override;
    [[nodiscard]] DeviceStats stats() const override { return m_stats; }

private:
    struct DrawCommand {
        MeshHandle mesh{};
        Mat4 model = Mat4::identity();
        Material material{};
    };

    void rasterizeCommand(const DrawCommand& command);
    void drawTriangle(const Vertex& a, const Vertex& b, const Vertex& c, const Mat4& mvp,
                      const Mat4& normalMatrix, const Material& material);
    void writePixel(u32 x, u32 y, f32 depth, f32 red, f32 green, f32 blue);

    u32 m_width = 0;
    u32 m_height = 0;
    std::vector<u8> m_color;
    std::vector<f32> m_depth;
    std::vector<MeshData> m_meshes;
    std::vector<u32> m_freeMeshSlots;
    std::vector<DrawCommand> m_commands;
    CameraDesc m_camera{};
    DirectionalLight m_light{};
    ClearColor m_clear{};
    DeviceStats m_stats{};
    bool m_initialized = false;
    bool m_frameActive = false;
};

} // namespace zy

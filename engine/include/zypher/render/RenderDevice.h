// Rendering backend interface. Concrete devices live in render/<backend>/ and
// are selected at runtime through createRenderDevice().
#pragma once

#include "zypher/core/Memory.h"
#include "zypher/core/Result.h"
#include "zypher/render/RenderTypes.h"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace zy {

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual RenderBackend backend() const = 0;

    [[nodiscard]] virtual Status initialize(const RenderDeviceDesc& desc) = 0;
    virtual void shutdown() = 0;

    [[nodiscard]] virtual Status createMesh(const MeshData& mesh, MeshHandle& outHandle) = 0;
    virtual void destroyMesh(MeshHandle handle) = 0;

    virtual void beginFrame(const ClearColor& clearColor) = 0;
    virtual void setCamera(const CameraDesc& camera) = 0;
    virtual void setLight(const DirectionalLight& light) = 0;
    virtual void drawMesh(MeshHandle handle, const Mat4& model, const Material& material) = 0;
    virtual void endFrame() = 0;

    virtual void resize(u32 width, u32 height) = 0;
    [[nodiscard]] virtual u32 width() const = 0;
    [[nodiscard]] virtual u32 height() const = 0;

    // Returns tightly packed RGBA8 pixels, or nullopt when the backend cannot
    // read back the current frame (a real swap chain usually cannot).
    [[nodiscard]] virtual std::optional<std::vector<u8>> readbackRgba() const = 0;

    [[nodiscard]] virtual DeviceStats stats() const = 0;
};

// Returns an error with ErrorCode::Unavailable when the requested backend was
// not compiled in or its runtime is missing.
[[nodiscard]] Result<Scope<RenderDevice>> createRenderDevice(RenderBackend backend,
                                                             const RenderDeviceDesc& desc);

// Backends are only present when their option was enabled at configure time.
[[nodiscard]] bool isRenderBackendAvailable(RenderBackend backend);

} // namespace zy

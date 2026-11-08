#include "zypher/render/RenderDevice.h"

#include "zypher/core/Log.h"
#include "zypher/render/SoftwareDevice.h"

#include <array>
#include <string_view>

namespace zy {

std::optional<RenderBackend> renderBackendFromString(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, RenderBackend>, 5> kBackends{{
        {"software", RenderBackend::Software},
        {"vulkan", RenderBackend::Vulkan},
        {"d3d12", RenderBackend::Direct3D12},
        {"metal", RenderBackend::Metal},
        {"opengl", RenderBackend::OpenGL},
    }};
    for (const auto& [key, backend] : kBackends) {
        if (name == key) {
            return backend;
        }
    }
    return std::nullopt;
}

bool isRenderBackendAvailable(RenderBackend backend) {
    switch (backend) {
    case RenderBackend::Software:
        return true;
#if defined(ZYPHER_WITH_VULKAN)
    case RenderBackend::Vulkan:
        return true;
#endif
#if defined(ZYPHER_WITH_D3D12)
    case RenderBackend::Direct3D12:
        return true;
#endif
#if defined(ZYPHER_WITH_METAL)
    case RenderBackend::Metal:
        return true;
#endif
#if defined(ZYPHER_WITH_OPENGL)
    case RenderBackend::OpenGL:
        return true;
#endif
    default:
        return false;
    }
}

Result<Scope<RenderDevice>> createRenderDevice(RenderBackend backend, const RenderDeviceDesc& desc) {
    switch (backend) {
    case RenderBackend::Software: {
        auto device = makeScope<SoftwareDevice>();
        ZY_TRY(device->initialize(desc));
        return Scope<RenderDevice>(std::move(device));
    }
#if defined(ZYPHER_WITH_VULKAN)
    case RenderBackend::Vulkan: {
        extern Scope<RenderDevice> createVulkanDevice();
        return createVulkanDevice();
    }
#endif
#if defined(ZYPHER_WITH_D3D12)
    case RenderBackend::Direct3D12: {
        extern Scope<RenderDevice> createD3D12Device();
        return createD3D12Device();
    }
#endif
#if defined(ZYPHER_WITH_METAL)
    case RenderBackend::Metal: {
        extern Scope<RenderDevice> createMetalDevice();
        return createMetalDevice();
    }
#endif
#if defined(ZYPHER_WITH_OPENGL)
    case RenderBackend::OpenGL: {
        extern Scope<RenderDevice> createOpenGLDevice();
        return createOpenGLDevice();
    }
#endif
    default:
        break;
    }
    return Error{ErrorCode::Unavailable,
                 "render backend '" + std::string(renderBackendName(backend)) +
                     "' is not available in this build (reconfigure with the matching ZYPHER_WITH_* option)"};
}

} // namespace zy

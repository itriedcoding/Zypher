// Runtime application: loads a cooked pack and a scene, simulates physics and
// gameplay, renders frames and writes a machine readable report.
#pragma once

#include "zypher/assets/AssetRegistry.h"
#include "zypher/assets/PackFile.h"
#include "zypher/core/Jobs.h"
#include "zypher/core/Result.h"
#include "zypher/ecs/Registry.h"
#include "zypher/json/Json.h"
#include "zypher/physics/PhysicsWorld.h"
#include "zypher/render/RenderDevice.h"
#include "zypher/render/RenderTypes.h"
#include "zypher/scene/Scene.h"
#include "zypher/scripting/ScriptHost.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace zy {

struct AppOptions {
    std::string scenePath = "samples/demo/scene.zscene";
    std::string packPath;
    std::string scriptPath;
    std::string outputDirectory = "artifacts";
    RenderBackend backend = RenderBackend::Software;
    u32 width = 1280;
    u32 height = 720;
    u32 frames = 1;
    u32 simulationSteps = 120;
    f64 fixedDelta = 1.0 / 60.0;
    u32 threads = 0;
    bool render = true;
    bool dumpFrame = false;
    bool deterministic = true;
    bool writeReport = true;
};

struct AppReport {
    std::string engineVersion = ZYPHER_VERSION_STRING;
    std::string backend = "software";
    std::string scenePath;
    std::string sceneName;
    std::string packPath;
    std::string scriptModule;
    u32 frames = 0;
    u32 simulationSteps = 0;
    u32 entities = 0;
    u32 packAssets = 0;
    u32 drawCalls = 0;
    u64 triangles = 0;
    u64 renderChecksum = 0;
    u64 simulationChecksum = 0;
    f64 averageFrameMilliseconds = 0.0;
    f64 totalMilliseconds = 0.0;
    f64 simulationMilliseconds = 0.0;
    u32 threads = 1;
    bool deterministic = true;
    std::vector<std::string> warnings;

    [[nodiscard]] JsonValue toJson() const;
};

class Application final : NonCopyable {
public:
    Application() = default;
    ~Application();

    [[nodiscard]] Status initialize(const AppOptions& options);
    [[nodiscard]] Result<AppReport> run();
    void shutdown();

    [[nodiscard]] const AppOptions& options() const { return m_options; }
    [[nodiscard]] const Registry& registry() const { return m_registry; }

private:
    struct DrawItem {
        Entity entity{};
        MeshHandle mesh{};
        Mat4 model = Mat4::identity();
        Material material{};
    };

    void buildPhysicsWorld();
    [[nodiscard]] Status uploadMeshes();
    [[nodiscard]] CameraDesc makeCamera() const;
    [[nodiscard]] u64 computeSimulationChecksum() const;
    void writeFrameDump(ByteSpan rgba);

    AppOptions m_options{};
    PackFile m_pack;
    AssetRegistry m_assets;
    SceneDesc m_scene;
    Registry m_registry;
    PhysicsWorld m_physics;
    Scope<JobSystem> m_jobs;
    Scope<ScriptHost> m_scripts;
    Scope<RenderDevice> m_device;
    std::unordered_map<u64, MeshHandle> m_meshHandles;
    std::vector<std::string> m_warnings;
    u64 m_renderChecksum = 0;
    bool m_initialized = false;
};

} // namespace zy

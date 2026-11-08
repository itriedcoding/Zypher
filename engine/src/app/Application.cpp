#include "zypher/app/Application.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"
#include "zypher/core/Time.h"
#include "zypher/ecs/Components.h"
#include "zypher/math/Math.h"
#include "zypher/profiling/Profiler.h"
#include "zypher/render/MeshPrimitives.h"
#include "zypher/render/PngWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zy {
namespace {

[[nodiscard]] i64 quantise(f32 value) {
    return static_cast<i64>(std::llround(static_cast<f64>(value) * 10000.0));
}

} // namespace

Application::~Application() { shutdown(); }

Status Application::initialize(const AppOptions& options) {
    if (m_initialized) {
        return Error{ErrorCode::InvalidArgument, "application already initialised"};
    }
    m_options = options;
    m_options.width = std::clamp(m_options.width, 1u, 8192u);
    m_options.height = std::clamp(m_options.height, 1u, 8192u);

    m_jobs = makeScope<JobSystem>(m_options.threads);
    m_jobs->setDeterministic(m_options.deterministic);

    if (!m_options.packPath.empty()) {
        auto pack = PackFile::load(m_options.packPath);
        if (pack.hasError()) {
            return Error{pack.error().code, pack.error().message};
        }
        m_pack = std::move(pack).value();
    } else {
        ZY_LOG_WARN("runtime", "no asset pack supplied; meshes will fall back to primitives");
        m_warnings.push_back("no asset pack supplied");
    }
    m_assets.setPack(m_pack.valid() ? &m_pack : nullptr);

    auto scene = loadSceneFile(m_options.scenePath);
    if (scene.hasError()) {
        return Error{scene.error().code, scene.error().message};
    }
    m_scene = std::move(scene).value();
    instantiateScene(m_registry, m_scene);

    buildPhysicsWorld();

    if (!m_options.scriptPath.empty()) {
        m_scripts = makeScope<ScriptHost>();
        const auto status = m_scripts->loadModule(m_options.scriptPath);
        if (status.hasError()) {
            return Error{status.error().code, status.error().message};
        }
        m_scripts->startAll(m_registry);
    }

    if (m_options.render) {
        RenderDeviceDesc desc;
        desc.width = m_options.width;
        desc.height = m_options.height;
        desc.applicationName = "Zypher Runtime";
        auto device = createRenderDevice(m_options.backend, desc);
        if (device.hasError()) {
            return Error{device.error().code, device.error().message};
        }
        m_device = std::move(device).value();
        ZY_TRY(uploadMeshes());
    }

    m_initialized = true;
    ZY_LOG_INFO("runtime", "initialised scene '", m_scene.name, "' with ", m_registry.entityCount(),
                " entities, ", m_physics.bodyCount(), " dynamic bodies, ", m_physics.staticCount(),
                " static colliders");
    return ok();
}

void Application::buildPhysicsWorld() {
    m_physics.reset();
    PhysicsSettings settings;
    settings.fixedDelta = static_cast<f32>(m_options.fixedDelta);
    m_physics.configure(settings);

    m_registry.each<RigidBody, Transform>([&](Entity entity, RigidBody& body, Transform& transform) {
        const Vec3 worldExtents{std::fabs(body.halfExtents.x * transform.scale.x),
                                std::fabs(body.halfExtents.y * transform.scale.y),
                                std::fabs(body.halfExtents.z * transform.scale.z)};
        if (body.isStatic || body.inverseMass <= 0.0f) {
            m_physics.addStaticBox(entity, Aabb::fromCenterExtents(transform.position, worldExtents));
            return;
        }
        body.halfExtents = worldExtents;
        m_physics.addDynamicBody(entity, worldExtents);
    });
}

Status Application::uploadMeshes() {
    std::vector<Entity> entities;
    m_registry.each<MeshRenderer>([&](Entity entity, MeshRenderer&) { entities.push_back(entity); });

    u32 loaded = 0;
    u32 fallbacks = 0;
    for (const Entity entity : entities) {
        auto& renderer = m_registry.get<MeshRenderer>(entity);
        if (const auto cached = m_meshHandles.find(renderer.mesh); cached != m_meshHandles.end()) {
            continue;
        }

        MeshData mesh;
        bool usedFallback = false;
        if (renderer.mesh != kInvalidAsset) {
            auto cooked = m_assets.loadMesh(renderer.mesh);
            if (cooked.hasValue()) {
                mesh = std::move(cooked).value();
            } else {
                ZY_LOG_WARN("runtime", cooked.error().message);
                m_warnings.push_back(cooked.error().message);
                usedFallback = true;
            }
        } else {
            usedFallback = true;
        }
        if (usedFallback) {
            const std::string name = m_registry.tryGet<Name>(entity) != nullptr
                                         ? m_registry.get<Name>(entity).value
                                         : std::string("entity");
            mesh = makeCubeMesh(1.0f);
            m_warnings.push_back("mesh fallback (unit cube) for '" + name + "'");
            ++fallbacks;
        }

        MeshHandle handle;
        const auto status = m_device->createMesh(mesh, handle);
        if (status.hasError()) {
            return Error{status.error().code, status.error().message};
        }
        m_meshHandles.emplace(renderer.mesh, handle);
        ++loaded;
    }
    ZY_LOG_INFO("render", "uploaded ", loaded, " mesh(es), ", fallbacks, " fallback(s)");
    return ok();
}

CameraDesc Application::makeCamera() const {
    CameraDesc camera;
    camera.position = m_scene.cameraPosition;
    camera.view = Mat4::lookAt(m_scene.cameraPosition, m_scene.cameraTarget, Vec3::up());
    const f32 aspect = static_cast<f32>(m_options.width) / static_cast<f32>(m_options.height);
    camera.projection = Mat4::perspective(math::radians(m_scene.cameraFovDegrees), aspect,
                                          m_scene.cameraNear, m_scene.cameraFar);
    return camera;
}

Result<AppReport> Application::run() {
    if (!m_initialized) {
        return Error{ErrorCode::Internal, "run() called before initialize()"};
    }
    AppReport report;
    report.backend = std::string(renderBackendName(m_options.backend));
    report.scenePath = m_options.scenePath;
    report.sceneName = m_scene.name;
    report.packPath = m_options.packPath;
    report.packAssets = static_cast<u32>(m_pack.assetCount());
    report.simulationSteps = m_options.simulationSteps;
    report.frames = m_options.render ? m_options.frames : 0;
    report.deterministic = m_options.deterministic;
    report.threads = m_jobs != nullptr ? m_jobs->threadCount() : 0;
    if (m_scripts != nullptr && m_scripts->loaded()) {
        report.scriptModule = std::string(m_scripts->moduleName());
    }

    const Stopwatch totalTimer;

    // --- simulation -------------------------------------------------------
    {
        const Stopwatch simulationTimer;
        for (u32 step = 0; step < m_options.simulationSteps; ++step) {
            m_physics.step(static_cast<f32>(m_options.fixedDelta), m_registry);
            if (m_scripts != nullptr) {
                m_scripts->updateAll(m_registry, static_cast<f32>(m_options.fixedDelta));
            }
        }
        report.simulationMilliseconds = simulationTimer.elapsedMilliseconds();
    }

    // --- render -----------------------------------------------------------
    if (m_options.render && m_device != nullptr) {
        ZY_PROFILE_FRAME();
        std::vector<DrawItem> items;
        items.reserve(m_meshHandles.size() * 4U);
        m_registry.each<MeshRenderer, Transform>([&](Entity entity, MeshRenderer& renderer, Transform& transform) {
            const auto handle = m_meshHandles.find(renderer.mesh);
            if (handle == m_meshHandles.end()) {
                return;
            }
            DrawItem item;
            item.entity = entity;
            item.mesh = handle->second;
            item.model = transform.matrix();
            item.material.baseColor = renderer.baseColor;
            item.material.roughness = renderer.roughness;
            item.material.metallic = renderer.metallic;
            items.push_back(item);
        });

        const CameraDesc camera = makeCamera();
        const Stopwatch renderTimer;
        for (u32 frame = 0; frame < m_options.frames; ++frame) {
            m_device->beginFrame(m_scene.clearColor);
            m_device->setCamera(camera);
            m_device->setLight(m_scene.light);
            for (const auto& item : items) {
                m_device->drawMesh(item.mesh, item.model, item.material);
            }
            m_device->endFrame();
        }
        const f64 elapsed = renderTimer.elapsedMilliseconds();
        report.averageFrameMilliseconds =
            m_options.frames > 0 ? elapsed / static_cast<f64>(m_options.frames) : 0.0;
        report.frames = m_options.frames;

        const auto stats = m_device->stats();
        report.drawCalls = stats.drawCalls;
        report.triangles = stats.triangles;

        if (const auto pixels = m_device->readbackRgba(); pixels.has_value()) {
            m_renderChecksum = fnv1a64(pixels->data(), pixels->size());
            report.renderChecksum = m_renderChecksum;
            if (m_options.dumpFrame) {
                writeFrameDump(ByteSpan{pixels->data(), pixels->size()});
            }
        }
    }

    report.entities = static_cast<u32>(m_registry.entityCount());
    report.simulationChecksum = computeSimulationChecksum();
    report.totalMilliseconds = totalTimer.elapsedMilliseconds();
    report.warnings = m_warnings;

    if (m_options.writeReport) {
        const std::string path = FileSystem::join(m_options.outputDirectory, "report.json");
        if (const auto status = FileSystem::writeText(path, report.toJson().dump(true)); status.hasError()) {
            return Error{status.error().code, "cannot write report: " + status.error().message};
        }
        ZY_LOG_INFO("runtime", "wrote report to ", path);
    }

    return report;
}

void Application::writeFrameDump(ByteSpan rgba) {
    if (m_device == nullptr) {
        return;
    }
    const std::string path = FileSystem::join(m_options.outputDirectory, "frame.png");
    (void)FileSystem::createDirectories(m_options.outputDirectory);
    if (writePng(path, m_device->width(), m_device->height(), rgba)) {
        ZY_LOG_INFO("runtime", "wrote frame dump to ", path);
    }
}

u64 Application::computeSimulationChecksum() const {
    std::string state;
    state.reserve(m_registry.entityCount() * 48);
    for (const Entity entity : m_physics.bodyEntities()) {
        if (!m_registry.alive(entity)) {
            continue;
        }
        const auto* transform = m_registry.tryGet<Transform>(entity);
        const auto* rigidBody = m_registry.tryGet<RigidBody>(entity);
        if (transform == nullptr || rigidBody == nullptr) {
            continue;
        }
        char buffer[128]{};
        std::snprintf(buffer, sizeof(buffer), "%u:%lld,%lld,%lld|%lld,%lld,%lld;", entity.index,
                      static_cast<long long>(quantise(transform->position.x)),
                      static_cast<long long>(quantise(transform->position.y)),
                      static_cast<long long>(quantise(transform->position.z)),
                      static_cast<long long>(quantise(rigidBody->velocity.x)),
                      static_cast<long long>(quantise(rigidBody->velocity.y)),
                      static_cast<long long>(quantise(rigidBody->velocity.z)));
        state += buffer;
    }
    return fnv1a64(state);
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }
    if (m_scripts != nullptr) {
        m_scripts->stopAll(m_registry);
        m_scripts->unload();
        m_scripts.reset();
    }
    m_device.reset();
    m_jobs.reset();
    m_meshHandles.clear();
    m_assets.clearCache();
    m_registry.clear();
    m_physics.reset();
    m_initialized = false;
}

JsonValue AppReport::toJson() const {
    JsonValue root = JsonValue::makeObject();
    root.set("engineVersion", engineVersion);
    root.set("backend", backend);
    root.set("scene", scenePath);
    root.set("sceneName", sceneName);
    root.set("pack", packPath);
    root.set("packAssets", static_cast<u32>(packAssets));
    root.set("scriptModule", scriptModule);
    root.set("frames", frames);
    root.set("simulationSteps", simulationSteps);
    root.set("entities", entities);
    root.set("drawCalls", drawCalls);
    root.set("triangles", static_cast<f64>(triangles));
    root.set("renderChecksum", assetIdToString(renderChecksum));
    root.set("simulationChecksum", assetIdToString(simulationChecksum));
    root.set("averageFrameMs", averageFrameMilliseconds);
    root.set("totalMs", totalMilliseconds);
    root.set("simulationMs", simulationMilliseconds);
    root.set("threads", threads);
    root.set("deterministic", deterministic);

    JsonValue warningArray = JsonValue::makeArray();
    for (const auto& warning : warnings) {
        warningArray.push(warning);
    }
    root.set("warnings", std::move(warningArray));
    return root;
}

} // namespace zy

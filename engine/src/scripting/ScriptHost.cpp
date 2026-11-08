#include "zypher/scripting/ScriptHost.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"
#include "zypher/ecs/Components.h"
#include "zypher/math/Math.h"
#include "zypher/scripting/ScriptApi.h"

#include <algorithm>
#include <cstring>

#if defined(ZYPHER_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace zy {
namespace {

#if defined(ZYPHER_PLATFORM_WINDOWS)
// Module paths are UTF-8 everywhere in the engine, but the Windows loader and
// LoadLibraryA use the ANSI code page. Convert so non-ASCII paths still load.
[[nodiscard]] std::wstring toWidePath(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<usize>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}
#endif

ZypherVec3 toApi(const Vec3& value) { return ZypherVec3{value.x, value.y, value.z}; }
Vec3 fromApi(const ZypherVec3& value) { return Vec3{value.x, value.y, value.z}; }
ZypherEntityHandle toApi(Entity entity) { return ZypherEntityHandle{entity.index, entity.generation}; }
Entity fromApi(const ZypherEntityHandle& handle) { return Entity{handle.index, handle.generation}; }

} // namespace

struct ScriptHost::Module {
    ZypherScriptModule* table = nullptr;
};

// Engine side of the ABI. Lifetime is tied to ScriptHost; the pointer handed to
// scripts stays valid until unload().
struct ScriptHost::HostBridge {
    Registry* registry = nullptr;
    ZypherScriptContext context{};
};

ScriptHost::~ScriptHost() { unload(); }

bool ScriptHost::available() {
#if defined(ZYPHER_PLATFORM_WINDOWS) || defined(__unix__) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

Status ScriptHost::loadModule(const std::string& libraryPath) {
    if (m_module != nullptr) {
        return Error{ErrorCode::InvalidArgument, "a script module is already loaded"};
    }
    if (!FileSystem::exists(libraryPath)) {
        return Error{ErrorCode::NotFound, "script module '" + libraryPath + "' does not exist"};
    }

#if defined(ZYPHER_PLATFORM_WINDOWS)
    HMODULE library = LoadLibraryW(toWidePath(libraryPath).c_str());
    if (library == nullptr) {
        return Error{ErrorCode::Io, "LoadLibrary failed for '" + libraryPath + "' (error " +
                                        std::to_string(GetLastError()) + ")"};
    }
    const auto symbol = GetProcAddress(library, "zypher_script_module");
#else
    void* library = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        const char* message = dlerror();
        return Error{ErrorCode::Io, std::string("dlopen failed for '") + libraryPath + "': " +
                                        (message != nullptr ? message : "unknown error")};
    }
    // dlsym returns void*; keep it non-const because casting a const void* to a
    // function pointer casts away qualifiers, which clang rejects.
    void* symbol = dlsym(library, "zypher_script_module");
#endif

    if (symbol == nullptr) {
        unload();
#if defined(ZYPHER_PLATFORM_WINDOWS)
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return Error{ErrorCode::Parse,
                     "'" + libraryPath + "' does not export zypher_script_module()"};
    }

    using ModuleGetter = ZypherScriptModule* (*)();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto getter = reinterpret_cast<ModuleGetter>(symbol);
    ZypherScriptModule* table = getter();
    if (table == nullptr) {
#if defined(ZYPHER_PLATFORM_WINDOWS)
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return Error{ErrorCode::Parse, "zypher_script_module() returned null"};
    }
    if (table->abi_version != ZYPHER_SCRIPT_ABI_VERSION) {
#if defined(ZYPHER_PLATFORM_WINDOWS)
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return Error{ErrorCode::Unsupported,
                     "script module ABI " + std::to_string(table->abi_version) + " does not match engine ABI " +
                         std::to_string(ZYPHER_SCRIPT_ABI_VERSION)};
    }

    m_library = library;
    m_module = new Module{table};
    m_modulePath = libraryPath;

    m_bridge = new HostBridge();
    ZypherScriptContext& context = m_bridge->context;
    context.struct_size = sizeof(ZypherScriptContext);
    context.host = m_bridge;
    context.delta_seconds = 0.0f;
    context.log_info = [](void* host, const char* message) {
        (void)host;
        Log::write(LogLevel::Info, "script", message != nullptr ? message : "");
    };
    context.log_warn = [](void* host, const char* message) {
        (void)host;
        Log::write(LogLevel::Warn, "script", message != nullptr ? message : "");
    };
    context.get_transform = [](void* host, ZypherEntityHandle handle) -> ZypherTransform {
        ZypherTransform result{};
        result.scale = ZypherVec3{1.0f, 1.0f, 1.0f};
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge == nullptr || bridge->registry == nullptr) {
            return result;
        }
        const Entity entity = fromApi(handle);
        if (auto* transform = bridge->registry->tryGet<Transform>(entity); transform != nullptr) {
            result.position = toApi(transform->position);
            result.scale = toApi(transform->scale);
        }
        return result;
    };
    context.set_transform = [](void* host, ZypherEntityHandle handle, ZypherTransform value) {
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge == nullptr || bridge->registry == nullptr) {
            return;
        }
        const Entity entity = fromApi(handle);
        if (auto* transform = bridge->registry->tryGet<Transform>(entity); transform != nullptr) {
            transform->position = fromApi(value.position);
            transform->scale = fromApi(value.scale);
        }
    };
    context.get_velocity = [](void* host, ZypherEntityHandle handle) -> ZypherVec3 {
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge != nullptr && bridge->registry != nullptr) {
            if (const auto* body = bridge->registry->tryGet<RigidBody>(fromApi(handle)); body != nullptr) {
                return toApi(body->velocity);
            }
        }
        return ZypherVec3{0.0f, 0.0f, 0.0f};
    };
    context.set_velocity = [](void* host, ZypherEntityHandle handle, ZypherVec3 value) {
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge != nullptr && bridge->registry != nullptr) {
            if (auto* body = bridge->registry->tryGet<RigidBody>(fromApi(handle)); body != nullptr) {
                body->velocity = fromApi(value);
            }
        }
    };
    context.set_velocity_y = [](void* host, ZypherEntityHandle handle, float value) {
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge != nullptr && bridge->registry != nullptr) {
            if (auto* body = bridge->registry->tryGet<RigidBody>(fromApi(handle)); body != nullptr) {
                body->velocity.y = value;
            }
        }
    };
    context.find_entity = [](void* host, const char* name) -> ZypherEntityHandle {
        auto* bridge = static_cast<HostBridge*>(host);
        if (bridge != nullptr && bridge->registry != nullptr && name != nullptr) {
            ZypherEntityHandle result{Entity::kInvalidIndex, 0};
            bridge->registry->each<Name>([&](Entity entity, Name& component) {
                if (result.index == Entity::kInvalidIndex && component.value == name) {
                    result = toApi(entity);
                }
            });
            return result;
        }
        return ZypherEntityHandle{Entity::kInvalidIndex, 0};
    };

    ZY_LOG_INFO("script", "loaded module '", table->name != nullptr ? table->name : libraryPath, "' from ",
                libraryPath);
    return ok();
}

void ScriptHost::unload() {
    if (m_module == nullptr) {
        return;
    }
    for (auto& instance : m_instances) {
        if (instance.handle != nullptr && m_module->table->on_stop != nullptr) {
            m_module->table->on_stop(instance.handle);
        }
        if (instance.handle != nullptr && m_module->table->destroy_instance != nullptr) {
            m_module->table->destroy_instance(instance.handle);
        }
    }
    m_instances.clear();
    delete m_module;
    m_module = nullptr;
    delete m_bridge;
    m_bridge = nullptr;

    if (m_library != nullptr) {
#if defined(ZYPHER_PLATFORM_WINDOWS)
        FreeLibrary(static_cast<HMODULE>(m_library));
#else
        dlclose(m_library);
#endif
        m_library = nullptr;
    }
    m_modulePath.clear();
}

std::string_view ScriptHost::moduleName() const {
    if (m_module == nullptr || m_module->table == nullptr || m_module->table->name == nullptr) {
        return {};
    }
    return m_module->table->name;
}

void ScriptHost::startAll(Registry& registry) {
    if (m_module == nullptr || m_module->table->create_instance == nullptr) {
        return;
    }
    m_bridge->registry = &registry;

    registry.each<ScriptComponent>([&](Entity entity, ScriptComponent& script) {
        if (!script.enabled || script.className.empty()) {
            return;
        }
        void* handle = m_module->table->create_instance(script.className.c_str(), toApi(entity),
                                                        &m_bridge->context);
        if (handle == nullptr) {
            ZY_LOG_WARN("script", "module could not create an instance of '", script.className, "'");
            return;
        }
        m_instances.push_back(Instance{entity, script.className, handle});
        if (m_module->table->on_start != nullptr) {
            m_module->table->on_start(handle);
        }
    });
    ZY_LOG_INFO("script", "started ", m_instances.size(), " script instance(s)");
}

void ScriptHost::updateAll(Registry& registry, f32 deltaSeconds) {
    m_lastUpdateCount = 0;
    if (m_module == nullptr) {
        return;
    }
    m_bridge->registry = &registry;
    m_bridge->context.delta_seconds = deltaSeconds;

    for (auto& instance : m_instances) {
        if (!registry.alive(instance.entity)) {
            if (m_module->table->destroy_instance != nullptr) {
                m_module->table->destroy_instance(instance.handle);
            }
            instance.handle = nullptr;
            continue;
        }
        if (m_module->table->on_update != nullptr && instance.handle != nullptr) {
            m_module->table->on_update(instance.handle, deltaSeconds);
            ++m_lastUpdateCount;
        }
    }
    m_instances.erase(std::remove_if(m_instances.begin(), m_instances.end(),
                                     [](const Instance& instance) { return instance.handle == nullptr; }),
                      m_instances.end());
}

void ScriptHost::stopAll(Registry& registry) {
    if (m_module == nullptr) {
        return;
    }
    m_bridge->registry = &registry;
    for (auto& instance : m_instances) {
        if (instance.handle != nullptr && m_module->table->on_stop != nullptr) {
            m_module->table->on_stop(instance.handle);
        }
        if (instance.handle != nullptr && m_module->table->destroy_instance != nullptr) {
            m_module->table->destroy_instance(instance.handle);
        }
        instance.handle = nullptr;
    }
    m_instances.clear();
}

} // namespace zy

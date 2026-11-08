// Loads gameplay modules (NativeAOT C# libraries) implementing ScriptApi.h and
// drives their life cycle from the runtime.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/core/Result.h"
#include "zypher/ecs/Registry.h"

#include <string>
#include <string_view>
#include <vector>

namespace zy {

class ScriptHost final : NonCopyable {
public:
    ScriptHost() = default;
    ~ScriptHost();

    [[nodiscard]] Status loadModule(const std::string& libraryPath);
    void unload();

    // Creates instances for every entity with a ScriptComponent.
    void startAll(Registry& registry);
    void updateAll(Registry& registry, f32 deltaSeconds);
    void stopAll(Registry& registry);

    [[nodiscard]] bool loaded() const { return m_module != nullptr; }
    [[nodiscard]] std::string_view moduleName() const;
    [[nodiscard]] const std::string& modulePath() const { return m_modulePath; }
    [[nodiscard]] usize instanceCount() const { return m_instances.size(); }
    [[nodiscard]] u32 lastUpdateCount() const { return m_lastUpdateCount; }

    // Whether script modules can be loaded at all in this build/platform.
    [[nodiscard]] static bool available();

private:
    struct Instance {
        Entity entity{};
        std::string className;
        void* handle = nullptr;
    };
    struct Module;
    struct HostBridge;

    Module* m_module = nullptr;
    void* m_library = nullptr;
    std::string m_modulePath;
    std::vector<Instance> m_instances;
    HostBridge* m_bridge = nullptr;
    u32 m_lastUpdateCount = 0;
};

} // namespace zy

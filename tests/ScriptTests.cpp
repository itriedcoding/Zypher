#include "TestFramework.h"

#include "zypher/ecs/Components.h"
#include "zypher/ecs/Registry.h"
#include "zypher/scripting/ScriptHost.h"

#include <string>

using namespace zy;

ZY_TEST(script_host_loads_and_runs_module) {
    ScriptHost host;
    ASSERT_TRUE(ScriptHost::available());
    const auto status = host.loadModule(ZYPHER_TEST_SCRIPT_MODULE);
    ASSERT_TRUE(status.hasValue());
    EXPECT_TRUE(host.loaded());
    EXPECT_TRUE(host.moduleName().find("Test Script Module") != std::string::npos);

    Registry registry;
    const Entity entity = registry.create();
    Transform transform;
    transform.position = Vec3{0.0f, 1.0f, 0.0f};
    registry.add<Transform>(entity, transform);
    registry.add<Name>(entity, Name{"player", true});
    registry.add<ScriptComponent>(entity, ScriptComponent{"Spinner", true});

    host.startAll(registry);
    EXPECT_EQ(host.instanceCount(), 1U);

    host.updateAll(registry, 0.5f);
    EXPECT_EQ(host.lastUpdateCount(), 1U);
    EXPECT_NEAR(registry.get<Transform>(entity).position.y, 1.5, 1e-5);

    host.updateAll(registry, 0.25f);
    EXPECT_NEAR(registry.get<Transform>(entity).position.y, 1.75, 1e-5);

    host.stopAll(registry);
    EXPECT_EQ(host.instanceCount(), 0U);
    host.unload();
    EXPECT_FALSE(host.loaded());
}

ZY_TEST(script_host_rejects_missing_module) {
    ScriptHost host;
    const auto status = host.loadModule("does-not-exist.dylib");
    EXPECT_TRUE(status.hasError());
    EXPECT_FALSE(host.loaded());
}

ZY_TEST(script_host_ignores_unknown_class) {
    ScriptHost host;
    ASSERT_TRUE(host.loadModule(ZYPHER_TEST_SCRIPT_MODULE).hasValue());
    Registry registry;
    const Entity entity = registry.create();
    registry.add<Transform>(entity);
    registry.add<ScriptComponent>(entity, ScriptComponent{"NotARealBehaviour", true});
    host.startAll(registry);
    EXPECT_EQ(host.instanceCount(), 0U);
    host.unload();
}

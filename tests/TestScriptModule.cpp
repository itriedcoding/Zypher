// Native script module used by ScriptTests. It implements the Zypher script ABI
// the same way a NativeAOT compiled C# gameplay assembly does.
#include "zypher/scripting/ScriptApi.h"

#include <cstring>
#include <string>

namespace {

struct Instance {
    std::string className;
    ZypherScriptContext context{};
    ZypherEntityHandle entity{};
    int updates = 0;
    bool started = false;
};

Instance* cast(void* pointer) { return static_cast<Instance*>(pointer); }

void* createInstance(const char* className, ZypherEntityHandle entity, const ZypherScriptContext* context) {
    if (className == nullptr || context == nullptr) {
        return nullptr;
    }
    if (std::strcmp(className, "Spinner") != 0 && std::strcmp(className, "Faller") != 0) {
        return nullptr;
    }
    auto* instance = new Instance();
    instance->className = className;
    instance->context = *context;
    instance->entity = entity;
    return instance;
}

void destroyInstance(void* pointer) { delete cast(pointer); }

void onStart(void* pointer) {
    auto* instance = cast(pointer);
    if (instance == nullptr) {
        return;
    }
    instance->started = true;
    instance->context.log_info(instance->context.host, "test module started");
}

void onUpdate(void* pointer, float deltaSeconds) {
    auto* instance = cast(pointer);
    if (instance == nullptr) {
        return;
    }
    ++instance->updates;
    ZypherTransform transform = instance->context.get_transform(instance->context.host, instance->entity);
    transform.position.y += deltaSeconds;
    instance->context.set_transform(instance->context.host, instance->entity, transform);
}

void onStop(void* pointer) {
    auto* instance = cast(pointer);
    if (instance != nullptr) {
        instance->context.log_info(instance->context.host, "test module stopped");
    }
}

ZypherScriptModule g_module{};

} // namespace

extern "C" ZYPHER_SCRIPT_EXPORT ZypherScriptModule* zypher_script_module(void) {
    g_module.abi_version = ZYPHER_SCRIPT_ABI_VERSION;
    g_module.name = "Zypher Test Script Module";
    g_module.create_instance = createInstance;
    g_module.destroy_instance = destroyInstance;
    g_module.on_start = onStart;
    g_module.on_update = onUpdate;
    g_module.on_stop = onStop;
    return &g_module;
}

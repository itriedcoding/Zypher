/*
 * Zypher script module ABI (version 1).
 *
 * Gameplay code is written in C#. The game assembly is published with .NET
 * NativeAOT into a native shared library that exports
 * `zypher_script_module()` and returns a table of function pointers, so the
 * engine loads scripts with the platform loader (no managed runtime hosted in
 * process, no GC pauses inside the frame).
 *
 * See engine/scripting/csharp for the C# side and docs/scripting.md.
 */
#ifndef ZYPHER_SCRIPT_API_H
#define ZYPHER_SCRIPT_API_H

#include <stdint.h>

#if defined(_WIN32)
#define ZYPHER_SCRIPT_EXPORT __declspec(dllexport)
#else
#define ZYPHER_SCRIPT_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ZYPHER_SCRIPT_ABI_VERSION 1u

typedef struct ZypherVec3 {
    float x;
    float y;
    float z;
} ZypherVec3;

typedef struct ZypherTransform {
    ZypherVec3 position;
    ZypherVec3 rotation_euler_degrees;
    ZypherVec3 scale;
} ZypherTransform;

typedef struct ZypherEntityHandle {
    uint32_t index;
    uint32_t generation;
} ZypherEntityHandle;

/* Callbacks provided by the engine for the duration of a frame. */
typedef struct ZypherScriptContext {
    uint32_t struct_size;
    void* host;
    float delta_seconds;
    void (*log_info)(void* host, const char* message);
    void (*log_warn)(void* host, const char* message);
    ZypherTransform (*get_transform)(void* host, ZypherEntityHandle entity);
    void (*set_transform)(void* host, ZypherEntityHandle entity, ZypherTransform transform);
    ZypherVec3 (*get_velocity)(void* host, ZypherEntityHandle entity);
    void (*set_velocity)(void* host, ZypherEntityHandle entity, ZypherVec3 velocity);
    void (*set_velocity_y)(void* host, ZypherEntityHandle entity, float value);
    ZypherEntityHandle (*find_entity)(void* host, const char* name);
} ZypherScriptContext;

typedef struct ZypherScriptModule {
    uint32_t abi_version;
    const char* name;
    void* (*create_instance)(const char* class_name, ZypherEntityHandle entity,
                             const ZypherScriptContext* context);
    void (*destroy_instance)(void* instance);
    void (*on_start)(void* instance);
    void (*on_update)(void* instance, float delta_seconds);
    void (*on_stop)(void* instance);
} ZypherScriptModule;

/* Every game module exports this symbol. */
ZYPHER_SCRIPT_EXPORT ZypherScriptModule* zypher_script_module(void);

#ifdef __cplusplus
}
#endif

#endif /* ZYPHER_SCRIPT_API_H */

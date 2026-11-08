// Built-in components. Games may register their own; these cover what the
// engine runtime itself understands.
#pragma once

#include "zypher/assets/AssetId.h"
#include "zypher/math/Geometry.h"
#include "zypher/math/Transform.h"

#include <string>

namespace zy {

struct Name {
    std::string value;
    bool enabled = true;
};

struct MeshRenderer {
    AssetId mesh = kInvalidAsset;
    Vec4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    bool castShadows = true;
};

struct Camera {
    f32 verticalFovDegrees = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 500.0f;
    bool primary = false;
};

// Bounds are specified in local space and scaled by the transform.
struct RigidBody {
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    f32 inverseMass = 1.0f; // 0 == static/kinematic
    f32 restitution = 0.2f;
    f32 linearDamping = 0.02f;
    bool isStatic = false;
    bool grounded = false;
};

struct ScriptComponent {
    std::string className;
    bool enabled = true;
};

struct AudioSource {
    AssetId clip = kInvalidAsset;
    f32 gain = 1.0f;
    bool loop = false;
    bool playOnStart = false;
};

// Marks entities that are only relevant to the editor.
struct EditorOnly {};

} // namespace zy

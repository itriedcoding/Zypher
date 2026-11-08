// Scene authoring format (.zscene, JSON) and instantiation into the ECS.
#pragma once

#include "zypher/assets/AssetId.h"
#include "zypher/core/Result.h"
#include "zypher/ecs/Registry.h"
#include "zypher/json/Json.h"
#include "zypher/math/Transform.h"
#include "zypher/render/RenderTypes.h"

#include <string>
#include <vector>

namespace zy {

struct EntityDesc {
    std::string name = "Entity";
    Transform transform{};
    std::string meshPath; // project relative, e.g. "models/cube.obj"
    Material material{};
    bool castShadows = true;

    bool hasRigidBody = false;
    bool isStatic = false;
    f32 mass = 1.0f;
    f32 restitution = 0.2f;
    std::optional<Vec3> colliderHalfExtents;

    bool hasCamera = false;
    f32 cameraFovDegrees = 60.0f;

    std::string scriptClass;
    bool optional = false;
};

struct SceneDesc {
    std::string name = "Untitled";
    ClearColor clearColor{};
    DirectionalLight light{};
    Vec3 cameraPosition{0.0f, 2.5f, 7.0f};
    Vec3 cameraTarget{0.0f, 1.0f, 0.0f};
    f32 cameraFovDegrees = 55.0f;
    f32 cameraNear = 0.1f;
    f32 cameraFar = 500.0f;
    std::vector<EntityDesc> entities;
};

// Parses the JSON scene object. Unknown fields are ignored, which keeps older
// scenes loadable after the format grows.
[[nodiscard]] Result<SceneDesc> parseScene(const JsonValue& document);
[[nodiscard]] Result<SceneDesc> loadSceneFile(const std::string& path);

// Builds entities, transforms, renderers, cameras and rigid bodies.
void instantiateScene(Registry& registry, const SceneDesc& scene);

// Serialises a scene description back to JSON (used by the editor bridge).
[[nodiscard]] JsonValue sceneToJson(const SceneDesc& scene);

} // namespace zy

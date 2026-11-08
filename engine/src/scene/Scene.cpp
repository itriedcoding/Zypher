#include "zypher/scene/Scene.h"

#include "zypher/assets/AssetRegistry.h"
#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"
#include "zypher/ecs/Components.h"
#include "zypher/math/Math.h"

namespace zy {
namespace {

[[nodiscard]] AssetId meshIdForPath(const std::string& path) {
    return path.empty() ? kInvalidAsset : assetIdFromPath(FileSystem::normalize(path));
}

[[nodiscard]] Quat eulerFromJson(const JsonValue& value) {
    const Vec3 degrees = value.getVec3("rotation", Vec3::zero());
    return Quat::fromEuler(Vec3{math::radians(degrees.x), math::radians(degrees.y),
                                math::radians(degrees.z)});
}

} // namespace

Result<SceneDesc> parseScene(const JsonValue& document) {
    if (!document.isObject()) {
        return Error{ErrorCode::Parse, "scene root must be an object"};
    }
    SceneDesc scene;
    scene.name = document.getString("name", "Untitled");

    if (document.has("environment")) {
        const JsonValue& environment = document["environment"];
        scene.clearColor = ClearColor{environment["clearColor"].at(0).asFloat(0.06f),
                                      environment["clearColor"].at(1).asFloat(0.07f),
                                      environment["clearColor"].at(2).asFloat(0.09f),
                                      environment["clearColor"].at(3).asFloat(1.0f)};
        if (environment.has("light")) {
            const JsonValue& light = environment["light"];
            scene.light.direction = light.getVec3("direction", scene.light.direction);
            scene.light.color = light.getVec3("color", scene.light.color);
            scene.light.intensity = light.getFloat("intensity", scene.light.intensity);
        }
    }

    if (document.has("camera")) {
        const JsonValue& camera = document["camera"];
        scene.cameraPosition = camera.getVec3("position", scene.cameraPosition);
        scene.cameraTarget = camera.getVec3("target", scene.cameraTarget);
        scene.cameraFovDegrees = camera.getFloat("fov", scene.cameraFovDegrees);
        scene.cameraNear = camera.getFloat("near", scene.cameraNear);
        scene.cameraFar = camera.getFloat("far", scene.cameraFar);
    }

    const JsonValue& entities = document["entities"];
    if (!entities.isArray()) {
        return Error{ErrorCode::Parse, "scene is missing an 'entities' array"};
    }

    scene.entities.reserve(entities.size());
    for (usize index = 0; index < entities.size(); ++index) {
        const JsonValue& source = entities.at(index);
        if (!source.isObject()) {
            return Error{ErrorCode::Parse, "entity " + std::to_string(index) + " is not an object"};
        }
        EntityDesc entity;
        entity.name = source.getString("name", "Entity " + std::to_string(index));
        entity.optional = source.getBool("optional", false);

        if (source.has("transform")) {
            const JsonValue& transform = source["transform"];
            entity.transform.position = transform.getVec3("position", Vec3::zero());
            entity.transform.scale = transform.getVec3("scale", Vec3::one());
            if (transform.has("rotation")) {
                entity.transform.rotation = eulerFromJson(transform);
            }
        }

        entity.meshPath = FileSystem::normalize(source.getString("mesh", ""));
        if (source.has("material")) {
            const JsonValue& material = source["material"];
            entity.material.baseColor = material.getVec4("baseColor", entity.material.baseColor);
            entity.material.roughness = material.getFloat("roughness", entity.material.roughness);
            entity.material.metallic = material.getFloat("metallic", entity.material.metallic);
            entity.material.emissive = material.getVec3("emissive", entity.material.emissive);
        }
        entity.castShadows = source.getBool("castShadows", true);

        if (source.has("rigidbody")) {
            const JsonValue& body = source["rigidbody"];
            entity.hasRigidBody = true;
            entity.isStatic = body.getBool("static", false);
            entity.mass = body.getFloat("mass", 1.0f);
            entity.restitution = body.getFloat("restitution", 0.2f);
            if (body.has("halfExtents")) {
                entity.colliderHalfExtents = body.getVec3("halfExtents", Vec3{0.5f, 0.5f, 0.5f});
            }
        }

        if (source.has("camera")) {
            entity.hasCamera = true;
            entity.cameraFovDegrees = source["camera"].getFloat("fov", 60.0f);
        }

        entity.scriptClass = source.getString("script", "");
        scene.entities.push_back(std::move(entity));
    }
    return scene;
}

Result<SceneDesc> loadSceneFile(const std::string& path) {
    auto text = FileSystem::readText(path);
    if (!text.has_value()) {
        return Error{ErrorCode::NotFound, "cannot read scene file '" + path + "'"};
    }
    auto document = parseJson(*text);
    if (document.hasError()) {
        return Error{document.error().code, "'" + path + "': " + document.error().message};
    }
    auto scene = parseScene(*document);
    if (scene.hasError()) {
        return Error{scene.error().code, "'" + path + "': " + scene.error().message};
    }
    ZY_LOG_INFO("scene", "loaded '", scene.value().name, "' with ", scene.value().entities.size(),
                " entities");
    return scene;
}

void instantiateScene(Registry& registry, const SceneDesc& scene) {
    for (const auto& source : scene.entities) {
        const Entity entity = registry.create();
        registry.add<Name>(entity, Name{source.name, true});
        registry.add<Transform>(entity, source.transform);

        if (!source.meshPath.empty()) {
            MeshRenderer renderer;
            renderer.mesh = meshIdForPath(source.meshPath);
            renderer.baseColor = source.material.baseColor;
            renderer.roughness = source.material.roughness;
            renderer.metallic = source.material.metallic;
            renderer.castShadows = source.castShadows;
            registry.add<MeshRenderer>(entity, renderer);
        }

        if (source.hasCamera) {
            Camera camera;
            camera.verticalFovDegrees = source.cameraFovDegrees;
            camera.primary = true;
            registry.add<Camera>(entity, camera);
        }

        if (source.hasRigidBody) {
            RigidBody body;
            body.isStatic = source.isStatic;
            body.restitution = source.restitution;
            body.inverseMass = source.isStatic ? 0.0f : (source.mass > 0.0f ? 1.0f / source.mass : 0.0f);
            if (source.colliderHalfExtents.has_value()) {
                body.halfExtents = *source.colliderHalfExtents;
            }
            registry.add<RigidBody>(entity, body);
        }

        if (!source.scriptClass.empty()) {
            registry.add<ScriptComponent>(entity, ScriptComponent{source.scriptClass, true});
        }
    }
}

JsonValue sceneToJson(const SceneDesc& scene) {
    JsonValue root = JsonValue::makeObject();
    root.set("name", scene.name);

    JsonValue environment = JsonValue::makeObject();
    JsonValue clearColor = JsonValue::makeArray();
    clearColor.push(scene.clearColor.r);
    clearColor.push(scene.clearColor.g);
    clearColor.push(scene.clearColor.b);
    clearColor.push(scene.clearColor.a);
    environment.set("clearColor", std::move(clearColor));

    JsonValue light = JsonValue::makeObject();
    JsonValue direction = JsonValue::makeArray();
    direction.push(scene.light.direction.x);
    direction.push(scene.light.direction.y);
    direction.push(scene.light.direction.z);
    light.set("direction", std::move(direction));
    light.set("intensity", scene.light.intensity);
    environment.set("light", std::move(light));
    root.set("environment", std::move(environment));

    JsonValue camera = JsonValue::makeObject();
    JsonValue cameraPosition = JsonValue::makeArray();
    cameraPosition.push(scene.cameraPosition.x);
    cameraPosition.push(scene.cameraPosition.y);
    cameraPosition.push(scene.cameraPosition.z);
    camera.set("position", std::move(cameraPosition));
    JsonValue cameraTarget = JsonValue::makeArray();
    cameraTarget.push(scene.cameraTarget.x);
    cameraTarget.push(scene.cameraTarget.y);
    cameraTarget.push(scene.cameraTarget.z);
    camera.set("target", std::move(cameraTarget));
    camera.set("fov", scene.cameraFovDegrees);
    root.set("camera", std::move(camera));

    JsonValue entities = JsonValue::makeArray();
    for (const auto& source : scene.entities) {
        JsonValue entity = JsonValue::makeObject();
        entity.set("name", source.name);

        JsonValue transform = JsonValue::makeObject();
        JsonValue position = JsonValue::makeArray();
        position.push(source.transform.position.x);
        position.push(source.transform.position.y);
        position.push(source.transform.position.z);
        transform.set("position", std::move(position));
        JsonValue scale = JsonValue::makeArray();
        scale.push(source.transform.scale.x);
        scale.push(source.transform.scale.y);
        scale.push(source.transform.scale.z);
        transform.set("scale", std::move(scale));
        entity.set("transform", std::move(transform));

        if (!source.meshPath.empty()) {
            entity.set("mesh", source.meshPath);
            JsonValue material = JsonValue::makeObject();
            JsonValue baseColor = JsonValue::makeArray();
            baseColor.push(source.material.baseColor.x);
            baseColor.push(source.material.baseColor.y);
            baseColor.push(source.material.baseColor.z);
            baseColor.push(source.material.baseColor.w);
            material.set("baseColor", std::move(baseColor));
            material.set("roughness", source.material.roughness);
            entity.set("material", std::move(material));
        }

        if (source.hasRigidBody) {
            JsonValue body = JsonValue::makeObject();
            body.set("static", source.isStatic);
            body.set("mass", source.mass);
            if (source.colliderHalfExtents.has_value()) {
                JsonValue extents = JsonValue::makeArray();
                extents.push(source.colliderHalfExtents->x);
                extents.push(source.colliderHalfExtents->y);
                extents.push(source.colliderHalfExtents->z);
                body.set("halfExtents", std::move(extents));
            }
            entity.set("rigidbody", std::move(body));
        }

        if (!source.scriptClass.empty()) {
            entity.set("script", source.scriptClass);
        }

        entities.push(std::move(entity));
    }
    root.set("entities", std::move(entities));
    return root;
}

} // namespace zy

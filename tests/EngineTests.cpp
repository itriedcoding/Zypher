#include "TestFramework.h"

#include "zypher/app/Application.h"
#include "zypher/assets/MeshBlob.h"
#include "zypher/assets/PackFile.h"
#include "zypher/core/Config.h"
#include "zypher/core/Jobs.h"
#include "zypher/core/Memory.h"
#include "zypher/ecs/Components.h"
#include "zypher/ecs/Registry.h"
#include "zypher/json/Json.h"
#include "zypher/math/Geometry.h"
#include "zypher/math/Matrix.h"
#include "zypher/math/Transform.h"
#include "zypher/nav/NavGrid.h"
#include "zypher/physics/PhysicsWorld.h"
#include "zypher/render/MeshPrimitives.h"
#include "zypher/render/PngWriter.h"
#include "zypher/render/SoftwareDevice.h"
#include "zypher/scene/Scene.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace zy;

namespace {

std::vector<u8> buildTestPack(bool corruptChecksum = false) {
    struct Input {
        std::string path;
        std::vector<u8> data;
    };
    std::vector<Input> inputs;
    inputs.push_back(Input{"data/a.txt", std::vector<u8>{'h', 'e', 'l', 'l', 'o'}});
    inputs.push_back(Input{"data/b.bin", std::vector<u8>{1, 2, 3, 4, 5, 6, 7}});

    std::string stringTable;
    std::vector<u32> nameOffsets;
    for (const auto& input : inputs) {
        nameOffsets.push_back(static_cast<u32>(stringTable.size()));
        stringTable += input.path;
    }

    const usize headerSize = kPackHeaderSize;
    const usize indexSize = inputs.size() * kPackIndexEntrySize;
    const u64 stringOffset = headerSize + indexSize;
    const u64 dataStart = stringOffset + stringTable.size();

    u64 dataOffset = dataStart;
    std::vector<std::array<u8, 32>> entries;
    for (usize index = 0; index < inputs.size(); ++index) {
        std::array<u8, 32> entry{};
        const u64 id = assetIdFromPath(inputs[index].path);
        std::memcpy(entry.data(), &id, 8);
        const u32 nameOffset = nameOffsets[index];
        std::memcpy(entry.data() + 8, &nameOffset, 4);
        const u32 nameLength = static_cast<u32>(inputs[index].path.size());
        std::memcpy(entry.data() + 12, &nameLength, 4);
        const u32 offset = static_cast<u32>(dataOffset);
        std::memcpy(entry.data() + 16, &offset, 4);
        const u32 size = static_cast<u32>(inputs[index].data.size());
        std::memcpy(entry.data() + 20, &size, 4);
        entry[24] = static_cast<u8>(AssetType::Raw);
        u32 checksum = crc32(ByteSpan{inputs[index].data.data(), inputs[index].data.size()});
        if (corruptChecksum && index == 0) {
            checksum ^= 0xFFFFu;
        }
        std::memcpy(entry.data() + 28, &checksum, 4);
        entries.push_back(entry);
        dataOffset += inputs[index].data.size();
    }

    std::vector<u8> file;
    file.resize(headerSize, 0);
    std::memcpy(file.data(), kPackMagic.data(), kPackMagic.size());
    const u32 version = kPackVersion;
    std::memcpy(file.data() + 8, &version, 4);
    const u32 flags = 0;
    std::memcpy(file.data() + 12, &flags, 4);
    const u32 assetCount = static_cast<u32>(inputs.size());
    std::memcpy(file.data() + 16, &assetCount, 4);
    const u32 reserved = 0;
    std::memcpy(file.data() + 20, &reserved, 4);
    const u64 indexOffset = headerSize;
    std::memcpy(file.data() + 24, &indexOffset, 8);
    const u64 stringOffsetValue = stringOffset;
    std::memcpy(file.data() + 32, &stringOffsetValue, 8);
    const u64 stringSize = stringTable.size();
    std::memcpy(file.data() + 40, &stringSize, 8);
    const u64 totalSize = dataOffset;
    std::memcpy(file.data() + 48, &totalSize, 8);

    for (const auto& entry : entries) {
        file.insert(file.end(), entry.begin(), entry.end());
    }
    file.insert(file.end(), stringTable.begin(), stringTable.end());
    for (const auto& input : inputs) {
        file.insert(file.end(), input.data.begin(), input.data.end());
    }

    // Patch total size now that it is known.
    const u64 finalSize = file.size();
    std::memcpy(file.data() + 48, &finalSize, 8);
    return file;
}

} // namespace

// --- math ------------------------------------------------------------------

ZY_TEST(math_vec_operations) {
    const Vec3 x = Vec3::right();
    const Vec3 y = Vec3::up();
    const Vec3 z = cross(x, y);
    EXPECT_NEAR(z.x, 0.0, 1e-6);
    EXPECT_NEAR(z.y, 0.0, 1e-6);
    EXPECT_NEAR(z.z, 1.0, 1e-6);
    EXPECT_NEAR(dot(x, y), 0.0, 1e-6);
    const Vec3 normalised = normalize(Vec3{3.0f, 0.0f, 4.0f});
    EXPECT_NEAR(normalised.length(), 1.0, 1e-6);
    EXPECT_NEAR(normalised.x, 0.6f, 1e-6);
}

ZY_TEST(math_matrix_identity_and_trs) {
    const Mat4 identity = Mat4::identity();
    const Mat4 product = identity * Mat4::translation(Vec3{1.0f, 2.0f, 3.0f});
    EXPECT_TRUE(approxEqual(product.transformPoint(Vec3::zero()), Vec3{1.0f, 2.0f, 3.0f}));

    Transform transform;
    transform.position = Vec3{2.0f, -1.0f, 0.5f};
    transform.rotation = Quat::fromAxisAngle(Vec3::up(), 1.57079632679f);
    transform.scale = Vec3{2.0f, 2.0f, 2.0f};
    const Mat4 matrix = transform.matrix();
    const Vec3 point = matrix.transformPoint(Vec3{1.0f, 0.0f, 0.0f});
    // Rotating +X by 90 degrees around +Y gives -Z.
    EXPECT_NEAR(point.x, 2.0, 1e-5);
    EXPECT_NEAR(point.y, -1.0, 1e-5);
    EXPECT_NEAR(point.z, -1.5, 1e-5);
}

ZY_TEST(math_matrix_inverse) {
    const Mat4 matrix = fromTRS(Vec3{1.5f, -2.0f, 4.0f}, Quat::fromEuler(Vec3{0.3f, 0.7f, -0.2f}),
                                Vec3{2.0f, 0.5f, 1.5f});
    const Mat4 identity = matrix * matrix.affineInverted();
    EXPECT_NEAR(identity.at(0, 0) - 1.0f, 0.0, 1e-4);
    EXPECT_NEAR(identity.at(1, 1) - 1.0f, 0.0, 1e-4);
    EXPECT_NEAR(identity.at(2, 2) - 1.0f, 0.0, 1e-4);
    EXPECT_NEAR(identity.at(0, 3), 0.0, 1e-4);
    EXPECT_NEAR(identity.at(1, 3), 0.0, 1e-4);
    EXPECT_NEAR(identity.at(2, 3), 0.0, 1e-4);

    const Mat4 general = matrix.inverted();
    const Mat4 generalIdentity = general * matrix;
    EXPECT_NEAR(generalIdentity.at(0, 0), 1.0, 1e-4);
    EXPECT_NEAR(generalIdentity.at(3, 3), 1.0, 1e-4);
}

ZY_TEST(math_projection_depth_range) {
    const Mat4 projection = Mat4::perspective(1.0f, 1.7f, 0.1f, 100.0f);
    const Vec4 nearPoint = projection.transform(Vec4{0.0f, 0.0f, -0.1f, 1.0f});
    const Vec4 farPoint = projection.transform(Vec4{0.0f, 0.0f, -100.0f, 1.0f});
    EXPECT_NEAR(nearPoint.z / nearPoint.w, 0.0, 1e-5);
    EXPECT_NEAR(farPoint.z / farPoint.w, 1.0, 1e-5);
    EXPECT_TRUE(nearPoint.w > 0.0f);
}

ZY_TEST(math_look_at) {
    const Mat4 view = Mat4::lookAt(Vec3{5.0f, 5.0f, 5.0f}, Vec3::zero(), Vec3::up());
    const Vec3 eye = view.transformPoint(Vec3{5.0f, 5.0f, 5.0f});
    EXPECT_NEAR(eye.x, 0.0, 1e-5);
    EXPECT_NEAR(eye.y, 0.0, 1e-5);
    EXPECT_NEAR(eye.z, 0.0, 1e-5);
    const Vec3 target = view.transformPoint(Vec3::zero());
    EXPECT_NEAR(target.z, -8.66, 0.01);
}

ZY_TEST(math_quaternion_rotation) {
    const Quat rotation = Quat::fromAxisAngle(Vec3::up(), 3.14159265f * 0.5f);
    const Vec3 forward = rotation.rotate(Vec3::forward());
    EXPECT_NEAR(forward.x, -1.0, 1e-5);
    EXPECT_NEAR(forward.y, 0.0, 1e-5);
    EXPECT_NEAR(forward.z, 0.0, 1e-5);
    const Quat slerped = Quat::slerp(Quat::identity(), rotation, 0.0f);
    EXPECT_NEAR(slerped.w, 1.0, 1e-5);
    const Vec3 roundTrip = rotation.rotateInverse(rotation.rotate(Vec3{1.0f, 2.0f, 3.0f}));
    EXPECT_TRUE(approxEqual(roundTrip, Vec3{1.0f, 2.0f, 3.0f}, 1e-5f));
}

ZY_TEST(math_geometry_ray_aabb) {
    const Aabb box = Aabb::fromCenterExtents(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    Ray ray;
    ray.origin = Vec3{0.0f, 0.0f, 5.0f};
    ray.direction = Vec3{0.0f, 0.0f, -1.0f};
    f32 distance = 0.0f;
    EXPECT_TRUE(intersectRayAabb(ray, box, distance));
    EXPECT_NEAR(distance, 4.0, 1e-5);
    ray.direction = Vec3{0.0f, 0.0f, 1.0f};
    EXPECT_FALSE(intersectRayAabb(ray, box, distance));
    const Aabb transformed = Aabb{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const Aabb moved = transformed.transformed(Mat4::translation(Vec3{2.0f, 0.0f, 0.0f}));
    EXPECT_NEAR(moved.min.x, 2.0, 1e-6);
}

// --- ECS -------------------------------------------------------------------

ZY_TEST(ecs_entity_lifecycle) {
    Registry registry;
    const Entity entity = registry.create();
    ASSERT_TRUE(registry.alive(entity));
    EXPECT_EQ(registry.entityCount(), 1U);
    registry.destroy(entity);
    EXPECT_FALSE(registry.alive(entity));

    const Entity reused = registry.create();
    EXPECT_EQ(reused.index, entity.index);
    EXPECT_NE(reused.generation, entity.generation);
    EXPECT_TRUE(registry.alive(reused));
    EXPECT_FALSE(registry.alive(entity));
}

ZY_TEST(ecs_components) {
    Registry registry;
    const Entity entity = registry.create();
    registry.add<Transform>(entity, Transform{Vec3{1.0f, 2.0f, 3.0f}, Quat::identity(), Vec3::one()});
    EXPECT_TRUE(registry.has<Transform>(entity));
    EXPECT_FALSE(registry.has<Name>(entity));
    registry.get<Transform>(entity).position.y = 7.0f;
    EXPECT_NEAR(registry.get<Transform>(entity).position.y, 7.0, 1e-6);
    registry.add<Name>(entity, Name{"player", true});
    registry.remove<Transform>(entity);
    EXPECT_FALSE(registry.has<Transform>(entity));
    EXPECT_TRUE(registry.has<Name>(entity));
}

ZY_TEST(ecs_each_queries) {
    Registry registry;
    for (int index = 0; index < 100; ++index) {
        const Entity entity = registry.create();
        registry.add<Transform>(entity);
        if (index % 2 == 0) {
            registry.add<MeshRenderer>(entity);
        }
    }
    EXPECT_EQ(registry.count<Transform>(), 100U);
    EXPECT_EQ((registry.count<Transform, MeshRenderer>()), 50U);

    int visited = 0;
    registry.each<Transform, MeshRenderer>([&](Entity, Transform& transform, MeshRenderer&) {
        transform.position.y += 1.0f;
        ++visited;
    });
    EXPECT_EQ(visited, 50);
}

ZY_TEST(ecs_structural_changes_during_iteration) {
    Registry registry;
    for (int index = 0; index < 10; ++index) {
        const Entity entity = registry.create();
        registry.add<Transform>(entity);
    }
    int spawned = 0;
    registry.each<Transform>([&](Entity entity, Transform&) {
        if (spawned < 3) {
            const Entity child = registry.create();
            registry.add<Transform>(child);
            registry.add<Name>(child, Name{"child", true});
            ++spawned;
        }
        registry.add<Name>(entity, Name{"tagged", true});
    });
    EXPECT_EQ(spawned, 3);
    EXPECT_EQ(registry.count<Name>(), 13U);
}

// --- JSON ------------------------------------------------------------------

ZY_TEST(json_parse_and_write) {
    const std::string text = R"({
        // comment
        "name": "demo",
        "count": 3,
        "scale": 0.25,
        "enabled": true,
        "nothing": null,
        "tags": ["a", "b"],
        "nested": {"value": -12.5e1}
    })";
    auto document = parseJson(text);
    ASSERT_TRUE(document.hasValue());
    const JsonValue& root = document.value();
    EXPECT_STR_EQ(root.getString("name", ""), "demo");
    EXPECT_EQ(root.getInt("count", 0), 3);
    EXPECT_NEAR(root.getFloat("scale", 0.0f), 0.25, 1e-6);
    EXPECT_TRUE(root.getBool("enabled", false));
    EXPECT_TRUE(root["nothing"].isNull());
    EXPECT_EQ(root["tags"].size(), 2U);
    EXPECT_STR_EQ(root["tags"].at(1).asString(), "b");
    EXPECT_NEAR(root["nested"].getFloat("value", 0.0f), -125.0, 1e-4);

    const std::string dumped = root.dump(true);
    auto reparsed = parseJson(dumped);
    ASSERT_TRUE(reparsed.hasValue());
    EXPECT_STR_EQ(reparsed.value().getString("name", ""), "demo");
    EXPECT_EQ(reparsed.value()["tags"].size(), 2U);
}

ZY_TEST(json_errors) {
    EXPECT_TRUE(parseJson("{").hasError());
    EXPECT_TRUE(parseJson("[1, 2,]").hasError());
    EXPECT_TRUE(parseJson("{\"a\": 1} trailing").hasError());
    EXPECT_TRUE(parseJson("\"unterminated").hasError());

    std::string deeplyNested;
    for (int index = 0; index < 200; ++index) {
        deeplyNested += '[';
    }
    EXPECT_TRUE(parseJson(deeplyNested).hasError());
}

ZY_TEST(json_unicode_escapes) {
    auto document = parseJson("{\"text\": \"caf\\u00e9 \\u2713\"}");
    ASSERT_TRUE(document.hasValue());
    EXPECT_STR_EQ(document.value().getString("text", ""), "caf\xC3\xA9 \xE2\x9C\x93");
}

ZY_TEST(json_vec_helpers) {
    auto document = parseJson("{\"position\": [1, 2, 3], \"colour\": {\"r\": 0.5, \"g\": 0.25}}");
    ASSERT_TRUE(document.hasValue());
    const Vec3 position = document.value().getVec3("position", Vec3::zero());
    EXPECT_TRUE(approxEqual(position, Vec3{1.0f, 2.0f, 3.0f}));
    const Vec4 colour = document.value().getVec4("colour", Vec4::one());
    EXPECT_NEAR(colour.x, 0.5, 1e-5);
    EXPECT_NEAR(colour.y, 0.25, 1e-5);
    EXPECT_NEAR(colour.w, 1.0, 1e-5);
}

// --- core ------------------------------------------------------------------

ZY_TEST(core_arena_allocator) {
    Arena arena(1024);
    for (int index = 0; index < 32; ++index) {
        const auto block = arena.allocateBytes(64);
        EXPECT_EQ(block.size(), 64U);
    }
    EXPECT_TRUE(arena.usedBytes() >= 32U * 64U);
    arena.reset();
    EXPECT_EQ(arena.usedBytes(), 0U);
    const auto block = arena.allocateBytes(128);
    EXPECT_EQ(block.size(), 128U);
}

ZY_TEST(core_job_system_parallel_for) {
    JobSystem jobs(2);
    std::atomic<int> counter{0};
    jobs.parallelFor(1000, [&counter](u32) { counter.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(counter.load(), 1000);

    std::vector<u32> values(64, 0);
    jobs.parallelFor(64, [&values](u32 index) { values[index] = index * 2; }, 4);
    EXPECT_EQ(values[0], 0U);
    EXPECT_EQ(values[63], 126U);
    jobs.waitIdle();
    EXPECT_TRUE(jobs.stats().executed > 0);
}

ZY_TEST(core_command_line) {
    const char* argv[] = {"zypher_runtime", "--scene", "a.zscene", "--steps=60", "--dump-frame", "positional"};
    CommandLine commandLine;
    commandLine.parse(6, argv);
    EXPECT_STR_EQ(commandLine.getString("scene", ""), "a.zscene");
    EXPECT_EQ(commandLine.getInt("steps", 0), 60);
    EXPECT_TRUE(commandLine.has("dump-frame"));
    ASSERT_TRUE(commandLine.positional().size() == 1);
    EXPECT_STR_EQ(commandLine.positional()[0], "positional");
}

ZY_TEST(core_ini_parsing) {
    const auto ini = IniFile::parse("[graphics]\nwidth = 1920\nvsync = false\n[game]\nname = \"Zypher Demo\"\n");
    EXPECT_EQ(ini.getInt("graphics.width", 0), 1920);
    EXPECT_FALSE(ini.getBool("graphics.vsync", true));
    EXPECT_STR_EQ(ini.getString("game.name", ""), "Zypher Demo");
}

// --- assets ----------------------------------------------------------------

ZY_TEST(assets_ids_and_crc) {
    const AssetId id = assetIdFromPath("models/cube.obj");
    EXPECT_EQ(id, assetIdFromPath("models/cube.obj"));
    EXPECT_NE(id, assetIdFromPath("models/Cube.obj"));
    const std::string text = "123456789";
    EXPECT_EQ(crc32(ByteSpan{reinterpret_cast<const u8*>(text.data()), text.size()}), 0xCBF43926U);
}

ZY_TEST(assets_mesh_blob_roundtrip) {
    const MeshData cube = makeCubeMesh(2.0f);
    const auto blob = encodeMeshBlob(cube);
    auto decoded = decodeMeshBlob(ByteSpan{blob.data(), blob.size()});
    ASSERT_TRUE(decoded.hasValue());
    EXPECT_EQ(decoded.value().vertices.size(), cube.vertices.size());
    EXPECT_EQ(decoded.value().indices.size(), cube.indices.size());
    EXPECT_NEAR(decoded.value().vertices[0].position.x, cube.vertices[0].position.x, 1e-6);

    auto truncated = decodeMeshBlob(ByteSpan{blob.data(), blob.size() - 4});
    EXPECT_TRUE(truncated.hasError());

    std::vector<u8> corrupted = blob;
    corrupted[0] = 'X';
    EXPECT_TRUE(decodeMeshBlob(ByteSpan{corrupted.data(), corrupted.size()}).hasError());
}

ZY_TEST(assets_pack_reader) {
    auto pack = PackFile::fromMemory(buildTestPack(), "test-pack", true);
    ASSERT_TRUE(pack.hasValue());
    EXPECT_EQ(pack.value().assetCount(), 2U);

    const PackAsset* asset = pack.value().findByPath("data/a.txt");
    ASSERT_TRUE(asset != nullptr);
    EXPECT_EQ(asset->type, AssetType::Raw);
    const ByteSpan bytes = pack.value().bytes(*asset);
    ASSERT_TRUE(bytes.size() == 5);
    EXPECT_EQ(static_cast<char>(bytes[0]), 'h');

    EXPECT_TRUE(pack.value().findByPath("data/missing.txt") == nullptr);
    EXPECT_TRUE(pack.value().find(assetIdFromPath("data/b.bin")) != nullptr);

    auto corrupted = PackFile::fromMemory(buildTestPack(true), "bad-pack", true);
    EXPECT_TRUE(corrupted.hasError());
    // Without verification the same data loads.
    EXPECT_TRUE(PackFile::fromMemory(buildTestPack(true), "bad-pack", false).hasValue());

    std::vector<u8> truncated = buildTestPack();
    truncated.resize(20);
    EXPECT_TRUE(PackFile::fromMemory(std::move(truncated), "truncated", true).hasError());
}

// --- scenegraph / physics / nav -------------------------------------------

ZY_TEST(scene_parse_and_instantiate) {
    auto document = parseJson(R"({
        "name": "test",
        "environment": {"clearColor": [0.1, 0.2, 0.3, 1.0]},
        "camera": {"position": [1, 2, 3], "target": [0, 1, 0], "fov": 70},
        "entities": [
            {"name": "ground", "mesh": "models/plane.obj", "transform": {"scale": [10, 1, 10]},
             "rigidbody": {"static": true, "halfExtents": [10, 0.5, 10]}},
            {"name": "box", "mesh": "models/cube.obj", "transform": {"position": [0, 4, 0]},
             "rigidbody": {"mass": 2.0}, "script": "Spinner"},
            {"name": "camera", "camera": {"fov": 65}}
        ]
    })");
    ASSERT_TRUE(document.hasValue());
    auto scene = parseScene(document.value());
    ASSERT_TRUE(scene.hasValue());
    EXPECT_EQ(scene.value().entities.size(), 3U);
    EXPECT_NEAR(scene.value().cameraFovDegrees, 70.0, 1e-5);
    EXPECT_NEAR(scene.value().clearColor.g, 0.2, 1e-5);
    EXPECT_TRUE(scene.value().entities[0].isStatic);
    EXPECT_NEAR(scene.value().entities[1].mass, 2.0, 1e-5);
    EXPECT_STR_EQ(scene.value().entities[1].scriptClass, "Spinner");

    Registry registry;
    instantiateScene(registry, scene.value());
    EXPECT_EQ(registry.entityCount(), 3U);
    EXPECT_EQ(registry.count<Transform>(), 3U);
    EXPECT_EQ(registry.count<MeshRenderer>(), 2U);
    EXPECT_EQ(registry.count<RigidBody>(), 2U);
    EXPECT_EQ(registry.count<Camera>(), 1U);
    EXPECT_EQ(registry.count<ScriptComponent>(), 1U);

    const JsonValue roundTrip = sceneToJson(scene.value());
    auto reparsed = parseScene(roundTrip);
    ASSERT_TRUE(reparsed.hasValue());
    EXPECT_EQ(reparsed.value().entities.size(), 3U);
}

ZY_TEST(physics_settles_on_ground) {
    Registry registry;
    PhysicsWorld world;
    PhysicsSettings settings;
    settings.gravity = Vec3{0.0f, -9.81f, 0.0f};
    world.configure(settings);

    const Entity ground = registry.create();
    registry.add<Transform>(ground);
    registry.add<RigidBody>(ground, RigidBody{});
    const Aabb groundBounds = Aabb::fromCenterExtents(Vec3{0.0f, -0.5f, 0.0f}, Vec3{10.0f, 0.5f, 10.0f});
    world.addStaticBox(ground, groundBounds);

    const Entity box = registry.create();
    Transform transform;
    transform.position = Vec3{0.0f, 3.0f, 0.0f};
    registry.add<Transform>(box, transform);
    registry.add<RigidBody>(box, RigidBody{});
    world.addDynamicBody(box, Vec3{0.5f, 0.5f, 0.5f});

    for (int step = 0; step < 240; ++step) {
        world.step(1.0f / 60.0f, registry);
    }

    const Transform& result = registry.get<Transform>(box);
    EXPECT_NEAR(result.position.y, 0.5, 0.02);
    EXPECT_TRUE(registry.get<RigidBody>(box).grounded);
    EXPECT_TRUE(world.lastContactCount() > 0);
}

ZY_TEST(physics_is_deterministic) {
    const auto simulate = [] {
        Registry registry;
        PhysicsWorld world;
        const Entity ground = registry.create();
        registry.add<Transform>(ground);
        registry.add<RigidBody>(ground, RigidBody{});
        world.addStaticBox(ground, Aabb::fromCenterExtents(Vec3{0.0f, -0.5f, 0.0f}, Vec3{8.0f, 0.5f, 8.0f}));
        std::vector<Entity> boxes;
        for (int index = 0; index < 5; ++index) {
            const Entity box = registry.create();
            Transform transform;
            transform.position = Vec3{static_cast<f32>(index) * 0.9f - 1.8f, 2.0f + static_cast<f32>(index), 0.0f};
            registry.add<Transform>(box, transform);
            registry.add<RigidBody>(box, RigidBody{});
            world.addDynamicBody(box, Vec3{0.4f, 0.4f, 0.4f});
            boxes.push_back(box);
        }
        for (int step = 0; step < 200; ++step) {
            world.step(1.0f / 60.0f, registry);
        }
        std::vector<f32> values;
        for (const Entity entity : boxes) {
            const Transform& transform = registry.get<Transform>(entity);
            values.push_back(transform.position.x);
            values.push_back(transform.position.y);
            values.push_back(transform.position.z);
        }
        return values;
    };

    const auto first = simulate();
    const auto second = simulate();
    ASSERT_TRUE(first.size() == second.size());
    for (usize index = 0; index < first.size(); ++index) {
        EXPECT_EQ(first[index], second[index]);
    }
}

ZY_TEST(nav_grid_path) {
    NavGrid grid(12, 12, 1.0f);
    const auto path = grid.findPath(Vec2{0.5f, 0.5f}, Vec2{11.5f, 11.5f});
    EXPECT_TRUE(path.size() >= 2);
    EXPECT_NEAR(path.front().x, 0.5, 1e-5);
    EXPECT_NEAR(path.back().y, 11.5, 1e-5);
    EXPECT_TRUE(grid.lastExpandedNodes() > 0);

    NavGrid blocked(12, 12, 1.0f);
    for (u32 row = 0; row < 10; ++row) {
        blocked.setWalkable(6, row, false);
    }
    const auto around = blocked.findPath(Vec2{0.5f, 0.5f}, Vec2{11.5f, 5.5f});
    EXPECT_TRUE(around.size() >= 3);

    NavGrid walled(12, 12, 1.0f);
    for (u32 row = 0; row < 12; ++row) {
        walled.setWalkable(6, row, false);
    }
    EXPECT_TRUE(walled.findPath(Vec2{0.5f, 11.5f}, Vec2{11.5f, 11.5f}).empty());

    NavGrid blockedGoal(8, 8, 1.0f);
    blockedGoal.setWalkable(7, 7, false);
    EXPECT_TRUE(blockedGoal.findPath(Vec2{0.5f, 0.5f}, Vec2{7.5f, 7.5f}).empty());
}

// --- rendering -------------------------------------------------------------

ZY_TEST(render_software_device) {
    RenderDeviceDesc desc;
    desc.width = 96;
    desc.height = 64;
    SoftwareDevice device;
    ASSERT_TRUE(device.initialize(desc).hasValue());
    ASSERT_TRUE(device.width() == 96 && device.height() == 64);

    const MeshData cube = makeCubeMesh(1.0f);
    MeshHandle handle;
    ASSERT_TRUE(device.createMesh(cube, handle).hasValue());
    EXPECT_TRUE(handle.valid());

    CameraDesc camera;
    camera.position = Vec3{2.5f, 2.0f, 3.5f};
    camera.view = Mat4::lookAt(camera.position, Vec3::zero(), Vec3::up());
    camera.projection = Mat4::perspective(0.9f, 96.0f / 64.0f, 0.1f, 50.0f);

    Material material;
    material.baseColor = Vec4{0.9f, 0.4f, 0.2f, 1.0f};

    device.beginFrame(ClearColor{0.02f, 0.03f, 0.05f, 1.0f});
    device.setCamera(camera);
    device.setLight(DirectionalLight{});
    device.drawMesh(handle, Mat4::identity(), material);
    device.endFrame();

    const auto pixels = device.readbackRgba();
    ASSERT_TRUE(pixels.has_value());
    ASSERT_TRUE(pixels->size() == 96U * 64U * 4U);

    bool foundLitPixel = false;
    for (usize index = 0; index < pixels->size(); index += 4) {
        if ((*pixels)[index] > 40 && (*pixels)[index] > (*pixels)[index + 2]) {
            foundLitPixel = true;
            break;
        }
    }
    EXPECT_TRUE(foundLitPixel);
    EXPECT_EQ(device.stats().drawCalls, 1U);
    EXPECT_EQ(device.stats().triangles, 12U);
}

ZY_TEST(render_is_deterministic) {
    const auto render = [] {
        SoftwareDevice device;
        RenderDeviceDesc desc;
        desc.width = 64;
        desc.height = 64;
        (void)device.initialize(desc);
        MeshHandle handle;
        (void)device.createMesh(makeUvSphereMesh(1.0f, 16, 12), handle);
        CameraDesc camera;
        camera.position = Vec3{0.0f, 0.0f, 4.0f};
        camera.view = Mat4::lookAt(camera.position, Vec3::zero(), Vec3::up());
        camera.projection = Mat4::perspective(1.0f, 1.0f, 0.1f, 100.0f);
        device.beginFrame(ClearColor{});
        device.setCamera(camera);
        device.drawMesh(handle, Mat4::identity(), Material{});
        device.endFrame();
        const auto pixels = device.readbackRgba();
        return pixels.has_value() ? fnv1a64(pixels->data(), pixels->size()) : u64{0};
    };
    const u64 first = render();
    const u64 second = render();
    EXPECT_NE(first, 0U);
    EXPECT_EQ(first, second);
}

ZY_TEST(render_png_encoding) {
    const u32 width = 8;
    const u32 height = 4;
    std::vector<u8> pixels(static_cast<usize>(width) * height * 4, 255);
    for (usize index = 0; index < pixels.size(); index += 4) {
        pixels[index] = 200;
        pixels[index + 1] = 40;
        pixels[index + 2] = 20;
    }
    const auto png = encodePng(width, height, ByteSpan{pixels.data(), pixels.size()});
    ASSERT_TRUE(png.size() > 64);
    const u8 signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    EXPECT_TRUE(std::memcmp(png.data(), signature, 8) == 0);
    // IHDR width/height are big endian at byte 16 and 20.
    EXPECT_EQ(png[16], 0U);
    EXPECT_EQ(png[19], static_cast<u8>(width));
    EXPECT_EQ(png[23], static_cast<u8>(height));
    EXPECT_TRUE(encodePng(0, 0, ByteSpan{}).empty());
}

// --- runtime integration ---------------------------------------------------

ZY_TEST(runtime_headless_run_is_deterministic) {
    const auto runOnce = [] {
        AppOptions options;
        options.scenePath = ZYPHER_TEST_DEMO_SCENE;
        options.outputDirectory = "runtime-test-output";
        options.width = 64;
        options.height = 48;
        options.frames = 2;
        options.simulationSteps = 90;
        options.writeReport = false;
        options.dumpFrame = false;
        options.deterministic = true;
        options.backend = RenderBackend::Software;

        Application application;
        const auto status = application.initialize(options);
        EXPECT_TRUE(status.hasValue());
        const auto report = application.run();
        EXPECT_TRUE(report.hasValue());
        if (report.hasError()) {
            return AppReport{};
        }
        AppReport copy = report.value();
        application.shutdown();
        return copy;
    };

    const AppReport first = runOnce();
    const AppReport second = runOnce();
    EXPECT_TRUE(first.entities >= 4U);
    EXPECT_TRUE(first.triangles > 0U);
    EXPECT_NE(first.renderChecksum, 0U);
    EXPECT_EQ(first.simulationChecksum, second.simulationChecksum);
    EXPECT_EQ(first.renderChecksum, second.renderChecksum);
    EXPECT_TRUE(first.averageFrameMilliseconds >= 0.0);
}

#include "zypher/assets/MeshBlob.h"

#include <cstring>

namespace zy {
namespace {

template <class T>
void appendRaw(std::vector<u8>& buffer, const T& value) {
    const auto* bytes = reinterpret_cast<const u8*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template <class T>
T readRaw(const u8* pointer) {
    T value{};
    std::memcpy(&value, pointer, sizeof(T));
    return value;
}

} // namespace

Result<MeshData> decodeMeshBlob(ByteSpan data) {
    if (data.size() < kMeshBlobHeaderSize) {
        return Error{ErrorCode::Parse, "mesh blob is truncated"};
    }
    if (std::memcmp(data.data(), kMeshBlobMagic.data(), kMeshBlobMagic.size()) != 0) {
        return Error{ErrorCode::Parse, "mesh blob has bad magic"};
    }
    const u32 version = readRaw<u32>(data.data() + 8);
    if (version != kMeshBlobVersion) {
        return Error{ErrorCode::Parse, "unsupported mesh blob version " + std::to_string(version)};
    }
    const u32 vertexCount = readRaw<u32>(data.data() + 12);
    const u32 indexCount = readRaw<u32>(data.data() + 16);
    const u32 flags = readRaw<u32>(data.data() + 20);

    const usize expected = kMeshBlobHeaderSize + static_cast<usize>(vertexCount) * kMeshVertexStride +
                           static_cast<usize>(indexCount) * sizeof(u32);
    if (data.size() != expected) {
        return Error{ErrorCode::Parse, "mesh blob size mismatch (expected " + std::to_string(expected) +
                                          ", got " + std::to_string(data.size()) + ")"};
    }
    if (indexCount % 3 != 0) {
        return Error{ErrorCode::Parse, "mesh blob index count is not a multiple of three"};
    }

    MeshData mesh;
    mesh.vertices.resize(vertexCount);
    mesh.indices.resize(indexCount);

    const u8* cursor = data.data() + kMeshBlobHeaderSize;
    for (u32 index = 0; index < vertexCount; ++index) {
        Vertex& vertex = mesh.vertices[index];
        vertex.position = Vec3{readRaw<f32>(cursor), readRaw<f32>(cursor + 4), readRaw<f32>(cursor + 8)};
        if ((flags & 1U) != 0U) {
            vertex.normal = Vec3{readRaw<f32>(cursor + 12), readRaw<f32>(cursor + 16),
                                 readRaw<f32>(cursor + 20)};
        } else {
            vertex.normal = Vec3::up();
        }
        if ((flags & 2U) != 0U) {
            vertex.uv = Vec2{readRaw<f32>(cursor + 24), readRaw<f32>(cursor + 28)};
        }
        cursor += kMeshVertexStride;
    }

    if (indexCount > 0) {
        std::memcpy(mesh.indices.data(), cursor, static_cast<usize>(indexCount) * sizeof(u32));
    }

    for (const u32 index : mesh.indices) {
        if (index >= vertexCount) {
            return Error{ErrorCode::Parse, "mesh blob references a vertex that does not exist"};
        }
    }
    return mesh;
}

std::vector<u8> encodeMeshBlob(const MeshData& mesh) {
    std::vector<u8> buffer;
    buffer.reserve(kMeshBlobHeaderSize + mesh.vertices.size() * kMeshVertexStride +
                   mesh.indices.size() * sizeof(u32));
    buffer.insert(buffer.end(), kMeshBlobMagic.begin(), kMeshBlobMagic.end());
    appendRaw(buffer, kMeshBlobVersion);
    appendRaw(buffer, static_cast<u32>(mesh.vertices.size()));
    appendRaw(buffer, static_cast<u32>(mesh.indices.size()));
    const u32 flags = 1U | 2U;
    appendRaw(buffer, flags);

    const Aabb bounds = mesh.bounds();
    appendRaw(buffer, bounds.min.x);
    appendRaw(buffer, bounds.min.y);
    appendRaw(buffer, bounds.min.z);
    appendRaw(buffer, bounds.max.x);
    appendRaw(buffer, bounds.max.y);
    appendRaw(buffer, bounds.max.z);

    for (const auto& vertex : mesh.vertices) {
        appendRaw(buffer, vertex.position.x);
        appendRaw(buffer, vertex.position.y);
        appendRaw(buffer, vertex.position.z);
        appendRaw(buffer, vertex.normal.x);
        appendRaw(buffer, vertex.normal.y);
        appendRaw(buffer, vertex.normal.z);
        appendRaw(buffer, vertex.uv.x);
        appendRaw(buffer, vertex.uv.y);
    }
    if (!mesh.indices.empty()) {
        const auto* bytes = reinterpret_cast<const u8*>(mesh.indices.data());
        buffer.insert(buffer.end(), bytes, bytes + mesh.indices.size() * sizeof(u32));
    }
    return buffer;
}

} // namespace zy

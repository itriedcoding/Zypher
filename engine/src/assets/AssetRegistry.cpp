#include "zypher/assets/AssetRegistry.h"

#include "zypher/assets/MeshBlob.h"
#include "zypher/core/Log.h"

namespace zy {

const PackAsset* AssetRegistry::find(AssetId id) const {
    return m_pack != nullptr ? m_pack->find(id) : nullptr;
}

const PackAsset* AssetRegistry::findByPath(std::string_view path) const {
    return m_pack != nullptr ? m_pack->findByPath(path) : nullptr;
}

bool AssetRegistry::contains(AssetId id) const { return find(id) != nullptr; }

Result<MeshData> AssetRegistry::loadMesh(AssetId id) {
    if (const auto iterator = m_meshCache.find(id); iterator != m_meshCache.end()) {
        return iterator->second;
    }
    const PackAsset* asset = find(id);
    if (asset == nullptr) {
        return Error{ErrorCode::NotFound, "mesh asset " + assetIdToString(id) + " is not in the pack"};
    }
    if (asset->type != AssetType::Mesh) {
        return Error{ErrorCode::InvalidArgument, "'" + asset->path + "' is not a mesh asset"};
    }
    auto mesh = decodeMeshBlob(m_pack->bytes(*asset));
    if (mesh.hasError()) {
        return Error{mesh.error().code, "'" + asset->path + "': " + mesh.error().message};
    }
    auto [iterator, inserted] = m_meshCache.emplace(id, std::move(mesh).value());
    (void)inserted;
    return iterator->second;
}

Result<ByteSpan> AssetRegistry::loadRaw(AssetId id) {
    const PackAsset* asset = find(id);
    if (asset == nullptr) {
        return Error{ErrorCode::NotFound, "asset " + assetIdToString(id) + " is not in the pack"};
    }
    return m_pack->bytes(*asset);
}

Result<std::string> AssetRegistry::loadText(AssetId id) {
    auto raw = loadRaw(id);
    if (raw.hasError()) {
        return Error{raw.error().code, raw.error().message};
    }
    const auto bytes = raw.value();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void AssetRegistry::clearCache() { m_meshCache.clear(); }

AssetRegistry::CacheStats AssetRegistry::cacheStats() const {
    CacheStats stats;
    stats.meshes = m_meshCache.size();
    for (const auto& entry : m_meshCache) {
        stats.cachedBytes += entry.second.vertices.size() * sizeof(Vertex) +
                             entry.second.indices.size() * sizeof(u32);
    }
    return stats;
}

} // namespace zy

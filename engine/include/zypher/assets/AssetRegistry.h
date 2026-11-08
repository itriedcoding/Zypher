// Runtime asset access: resolves ids through the pack file, decodes cooked
// blobs on demand and caches them.
#pragma once

#include "zypher/assets/PackFile.h"
#include "zypher/core/Result.h"
#include "zypher/render/RenderTypes.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace zy {

class AssetRegistry final : NonCopyable {
public:
    AssetRegistry() = default;

    // The registry does not own the pack; it must outlive the registry.
    void setPack(const PackFile* pack) { m_pack = pack; }

    [[nodiscard]] const PackFile* pack() const { return m_pack; }
    [[nodiscard]] const PackAsset* find(AssetId id) const;
    [[nodiscard]] const PackAsset* findByPath(std::string_view path) const;
    [[nodiscard]] bool contains(AssetId id) const;

    [[nodiscard]] Result<MeshData> loadMesh(AssetId id);
    [[nodiscard]] Result<ByteSpan> loadRaw(AssetId id);
    [[nodiscard]] Result<std::string> loadText(AssetId id);

    void clearCache();

    struct CacheStats {
        usize meshes = 0;
        usize cachedBytes = 0;
    };
    [[nodiscard]] CacheStats cacheStats() const;

private:
    const PackFile* m_pack = nullptr;
    std::unordered_map<AssetId, MeshData> m_meshCache;
};

} // namespace zy

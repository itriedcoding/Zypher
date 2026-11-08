// Runtime reader for the .zpkg pack file produced by `zypher-assetcooker`.
//
// Layout (little endian):
//   header (56 bytes)
//     0  char magic[8]     "ZYPKG001"
//     8  u32  version      1
//     12 u32  flags
//     16 u32  assetCount
//     20 u32  reserved
//     24 u64  indexOffset
//     32 u64  stringOffset
//     40 u64  stringSize
//     48 u64  totalSize
//   index entries (assetCount * 32 bytes, sorted by id)
//     0  u64 id
//     8  u32 nameOffset   (relative to stringOffset)
//     12 u32 nameLength
//     16 u32 dataOffset
//     20 u32 dataSize
//     24 u8  type
//     25 u8  reserved[3]
//     28 u32 checksum      (CRC-32 of the data)
//   string table (UTF-8 asset paths)
//   data blobs
#pragma once

#include "zypher/assets/AssetId.h"
#include "zypher/core/Result.h"

#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace zy {

inline constexpr std::string_view kPackMagic = "ZYPKG001";
inline constexpr u32 kPackVersion = 1;
inline constexpr usize kPackHeaderSize = 56;
inline constexpr usize kPackIndexEntrySize = 32;

struct PackAsset {
    AssetId id = kInvalidAsset;
    AssetType type = AssetType::Unknown;
    std::string path;
    u32 offset = 0;
    u32 size = 0;
    u32 checksum = 0;
};

[[nodiscard]] u32 crc32(ByteSpan data);

class PackFile final {
public:
    PackFile() = default;
    ~PackFile() = default;
    PackFile(const PackFile&) = delete;
    PackFile& operator=(const PackFile&) = delete;
    PackFile(PackFile&&) noexcept = default;
    PackFile& operator=(PackFile&&) noexcept = default;

    [[nodiscard]] static Result<PackFile> load(const std::string& path, bool verifyChecksums = true);
    [[nodiscard]] static Result<PackFile> fromMemory(std::vector<u8> data, std::string sourceName,
                                                     bool verifyChecksums = true);

    [[nodiscard]] bool valid() const { return !m_data.empty(); }
    [[nodiscard]] usize assetCount() const { return m_assets.size(); }
    [[nodiscard]] const std::vector<PackAsset>& assets() const { return m_assets; }
    [[nodiscard]] const std::string& sourceName() const { return m_sourceName; }

    [[nodiscard]] const PackAsset* find(AssetId id) const;
    [[nodiscard]] const PackAsset* findByPath(std::string_view path) const;

    [[nodiscard]] ByteSpan bytes(const PackAsset& asset) const;
    [[nodiscard]] ByteSpan bytes(AssetId id) const;

private:
    std::vector<u8> m_data;
    std::vector<PackAsset> m_assets;
    std::unordered_map<u64, usize> m_byId;
    std::unordered_map<std::string, usize> m_byPath;
    std::string m_sourceName;
};

} // namespace zy

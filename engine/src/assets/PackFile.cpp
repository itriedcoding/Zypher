#include "zypher/assets/PackFile.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"

#include <array>
#include <cstring>

namespace zy {
namespace {

std::array<u32, 256> buildCrcTable() {
    std::array<u32, 256> table{};
    for (u32 index = 0; index < 256; ++index) {
        u32 value = index;
        for (u32 bit = 0; bit < 8; ++bit) {
            value = (value & 1u) != 0u ? (value >> 1) ^ 0xEDB88320u : value >> 1;
        }
        table[index] = value;
    }
    return table;
}

[[nodiscard]] const std::array<u32, 256>& crcTable() {
    static const std::array<u32, 256> table = buildCrcTable();
    return table;
}

[[nodiscard]] u32 readU32(const u8* pointer) {
    u32 value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

[[nodiscard]] u64 readU64(const u8* pointer) {
    u64 value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

} // namespace

u32 crc32(ByteSpan data) {
    const auto& table = crcTable();
    u32 crc = 0xFFFFFFFFu;
    for (const u8 value : data) {
        crc = table[(crc ^ value) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

Result<PackFile> PackFile::load(const std::string& path, bool verifyChecksums) {
    auto fileData = FileSystem::readBinary(path);
    if (!fileData.has_value()) {
        return Error{ErrorCode::NotFound, "cannot read pack file '" + path + "'"};
    }
    if (fileData->empty()) {
        return Error{ErrorCode::Parse, "pack file '" + path + "' is empty"};
    }
    return fromMemory(std::move(*fileData), path, verifyChecksums);
}

Result<PackFile> PackFile::fromMemory(std::vector<u8> data, std::string sourceName, bool verifyChecksums) {
    PackFile pack;
    pack.m_sourceName = std::move(sourceName);
    pack.m_data = std::move(data);

    const usize totalSize = pack.m_data.size();
    const auto fail = [&pack](std::string message) {
        return Error{ErrorCode::Parse, "'" + pack.m_sourceName + "': " + std::move(message)};
    };

    if (totalSize < kPackHeaderSize) {
        return fail("file is smaller than the pack header");
    }
    const u8* base = pack.m_data.data();
    if (std::memcmp(base, kPackMagic.data(), kPackMagic.size()) != 0) {
        return fail("bad magic (expected ZYPKG001)");
    }
    const u32 version = readU32(base + 8);
    if (version != kPackVersion) {
        return fail("unsupported pack version " + std::to_string(version));
    }
    const u32 assetCount = readU32(base + 16);
    const u64 indexOffset = readU64(base + 24);
    const u64 stringOffset = readU64(base + 32);
    const u64 stringSize = readU64(base + 40);
    const u64 declaredSize = readU64(base + 48);

    if (declaredSize != static_cast<u64>(totalSize)) {
        return fail("declared size does not match the file size");
    }
    if (indexOffset < kPackHeaderSize || indexOffset > totalSize) {
        return fail("index offset out of range");
    }
    if (static_cast<u64>(assetCount) * kPackIndexEntrySize > totalSize - indexOffset) {
        return fail("index is truncated");
    }
    if (stringOffset > totalSize || stringSize > totalSize - stringOffset) {
        return fail("string table out of range");
    }

    pack.m_assets.reserve(assetCount);
    for (u32 index = 0; index < assetCount; ++index) {
        const u8* entry = base + indexOffset + static_cast<u64>(index) * kPackIndexEntrySize;
        PackAsset asset;
        asset.id = readU64(entry);
        const u32 nameOffset = readU32(entry + 8);
        const u32 nameLength = readU32(entry + 12);
        asset.offset = readU32(entry + 16);
        asset.size = readU32(entry + 20);
        asset.type = static_cast<AssetType>(entry[24]);
        asset.checksum = readU32(entry + 28);

        if (asset.id == kInvalidAsset) {
            return fail("asset with a zero id");
        }
        if (static_cast<u64>(nameOffset) + nameLength > stringSize) {
            return fail("asset name out of range");
        }
        if (static_cast<u64>(asset.offset) + asset.size > totalSize) {
            return fail("asset data out of range");
        }
        asset.path.assign(reinterpret_cast<const char*>(base + stringOffset + nameOffset), nameLength);
        if (asset.path.empty()) {
            return fail("asset with an empty path");
        }
        if (asset.size > 0 && verifyChecksums) {
            const ByteSpan payload{base + asset.offset, asset.size};
            const u32 actual = crc32(payload);
            if (actual != asset.checksum) {
                return fail("checksum mismatch for '" + asset.path + "'");
            }
        }
        pack.m_byId.emplace(asset.id, pack.m_assets.size());
        pack.m_byPath.emplace(asset.path, pack.m_assets.size());
        pack.m_assets.push_back(std::move(asset));
    }

    ZY_LOG_INFO("assets", "loaded pack '", pack.m_sourceName, "' with ", pack.m_assets.size(), " assets (",
                totalSize, " bytes)");
    return pack;
}

const PackAsset* PackFile::find(AssetId id) const {
    const auto iterator = m_byId.find(id);
    return iterator == m_byId.end() ? nullptr : &m_assets[iterator->second];
}

const PackAsset* PackFile::findByPath(std::string_view path) const {
    const auto iterator = m_byPath.find(std::string(path));
    return iterator == m_byPath.end() ? nullptr : &m_assets[iterator->second];
}

ByteSpan PackFile::bytes(const PackAsset& asset) const {
    if (asset.size == 0 || asset.offset + asset.size > m_data.size()) {
        return {};
    }
    return ByteSpan{m_data.data() + asset.offset, asset.size};
}

ByteSpan PackFile::bytes(AssetId id) const {
    const PackAsset* asset = find(id);
    return asset != nullptr ? bytes(*asset) : ByteSpan{};
}

} // namespace zy

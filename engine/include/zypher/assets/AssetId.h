// Stable asset identifiers and types shared by the engine and the Rust cooker.
#pragma once

#include "zypher/core/Base.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace zy {

using AssetId = u64;

inline constexpr AssetId kInvalidAsset = 0;

// Asset ids are the FNV-1a 64 hash of the project relative path with forward
// slashes, e.g. "models/cube.obj". The Rust asset cooker uses the exact same
// algorithm so ids are stable across the whole toolchain.
[[nodiscard]] constexpr AssetId assetIdFromPath(std::string_view path) {
    return fnv1a64(path);
}

[[nodiscard]] inline std::string assetIdToString(AssetId id) {
    char buffer[20]{};
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(id));
    return std::string(buffer);
}

// Values are part of the .zpkg binary format - do not renumber.
enum class AssetType : u8 {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Audio = 3,
    Scene = 4,
    Shader = 5,
    Json = 6,
    Font = 7,
    Raw = 8,
};

[[nodiscard]] constexpr std::string_view assetTypeName(AssetType type) {
    switch (type) {
    case AssetType::Mesh: return "mesh";
    case AssetType::Texture: return "texture";
    case AssetType::Audio: return "audio";
    case AssetType::Scene: return "scene";
    case AssetType::Shader: return "shader";
    case AssetType::Json: return "json";
    case AssetType::Font: return "font";
    case AssetType::Raw: return "raw";
    case AssetType::Unknown: break;
    }
    return "unknown";
}

} // namespace zy

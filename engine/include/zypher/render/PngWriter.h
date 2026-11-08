// Minimal PNG encoder (uncompressed deflate blocks). Used for headless frame
// dumps and editor thumbnails; it produces standards compliant PNG files
// without linking zlib.
#pragma once

#include "zypher/core/Base.h"

#include <span>
#include <string>
#include <vector>

namespace zy {

[[nodiscard]] std::vector<u8> encodePng(u32 width, u32 height, std::span<const u8> rgba);
[[nodiscard]] bool writePng(const std::string& path, u32 width, u32 height, std::span<const u8> rgba);

} // namespace zy

// Cooked mesh blob ("ZYMESH01"), written by the Rust asset cooker and decoded
// by the runtime. Layout:
//   0  char magic[8]    "ZYMESH01"
//   8  u32 version      1
//   12 u32 vertexCount
//   16 u32 indexCount
//   20 u32 flags        1 = has normals, 2 = has uvs
//   24 f32 boundsMin[3]
//   36 f32 boundsMax[3]
//   48 vertices          32 bytes each (position, normal, uv)
//      indices           u32 each
#pragma once

#include "zypher/core/Result.h"
#include "zypher/render/RenderTypes.h"

#include <vector>

namespace zy {

inline constexpr std::string_view kMeshBlobMagic = "ZYMESH01";
inline constexpr u32 kMeshBlobVersion = 1;
inline constexpr usize kMeshBlobHeaderSize = 48;
inline constexpr usize kMeshVertexStride = 32;

[[nodiscard]] Result<MeshData> decodeMeshBlob(ByteSpan data);
[[nodiscard]] std::vector<u8> encodeMeshBlob(const MeshData& mesh);

} // namespace zy

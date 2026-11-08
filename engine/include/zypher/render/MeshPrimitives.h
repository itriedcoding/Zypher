// Procedural meshes used by tests, the editor's preview viewport and as
// fallbacks when a scene references a mesh that is missing from the pack.
#pragma once

#include "zypher/render/RenderTypes.h"

namespace zy {

[[nodiscard]] MeshData makeCubeMesh(f32 size = 1.0f);
[[nodiscard]] MeshData makePlaneMesh(f32 size = 1.0f, u32 subdivisions = 1);
[[nodiscard]] MeshData makeUvSphereMesh(f32 radius = 0.5f, u32 segments = 24, u32 rings = 16);

} // namespace zy

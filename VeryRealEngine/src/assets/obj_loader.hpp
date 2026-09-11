/**
 * @file obj_loader.hpp
 * @brief Minimal Wavefront .OBJ (+ .MTL) loader.
 *
 * Written from scratch for VeryRealEngine — no third-party OBJ library —
 * consistent with the subject's "no non-system library" rule. Supports the
 * subset the engine actually needs: positions/normals/uvs, n-gon faces
 * (triangulated as a fan), `usemtl` material grouping into submeshes, and
 * `mtllib` diffuse color / diffuse texture map.
 */
#pragma once

#include "mesh_data.hpp"

namespace vre
{

/**
 * @brief Loads `path` into `out_mesh` and appends any materials referenced
 * via `mtllib` to `out_materials`.
 *
 * Paths inside `out_materials` are resolved relative to the .obj's
 * directory, ready to hand to the texture loader.
 * @param path Filesystem path to the .obj file.
 * @param out_mesh Receives the parsed geometry.
 * @param out_materials Materials referenced by the file are appended here.
 * @return true on success; false (leaving *out_mesh partially filled) on parse/IO failure.
 */
bool load_obj(const char *path, MeshData *out_mesh, std::vector<MaterialData> *out_materials);

} // namespace vre

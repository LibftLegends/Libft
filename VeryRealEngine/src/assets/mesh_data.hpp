/**
 * @file mesh_data.hpp
 * @brief CPU-side mesh/material data shared by the OBJ loader and the
 * renderer's GPU upload path.
 *
 * Kept separate from renderer.hpp so asset loading has no Vulkan
 * dependency at all.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vre
{

/**
 * @brief One vertex: position, normal, UV, plus up-to-4-bone GPU
 * linear-blend skinning data, matching the layout the graphics pipeline
 * expects (see mesh.vert's skinning computation).
 *
 * `bone_indices`/`bone_weights` exist for every vertex, not just skinned
 * meshes: an ordinary static mesh (anything loaded via obj_loader.cpp) is
 * simply bound entirely to bone slot 0 with weight 1.0, and
 * Renderer::GlobalUbo::bone_matrices[0] is always the identity matrix — so
 * `skin_matrix` in mesh.vert reduces to the identity for every vertex that
 * doesn't actually belong to an animated skeleton, and static geometry is
 * unaffected. This is what lets one shader/pipeline serve both static and
 * skinned meshes rather than needing two.
 */
struct MeshVertex
{
    float position[3];
    float normal[3];
    float uv[2];
    float bone_indices[4] = {0.0f, 0.0f, 0.0f, 0.0f}; ///< Indices into GlobalUbo::bone_matrices.
    float bone_weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};  ///< Must sum to 1.0 per vertex.
};

/// One contiguous run of indices in the mesh's index buffer that shares a single material — i.e. what `usemtl` splits an OBJ file into.
struct SubMesh
{
    uint32_t index_offset;
    uint32_t index_count;
    int32_t material_index; ///< -1 => no material (untextured default).
};

/// Parsed geometry for one mesh: vertices, indices, and per-material submesh ranges.
struct MeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> submeshes;
};

/// Parsed .mtl material data.
struct MaterialData
{
    std::string name;
    float diffuse_color[3] = {1.0f, 1.0f, 1.0f};
    std::string diffuse_texture_path; ///< Empty => untextured.

    /**
     * PBR roughness/metallic workflow (the "Pr"/"Pm" extension to the
     * classic .mtl format, used by Blender's OBJ exporter and others) —
     * this is what actually distinguishes "wood-like" from "metallic"
     * materials in the shading model, not just a different diffuse texture.
     * Defaults describe an ordinary rough dielectric (e.g. unfinished wood).
     */
    float roughness = 0.8f; ///< 0 = mirror-smooth, 1 = fully rough.
    float metallic = 0.0f;  ///< 0 = dielectric (wood, plastic, ...), 1 = metal.
};

} // namespace vre

// CPU-side mesh/material data shared by the OBJ loader and the renderer's
// GPU upload path. Kept separate from renderer.hpp so asset loading has no
// Vulkan dependency at all.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vre
{

struct MeshVertex
{
    float position[3];
    float normal[3];
    float uv[2];
};

// One contiguous run of indices in the mesh's index buffer that shares a
// single material — i.e. what `usemtl` splits an OBJ file into.
struct SubMesh
{
    uint32_t index_offset;
    uint32_t index_count;
    int32_t material_index; // -1 => no material (untextured default)
};

struct MeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> submeshes;
};

struct MaterialData
{
    std::string name;
    float diffuse_color[3] = {1.0f, 1.0f, 1.0f};
    std::string diffuse_texture_path; // empty => untextured

    // PBR roughness/metallic workflow (the "Pr"/"Pm" extension to the
    // classic .mtl format, used by Blender's OBJ exporter and others) —
    // this is what actually distinguishes "wood-like" from "metallic"
    // materials in the shading model, not just a different diffuse texture.
    // Defaults describe an ordinary rough dielectric (e.g. unfinished wood).
    float roughness = 0.8f; // 0 = mirror-smooth, 1 = fully rough
    float metallic = 0.0f;  // 0 = dielectric (wood, plastic, ...), 1 = metal
};

} // namespace vre

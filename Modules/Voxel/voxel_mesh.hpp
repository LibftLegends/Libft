#ifndef VOXEL_MESH_HPP
# define VOXEL_MESH_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "../Template/vector.hpp"
# include "../Errno/errno.hpp"
# include "voxel_lighting.hpp"
# include <stdint.h>

class game_voxel_chunk;
struct geometry_frustum;

enum chunk_mesh_face
{
    CHUNK_MESH_FACE_WEST = 0,
    CHUNK_MESH_FACE_EAST = 1,
    CHUNK_MESH_FACE_DOWN = 2,
    CHUNK_MESH_FACE_UP = 3,
    CHUNK_MESH_FACE_NORTH = 4,
    CHUNK_MESH_FACE_SOUTH = 5
};

struct chunk_mesh_vertex
{
    uint16_t    coordinate_x;
    uint16_t    coordinate_y;
    uint16_t    coordinate_z;
    uint16_t    texture_u;
    uint16_t    texture_v;
    uint8_t     face;
    uint8_t     packed_light;
    uint32_t    block_id;
};

struct chunk_mesh_bounds
{
    int32_t minimum_x;
    int32_t minimum_y;
    int32_t minimum_z;
    int32_t maximum_x;
    int32_t maximum_y;
    int32_t maximum_z;
};

struct chunk_mesh
{
    ft_vector<chunk_mesh_vertex> vertices;
    ft_vector<uint32_t>          indices;
    chunk_mesh_bounds            bounds;
    chunk_mesh_bounds            occupied_bounds;
    ft_bool                      has_occupied_bounds;
};

int32_t chunk_mesh_initialize(chunk_mesh &mesh) noexcept;
int32_t chunk_mesh_destroy(chunk_mesh &mesh) noexcept;
int32_t chunk_mesh_clear(chunk_mesh &mesh) noexcept;
int32_t chunk_mesh_generate_from_chunk(chunk_mesh &mesh,
    const game_voxel_chunk &chunk) noexcept;
int32_t chunk_mesh_generate_from_chunk_with_light(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, const voxel_light_chunk &light) noexcept;
int32_t chunk_mesh_apply_light(chunk_mesh &mesh,
    const voxel_light_chunk &light) noexcept;
int32_t chunk_mesh_generate_from_chunk_with_neighbors(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, int32_t chunk_x, int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light = nullptr) noexcept;
ft_bool chunk_mesh_intersects_frustum(const geometry_frustum &frustum,
    int32_t world_origin_x, int32_t world_origin_y,
    int32_t world_origin_z) noexcept;
ft_bool chunk_mesh_intersects_frustum(const geometry_frustum &frustum,
    const chunk_mesh &mesh, int32_t world_origin_x, int32_t world_origin_y,
    int32_t world_origin_z) noexcept;

#endif

#endif

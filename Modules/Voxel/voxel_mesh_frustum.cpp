#include "voxel_mesh.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include <stdint.h>
#include "../Errno/errno.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../Geometry/geometry_3d.hpp"
#include "../Math/vector3.hpp"

static ft_bool chunk_mesh_intersect_chunk_box(const geometry_frustum &frustum,
    double minimum_x, double minimum_y, double minimum_z,
    double maximum_x, double maximum_y, double maximum_z) noexcept
{
    int32_t plane_index;
    double normal_x;
    double normal_y;
    double normal_z;
    double vertex_x;
    double vertex_y;
    double vertex_z;
    double signed_distance;

    plane_index = 0;
    while (plane_index < 6)
    {
        normal_x = frustum.planes[plane_index].normal.get_x();
        normal_y = frustum.planes[plane_index].normal.get_y();
        normal_z = frustum.planes[plane_index].normal.get_z();
        vertex_x = minimum_x;
        vertex_y = minimum_y;
        vertex_z = minimum_z;
        if (normal_x >= 0.0)
            vertex_x = maximum_x;
        if (normal_y >= 0.0)
            vertex_y = maximum_y;
        if (normal_z >= 0.0)
            vertex_z = maximum_z;
        signed_distance = (vertex_x * normal_x) + (vertex_y * normal_y)
            + (vertex_z * normal_z)
            + frustum.planes[plane_index].distance;
        if (signed_distance < 0.0)
            return (FT_FALSE);
        plane_index += 1;
    }
    return (FT_TRUE);
}

ft_bool chunk_mesh_intersects_frustum(const geometry_frustum &frustum,
    int32_t world_origin_x, int32_t world_origin_y,
    int32_t world_origin_z) noexcept
{
    double minimum_x;
    double minimum_y;
    double minimum_z;
    double maximum_x;
    double maximum_y;
    double maximum_z;

    minimum_x = static_cast<double>(world_origin_x);
    minimum_y = static_cast<double>(world_origin_y);
    minimum_z = static_cast<double>(world_origin_z);
    maximum_x = static_cast<double>(world_origin_x + GAME_VOXEL_CHUNK_WIDTH);
    maximum_y = static_cast<double>(world_origin_y + GAME_VOXEL_CHUNK_HEIGHT);
    maximum_z = static_cast<double>(world_origin_z + GAME_VOXEL_CHUNK_DEPTH);
    return (chunk_mesh_intersect_chunk_box(frustum, minimum_x, minimum_y,
        minimum_z, maximum_x, maximum_y, maximum_z));
}

ft_bool chunk_mesh_intersects_frustum(const geometry_frustum &frustum,
    const chunk_mesh &mesh, int32_t world_origin_x, int32_t world_origin_y,
    int32_t world_origin_z) noexcept
{
    geometry_aabb3 box;
    int32_t error_code;

    if (mesh.has_occupied_bounds == FT_FALSE)
        return (FT_FALSE);
    error_code = box.minimum.initialize(static_cast<double>(world_origin_x
            + mesh.occupied_bounds.minimum_x),
        static_cast<double>(world_origin_y + mesh.occupied_bounds.minimum_y),
        static_cast<double>(world_origin_z + mesh.occupied_bounds.minimum_z));
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    error_code = box.maximum.initialize(static_cast<double>(world_origin_x
            + mesh.occupied_bounds.maximum_x),
        static_cast<double>(world_origin_y + mesh.occupied_bounds.maximum_y),
        static_cast<double>(world_origin_z + mesh.occupied_bounds.maximum_z));
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (geometry_frustum_intersect_aabb3(frustum, box));
}

#endif

#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/voxel_mesh.hpp"
#include "../../Modules/Geometry/geometry_3d.hpp"
#include "../../Modules/Voxel/voxel_api.hpp"

static int32_t initialize_unit_cube_frustum_or_fail(geometry_frustum &frustum)
{
    if (frustum.planes[0].normal.initialize(1.0, 0.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[0].distance = 1.0;
    if (frustum.planes[1].normal.initialize(-1.0, 0.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[1].distance = 1.0;
    if (frustum.planes[2].normal.initialize(0.0, 1.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[2].distance = 1.0;
    if (frustum.planes[3].normal.initialize(0.0, -1.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[3].distance = 1.0;
    if (frustum.planes[4].normal.initialize(0.0, 0.0, 1.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[4].distance = 1.0;
    if (frustum.planes[5].normal.initialize(0.0, 0.0, -1.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[5].distance = 1.0;
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_chunk_mesh_generate_single_block_visible_faces)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(VOXEL_GENERATOR_STONE_BLOCK, mesh.vertices[0].block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_intersects_frustum_detects_visible_chunk)
{
    geometry_frustum frustum;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_unit_cube_frustum_or_fail(frustum));
    FT_ASSERT_EQ(FT_TRUE, chunk_mesh_intersects_frustum(frustum, 0, 0, 0));
    return (1);
}

FT_TEST(test_chunk_mesh_intersects_frustum_rejects_far_chunk)
{
    geometry_frustum frustum;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_unit_cube_frustum_or_fail(frustum));
    FT_ASSERT_EQ(FT_FALSE, chunk_mesh_intersects_frustum(frustum, 32, 32, 32));
    return (1);
}

FT_TEST(test_chunk_mesh_generate_hides_shared_faces)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(2, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_empty_chunk_has_no_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(0, mesh.vertices.size());
    FT_ASSERT_EQ(0, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_boundary_block_visible_faces)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_intersects_frustum_uses_occupied_bounds)
{
    geometry_frustum frustum;
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_unit_cube_frustum_or_fail(frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(15, 0, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_TRUE, mesh.has_occupied_bounds);
    FT_ASSERT_EQ(15, mesh.occupied_bounds.minimum_x);
    FT_ASSERT_EQ(0, mesh.occupied_bounds.minimum_y);
    FT_ASSERT_EQ(0, mesh.occupied_bounds.minimum_z);
    FT_ASSERT_EQ(16, mesh.occupied_bounds.maximum_x);
    FT_ASSERT_EQ(1, mesh.occupied_bounds.maximum_y);
    FT_ASSERT_EQ(1, mesh.occupied_bounds.maximum_z);
    FT_ASSERT_EQ(FT_TRUE, chunk_mesh_intersects_frustum(frustum, 0, 0, 0));
    FT_ASSERT_EQ(FT_FALSE, chunk_mesh_intersects_frustum(frustum, mesh, 0, 0,
        0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_hides_vertical_shared_faces)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 2, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_merges_same_block_floor_faces)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    int32_t local_x;
    int32_t local_z;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    local_z = 0;
    while (local_z < 4)
    {
        local_x = 0;
        while (local_x < 4)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(local_x, 0,
                local_z, VOXEL_GENERATOR_STONE_BLOCK));
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_keeps_block_type_boundaries)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 0, 0,
        VOXEL_GENERATOR_DIRT_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(40, mesh.vertices.size());
    FT_ASSERT_EQ(60, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_clear_removes_generated_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_clear(mesh));
    FT_ASSERT_EQ(0, mesh.vertices.size());
    FT_ASSERT_EQ(0, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_replaces_previous_mesh)
{
    game_voxel_chunk chunk;
    game_voxel_chunk empty_chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh,
        empty_chunk));
    FT_ASSERT_EQ(0, mesh.vertices.size());
    FT_ASSERT_EQ(0, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_sets_chunk_bounds)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(0, mesh.bounds.minimum_x);
    FT_ASSERT_EQ(0, mesh.bounds.minimum_y);
    FT_ASSERT_EQ(0, mesh.bounds.minimum_z);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_WIDTH, mesh.bounds.maximum_x);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_HEIGHT, mesh.bounds.maximum_y);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_DEPTH, mesh.bounds.maximum_z);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_voxel_chunk_has_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_generate_chunk(chunk,
        "terrain-test-seed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_NEQ(0, mesh.vertices.size());
    FT_ASSERT_NEQ(0, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

#endif

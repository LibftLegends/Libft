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
    const ft_size_t initial_water_capacity = mesh.water_indices.capacity();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(36, mesh.solid_indices.size());
    FT_ASSERT_EQ(0U, mesh.water_indices.size());
    FT_ASSERT_EQ(initial_water_capacity, mesh.water_indices.capacity());
    FT_ASSERT_EQ(FT_TRUE, mesh.has_occupied_bounds);
    FT_ASSERT_EQ(VOXEL_GENERATOR_STONE_BLOCK, mesh.vertices[0].block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_prepartitions_solid_and_water_indices)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    ft_size_t index = 0U;
    ft_size_t solid_index = 0U;
    ft_size_t water_index = 0U;
    uint32_t vertex_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(4, 1, 1,
        VOXEL_GENERATOR_WATER_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_NEQ(0, mesh.solid_indices.size());
    FT_ASSERT_NEQ(0, mesh.water_indices.size());
    FT_ASSERT_EQ(mesh.indices.size(), mesh.solid_indices.size()
        + mesh.water_indices.size());
    while (index < mesh.indices.size())
    {
        vertex_index = mesh.indices[index];
        FT_ASSERT(vertex_index < mesh.vertices.size());
        if (mesh.vertices[vertex_index].block_id == VOXEL_GENERATOR_WATER_BLOCK)
        {
            FT_ASSERT(water_index < mesh.water_indices.size());
            FT_ASSERT_EQ(vertex_index, mesh.water_indices[water_index]);
            water_index += 1U;
        }
        else
        {
            FT_ASSERT(solid_index < mesh.solid_indices.size());
            FT_ASSERT_EQ(vertex_index, mesh.solid_indices[solid_index]);
            solid_index += 1U;
        }
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_skips_air_sections_without_losing_high_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 15, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 48, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 200, 1,
        VOXEL_GENERATOR_LADDER_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(56U, mesh.vertices.size());
    FT_ASSERT_EQ(84U, mesh.indices.size());
    FT_ASSERT_EQ(200U, mesh.vertices[48].coordinate_y);
    FT_ASSERT_EQ(VOXEL_GENERATOR_LADDER_BLOCK, mesh.vertices[48].block_id);
    FT_ASSERT_EQ(201, mesh.occupied_bounds.maximum_y);
    const chunk_mesh_bounds air_bounds = {0, 17, 0, 16, 47, 16};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_with_neighbors_in_bounds(
        mesh, chunk, 0, 0, nullptr, nullptr, nullptr, air_bounds));
    FT_ASSERT_EQ(0U, mesh.vertices.size());
    FT_ASSERT_EQ(0U, mesh.indices.size());
    FT_ASSERT_EQ(FT_FALSE, mesh.has_occupied_bounds);
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
    FT_ASSERT_EQ(FT_FALSE, mesh.has_occupied_bounds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_height_matches_generated_chunk_height)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    voxel_generation_config config;
    uint32_t block_id;
    int32_t highest_solid_y;
    int32_t local_x;
    int32_t local_z;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_height_profile(0U, 40, 0,
        0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_decoration_policy(0U,
        FT_FALSE, FT_FALSE, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_generate_chunk(chunk, 0, 0,
        "mesh-height-invariant", config));
    highest_solid_y = -1;
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            int32_t local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;

            while (local_y >= 0)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(local_x,
                    local_y, local_z, &block_id));
                if (block_id != GAME_VOXEL_AIR_BLOCK)
                {
                    if (local_y > highest_solid_y)
                        highest_solid_y = local_y;
                    break;
                }
                local_y -= 1;
            }
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_NEQ(-1, highest_solid_y);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_TRUE, mesh.has_occupied_bounds);
    FT_ASSERT_EQ(highest_solid_y + 1, mesh.occupied_bounds.maximum_y);
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
    FT_ASSERT_EQ(FT_TRUE, mesh.has_occupied_bounds);
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
    FT_ASSERT_EQ(FT_TRUE, chunk_mesh_intersects_frustum(frustum, mesh, -14, 0,
        0));
    FT_ASSERT_EQ(FT_FALSE, chunk_mesh_intersects_frustum(frustum, mesh, -13, 0,
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

FT_TEST(test_chunk_mesh_bounds_validate_and_set_full)
{
    chunk_mesh_bounds bounds;

    chunk_mesh_bounds_set_full(bounds);
    FT_ASSERT_EQ(FT_TRUE, chunk_mesh_bounds_is_valid(bounds));
    FT_ASSERT_EQ(0, bounds.minimum_x);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_WIDTH, bounds.maximum_x);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_HEIGHT, bounds.maximum_y);
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_DEPTH, bounds.maximum_z);
    bounds.minimum_x = -1;
    FT_ASSERT_EQ(FT_FALSE, chunk_mesh_bounds_is_valid(bounds));
    chunk_mesh_bounds_set_full(bounds);
    bounds.maximum_y = bounds.minimum_y;
    FT_ASSERT_EQ(FT_FALSE, chunk_mesh_bounds_is_valid(bounds));
    return (1);
}

FT_TEST(test_chunk_mesh_generate_in_bounds_excludes_other_sections)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh_bounds bounds;
    ft_size_t vertex_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12,
        VOXEL_GENERATOR_DIRT_BLOCK));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 4;
    bounds.maximum_y = 16;
    bounds.maximum_z = 4;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        mesh, chunk, bounds));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    vertex_index = 0U;
    while (vertex_index < mesh.vertices.size())
    {
        FT_ASSERT(mesh.vertices[vertex_index].coordinate_x <= 4U);
        FT_ASSERT(mesh.vertices[vertex_index].coordinate_y <= 16U);
        FT_ASSERT(mesh.vertices[vertex_index].coordinate_z <= 4U);
        FT_ASSERT(mesh.vertices[vertex_index].block_id
            == VOXEL_GENERATOR_STONE_BLOCK);
        vertex_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_generate_in_bounds_is_stable_when_other_region_changes)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh_before;
    chunk_mesh mesh_after;
    chunk_mesh_bounds bounds;
    ft_size_t vertex_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh_before));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh_after));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 4;
    bounds.maximum_y = 16;
    bounds.maximum_z = 4;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        mesh_before, chunk, bounds));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12,
        VOXEL_GENERATOR_DIRT_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        mesh_after, chunk, bounds));
    FT_ASSERT_EQ(mesh_before.vertices.size(), mesh_after.vertices.size());
    FT_ASSERT_EQ(mesh_before.indices.size(), mesh_after.indices.size());
    vertex_index = 0U;
    while (vertex_index < mesh_before.vertices.size())
    {
        FT_ASSERT_EQ(mesh_before.vertices[vertex_index].coordinate_x,
            mesh_after.vertices[vertex_index].coordinate_x);
        FT_ASSERT_EQ(mesh_before.vertices[vertex_index].coordinate_y,
            mesh_after.vertices[vertex_index].coordinate_y);
        FT_ASSERT_EQ(mesh_before.vertices[vertex_index].coordinate_z,
            mesh_after.vertices[vertex_index].coordinate_z);
        FT_ASSERT_EQ(mesh_before.vertices[vertex_index].block_id,
            mesh_after.vertices[vertex_index].block_id);
        vertex_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh_after));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh_before));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_replace_in_bounds_preserves_unaffected_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    chunk_mesh_bounds bounds;
    ft_size_t vertex_index;
    ft_size_t stone_vertices;
    ft_size_t dirt_vertices;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_DIRT_BLOCK));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 3;
    bounds.maximum_y = 3;
    bounds.maximum_z = 3;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        replacement, chunk, bounds));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_replace_in_bounds(mesh,
        replacement, bounds));
    FT_ASSERT_EQ(48, mesh.vertices.size());
    FT_ASSERT_EQ(72, mesh.indices.size());
    FT_ASSERT_EQ(mesh.indices.size(), mesh.solid_indices.size());
    stone_vertices = 0U;
    dirt_vertices = 0U;
    vertex_index = 0U;
    while (vertex_index < mesh.vertices.size())
    {
        if (mesh.vertices[vertex_index].block_id
            == VOXEL_GENERATOR_STONE_BLOCK)
            stone_vertices += 1U;
        if (mesh.vertices[vertex_index].block_id == VOXEL_GENERATOR_DIRT_BLOCK)
            dirt_vertices += 1U;
        vertex_index += 1U;
    }
    FT_ASSERT_EQ(24, stone_vertices);
    FT_ASSERT_EQ(24, dirt_vertices);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_replace_in_bounds_rejects_unexpanded_greedy_geometry)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    chunk_mesh_bounds bounds;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 0, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 1;
    bounds.maximum_y = 1;
    bounds.maximum_z = 1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        replacement, chunk, bounds));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, chunk_mesh_replace_in_bounds(mesh,
        replacement, bounds));
    FT_ASSERT_EQ(24, mesh.vertices.size());
    FT_ASSERT_EQ(36, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_replace_in_bounds_rebuilds_water_partition)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    chunk_mesh_bounds bounds;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_WATER_BLOCK));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 3;
    bounds.maximum_y = 3;
    bounds.maximum_z = 3;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(
        replacement, chunk, bounds));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_replace_in_bounds(mesh,
        replacement, bounds));
    FT_ASSERT_EQ(42, mesh.indices.size());
    FT_ASSERT_EQ(36, mesh.solid_indices.size());
    FT_ASSERT_EQ(6, mesh.water_indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_replace_in_bounds_rejects_outside_replacement_transactionally)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    chunk_mesh_bounds bounds;
    ft_size_t vertex_index;
    ft_size_t stone_vertices;
    ft_size_t dirt_vertices;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12,
        VOXEL_GENERATOR_DIRT_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(replacement,
        chunk));
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = 3;
    bounds.maximum_y = 3;
    bounds.maximum_z = 3;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, chunk_mesh_replace_in_bounds(mesh,
        replacement, bounds));
    FT_ASSERT_EQ(48, mesh.vertices.size());
    FT_ASSERT_EQ(72, mesh.indices.size());
    stone_vertices = 0U;
    dirt_vertices = 0U;
    vertex_index = 0U;
    while (vertex_index < mesh.vertices.size())
    {
        if (mesh.vertices[vertex_index].block_id
            == VOXEL_GENERATOR_STONE_BLOCK)
            stone_vertices += 1U;
        if (mesh.vertices[vertex_index].block_id == VOXEL_GENERATOR_DIRT_BLOCK)
            dirt_vertices += 1U;
        vertex_index += 1U;
    }
    FT_ASSERT_EQ(24, stone_vertices);
    FT_ASSERT_EQ(24, dirt_vertices);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_chunk_mesh_replace_in_bounds_rejects_bad_late_partition_transactionally)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    const chunk_mesh_bounds bounds = {0, 0, 0, 3, 3, 3};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1, VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(12, 40, 12, VOXEL_GENERATOR_DIRT_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1, VOXEL_GENERATOR_WATER_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_in_bounds(replacement, chunk, bounds));
    FT_ASSERT_EQ(6U, replacement.water_indices.size());
    replacement.water_indices[5] = UINT32_MAX;
    const chunk_mesh_vertex *original_vertices = mesh.vertices.begin();
    const uint32_t *original_indices = mesh.indices.begin();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, chunk_mesh_replace_in_bounds(mesh, replacement, bounds));
    FT_ASSERT(mesh.vertices.begin() == original_vertices);
    FT_ASSERT(mesh.indices.begin() == original_indices);
    FT_ASSERT_EQ(48U, mesh.vertices.size());
    FT_ASSERT_EQ(72U, mesh.solid_indices.size());
    FT_ASSERT_EQ(0U, mesh.water_indices.size());
    FT_ASSERT_EQ(0U, replacement.water_indices[0]);
    FT_ASSERT_EQ(UINT32_MAX, replacement.water_indices[5]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(replacement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

#endif

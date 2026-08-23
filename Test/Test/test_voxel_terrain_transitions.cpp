#include "../test_internal.hpp"
#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

static int32_t voxel_transition_surface_height(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_z)
{
    int32_t local_y;
    uint32_t block_id;

    local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
    while (local_y >= 0)
    {
        if (chunk.read_block(local_x, local_y, local_z, &block_id)
            != FT_ERR_SUCCESS)
            return (-1);
        if (block_id != GAME_VOXEL_AIR_BLOCK)
            return (local_y);
        local_y -= 1;
    }
    return (-1);
}

static void voxel_transition_disable_decorations(
    terrain_generation_config &config)
{
    uint32_t biome_index;

    config.layers.enable_snow_caps = FT_FALSE;
    config.enable_erosion = FT_TRUE;
    config.underground_structures.enable_ravines = FT_FALSE;
    config.underground_structures.enable_cave_rooms = FT_FALSE;
    config.fluids.enable_rivers = FT_FALSE;
    config.fluids.enable_lakes = FT_FALSE;
    biome_index = 0U;
    while (biome_index < config.biome_count)
    {
        config.biomes[biome_index].allow_shrubs = FT_FALSE;
        config.biomes[biome_index].allow_trees = FT_FALSE;
        config.biomes[biome_index].allow_snow_caps = FT_FALSE;
        biome_index += 1U;
    }
    return ;
}

FT_TEST(test_terrain_transition_border_is_generation_order_independent)
{
    terrain_generation_config config;
    game_voxel_chunk left_chunk;
    game_voxel_chunk right_chunk;
    game_voxel_chunk reversed_left_chunk;
    game_voxel_chunk reversed_right_chunk;
    int32_t local_x;
    int32_t local_z;
    int32_t left_height;
    int32_t right_height;
    int32_t reversed_left_height;
    int32_t reversed_right_height;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    voxel_transition_disable_decorations(config);
    config.enable_mountain_ridges = FT_TRUE;
    config.mountain_ridge_strength = 18U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed_left_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed_right_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(left_chunk, 0, 0,
        "transition-order-seed", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(right_chunk, 16, 0,
        "transition-order-seed", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(reversed_right_chunk,
        16, 0, "transition-order-seed", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(reversed_left_chunk,
        0, 0, "transition-order-seed", config));
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            left_height = voxel_transition_surface_height(left_chunk,
                local_x, local_z);
            reversed_left_height = voxel_transition_surface_height(
                reversed_left_chunk, local_x, local_z);
            right_height = voxel_transition_surface_height(right_chunk,
                local_x, local_z);
            reversed_right_height = voxel_transition_surface_height(
                reversed_right_chunk, local_x, local_z);
            FT_ASSERT_NEQ(-1, left_height);
            FT_ASSERT_NEQ(-1, reversed_left_height);
            FT_ASSERT_NEQ(-1, right_height);
            FT_ASSERT_NEQ(-1, reversed_right_height);
            FT_ASSERT_EQ(left_height, reversed_left_height);
            FT_ASSERT_EQ(right_height, reversed_right_height);
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed_right_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed_left_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.destroy());
    return (1);
}

FT_TEST(test_terrain_mountain_height_is_deterministic_with_warped_ranges)
{
    terrain_generation_config config;
    game_voxel_chunk first_chunk;
    game_voxel_chunk second_chunk;
    int32_t local_x;
    int32_t local_z;
    int32_t first_height;
    int32_t second_height;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    voxel_transition_disable_decorations(config);
    config.enable_mountain_ridges = FT_TRUE;
    config.mountain_ridge_scale = 22;
    config.mountain_ridge_strength = 24U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(first_chunk, 32, 0,
        "mountain-range-seed", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(second_chunk, 32, 0,
        "mountain-range-seed", config));
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            first_height = voxel_transition_surface_height(first_chunk,
                local_x, local_z);
            second_height = voxel_transition_surface_height(second_chunk,
                local_x, local_z);
            FT_ASSERT_EQ(first_height, second_height);
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.destroy());
    return (1);
}

#endif

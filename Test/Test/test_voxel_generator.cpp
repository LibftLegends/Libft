#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/Game/game_voxel_region.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <stdio.h>

static int32_t test_terrain_surface_height(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_z) noexcept
{
    int32_t local_y;
    uint32_t block_id;
    int32_t error_code;

    local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
    while (local_y >= 0)
    {
        error_code = chunk.read_block(local_x, local_y, local_z, &block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (-1);
        if (block_id != GAME_VOXEL_AIR_BLOCK)
            return (local_y);
        local_y -= 1;
    }
    return (-1);
}

static ft_bool test_terrain_height_is_within_builtin_envelope(
    int32_t surface_height)
{
    if (surface_height < 66 || surface_height > 121)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool test_terrain_is_builtin_surface_block(uint32_t block_id)
{
    uint32_t biome_index;

    if (block_id == TERRAIN_GENERATOR_SNOW_BLOCK)
        return (FT_TRUE);
    biome_index = 0U;
    while (biome_index <= TERRAIN_BIOME_MOUNTAINS)
    {
        if (block_id == terrain_surface_block_for_biome(
                static_cast<terrain_biome>(biome_index)))
            return (FT_TRUE);
        biome_index += 1U;
    }
    return (FT_FALSE);
}

FT_TEST(test_terrain_generate_chunk_generates_default_surface_chunk)
{
    game_voxel_chunk chunk;
    int32_t surface_height;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk,
        "terrain-test-seed"));
    surface_height = test_terrain_surface_height(chunk, 0, 0);
    FT_ASSERT_NEQ(-1, surface_height);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, surface_height, 0,
        &block_id));
    FT_ASSERT_EQ(FT_TRUE, test_terrain_is_builtin_surface_block(block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, surface_height + 1, 0,
        &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_TRUE,
        test_terrain_height_is_within_builtin_envelope(surface_height));
    FT_ASSERT_EQ(FT_FALSE, chunk.is_dirty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_get_biome_changes_across_zone_boundaries)
{
    terrain_biome left_biome;
    terrain_biome right_biome;

    left_biome = terrain_get_biome(0, 0, "terrain-test-seed");
    right_biome = terrain_get_biome(TERRAIN_BIOME_ZONE_WIDTH, 0,
        "terrain-test-seed");
    FT_ASSERT_NEQ(left_biome, right_biome);
    return (1);
}

FT_TEST(test_terrain_surface_helpers_match_biome_rules)
{
    FT_ASSERT_EQ(TERRAIN_GENERATOR_GRASS_BLOCK,
        terrain_surface_block_for_biome(TERRAIN_BIOME_PLAINS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_GRASS_BLOCK,
        terrain_surface_block_for_biome(TERRAIN_BIOME_HILLS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_SAND_BLOCK,
        terrain_surface_block_for_biome(TERRAIN_BIOME_DESERT));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_SNOW_BLOCK,
        terrain_surface_block_for_biome(TERRAIN_BIOME_SNOW));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_SLATE_BLOCK,
        terrain_surface_block_for_biome(TERRAIN_BIOME_MOUNTAINS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_DIRT_BLOCK,
        terrain_subsurface_block_for_biome(TERRAIN_BIOME_PLAINS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_DIRT_BLOCK,
        terrain_subsurface_block_for_biome(TERRAIN_BIOME_HILLS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CANYON_ROCK_BLOCK,
        terrain_subsurface_block_for_biome(TERRAIN_BIOME_DESERT));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PERMAFROST_BLOCK,
        terrain_subsurface_block_for_biome(TERRAIN_BIOME_SNOW));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_ANDESITE_BLOCK,
        terrain_subsurface_block_for_biome(TERRAIN_BIOME_MOUNTAINS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_STONE_BLOCK,
        terrain_deep_block_for_biome(TERRAIN_BIOME_PLAINS));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_BASALT_BLOCK,
        terrain_deep_block_for_biome(TERRAIN_BIOME_MOUNTAINS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_shrubs(TERRAIN_BIOME_PLAINS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_shrubs(TERRAIN_BIOME_HILLS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_shrubs(TERRAIN_BIOME_DESERT));
    FT_ASSERT_EQ(FT_FALSE, terrain_biome_has_shrubs(TERRAIN_BIOME_SNOW));
    FT_ASSERT_EQ(FT_FALSE, terrain_biome_has_shrubs(TERRAIN_BIOME_MOUNTAINS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_trees(TERRAIN_BIOME_PLAINS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_trees(TERRAIN_BIOME_HILLS));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_trees(TERRAIN_BIOME_DESERT));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_trees(TERRAIN_BIOME_SNOW));
    FT_ASSERT_EQ(FT_TRUE, terrain_biome_has_trees(TERRAIN_BIOME_MOUNTAINS));
    return (1);
}

FT_TEST(test_terrain_tree_templates_expose_expected_blocks)
{
    const terrain_tree_template &oak_tree_template =
        terrain_small_oak_tree_template();
    const terrain_tree_template &oak_tree_template_variant =
        terrain_small_oak_tree_template_variant(1U);
    const terrain_tree_template &pine_tree_template =
        terrain_small_pine_tree_template();
    const terrain_tree_template &pine_tree_template_variant =
        terrain_small_pine_tree_template_variant(2U);
    const terrain_tree_template &cactus_tree_template =
        terrain_small_cactus_tree_template();
    const terrain_tree_template &cactus_tree_template_variant =
        terrain_small_cactus_tree_template_variant(1U);
    const terrain_tree_template &large_oak_tree_template =
        terrain_large_oak_tree_template();
    const terrain_tree_template &large_oak_tree_template_variant =
        terrain_large_oak_tree_template_variant(1U);
    const terrain_tree_template &large_pine_tree_template =
        terrain_large_pine_tree_template();
    const terrain_tree_template &large_pine_tree_template_variant =
        terrain_large_pine_tree_template_variant(1U);

    FT_ASSERT_NEQ(0U, oak_tree_template.block_count);
    FT_ASSERT_NEQ(oak_tree_template.block_count,
        oak_tree_template_variant.block_count);
    FT_ASSERT_NEQ(0U, pine_tree_template.block_count);
    FT_ASSERT_NEQ(pine_tree_template.block_count,
        pine_tree_template_variant.block_count);
    FT_ASSERT_NEQ(0U, cactus_tree_template.block_count);
    FT_ASSERT_NEQ(cactus_tree_template.block_count,
        cactus_tree_template_variant.block_count);
    FT_ASSERT_NEQ(oak_tree_template.block_count,
        large_oak_tree_template.block_count);
    FT_ASSERT_NEQ(pine_tree_template.block_count,
        large_pine_tree_template.block_count);
    FT_ASSERT_NEQ(large_oak_tree_template.block_count,
        large_oak_tree_template_variant.block_count);
    FT_ASSERT_NEQ(large_pine_tree_template.block_count,
        large_pine_tree_template_variant.block_count);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK,
        oak_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK,
        pine_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK,
        cactus_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK,
        oak_tree_template_variant.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK,
        pine_tree_template_variant.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK,
        cactus_tree_template_variant.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK,
        large_oak_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK,
        large_pine_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK,
        large_oak_tree_template_variant.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK,
        large_pine_tree_template_variant.blocks[0].block_id);
    return (1);
}

FT_TEST(test_terrain_place_tree_template_writes_small_oak_tree)
{
    game_voxel_chunk chunk;
    const terrain_tree_template &oak_tree_template =
        terrain_small_oak_tree_template();
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_place_tree_template(chunk, 8, 12, 8,
        oak_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 12, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 16, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LEAVES_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_place_tree_template_writes_small_pine_tree)
{
    game_voxel_chunk chunk;
    const terrain_tree_template &pine_tree_template =
        terrain_small_pine_tree_template();
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_place_tree_template(chunk, 8, 12, 8,
        pine_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 12, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 18, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LEAVES_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_place_tree_template_writes_small_cactus)
{
    game_voxel_chunk chunk;
    const terrain_tree_template &cactus_tree_template =
        terrain_small_cactus_tree_template();
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_place_tree_template(chunk, 8, 12, 8,
        cactus_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 12, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 15, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_can_place_tree_template_rejects_occupied_space)
{
    game_voxel_chunk chunk;
    const terrain_tree_template &oak_tree_template =
        terrain_small_oak_tree_template();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(8, 12, 8,
        TERRAIN_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_FALSE, terrain_can_place_tree_template(chunk, 8, 12, 8,
        oak_tree_template));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, terrain_place_tree_template(chunk,
        8, 12, 8, oak_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_tree_template_for_biome_uses_seed_selected_large_variants)
{
    const terrain_tree_template &oak_tree_template =
        terrain_tree_template_for_biome(TERRAIN_BIOME_HILLS, 3U);
    const terrain_tree_template &pine_tree_template =
        terrain_tree_template_for_biome(TERRAIN_BIOME_SNOW, 4U);

    FT_ASSERT_EQ(terrain_large_oak_tree_template().block_count,
        oak_tree_template.block_count);
    FT_ASSERT_EQ(terrain_large_pine_tree_template(1U).block_count,
        pine_tree_template.block_count);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LOG_BLOCK,
        oak_tree_template.blocks[0].block_id);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_PINE_LOG_BLOCK,
        pine_tree_template.blocks[0].block_id);
    return (1);
}

FT_TEST(test_terrain_generate_chunk_uses_biome_profile)
{
    game_voxel_chunk left_chunk;
    game_voxel_chunk right_chunk;
    int32_t left_surface_height;
    int32_t right_surface_height;
    uint32_t block_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(left_chunk, 0, 0,
        "terrain-test-seed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(right_chunk,
        TERRAIN_BIOME_ZONE_WIDTH, 0, "terrain-test-seed"));
    left_surface_height = test_terrain_surface_height(left_chunk, 0, 0);
    right_surface_height = test_terrain_surface_height(right_chunk, 0, 0);
    FT_ASSERT_NEQ(-1, left_surface_height);
    FT_ASSERT_NEQ(-1, right_surface_height);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.read_block(0, left_surface_height,
        0, &block_id));
    FT_ASSERT_EQ(FT_TRUE, test_terrain_is_builtin_surface_block(block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.read_block(0,
        right_surface_height, 0, &block_id));
    FT_ASSERT_EQ(FT_TRUE, test_terrain_is_builtin_surface_block(block_id));
    FT_ASSERT_EQ(FT_TRUE,
        test_terrain_height_is_within_builtin_envelope(left_surface_height));
    FT_ASSERT_EQ(FT_TRUE,
        test_terrain_height_is_within_builtin_envelope(right_surface_height));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generate_chunk_height_varies_with_world_position)
{
    game_voxel_chunk left_chunk;
    game_voxel_chunk right_chunk;
    int32_t left_surface_height;
    int32_t right_surface_height;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(left_chunk, 0, 0,
        "terrain-test-seed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(right_chunk, 32, 0,
        "terrain-test-seed"));
    left_surface_height = test_terrain_surface_height(left_chunk, 0, 0);
    right_surface_height = test_terrain_surface_height(right_chunk, 0, 0);
    FT_ASSERT_NEQ(-1, left_surface_height);
    FT_ASSERT_NEQ(-1, right_surface_height);
    FT_ASSERT_NEQ(left_surface_height, right_surface_height);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generate_chunk_is_deterministic_for_same_seed_and_origin)
{
    game_voxel_chunk first_chunk;
    game_voxel_chunk second_chunk;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t first_block_id;
    uint32_t second_block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(first_chunk, 128, 256,
        "terrain-test-seed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(second_chunk, 128, 256,
        "terrain-test-seed"));
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_y = 0;
        while (local_y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            local_x = 0;
            while (local_x < GAME_VOXEL_CHUNK_WIDTH)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.read_block(local_x,
                    local_y, local_z, &first_block_id));
                FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.read_block(local_x,
                    local_y, local_z, &second_block_id));
                FT_ASSERT_EQ(first_block_id, second_block_id);
                local_x += 1;
            }
            local_y += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generate_chunk_accepts_random_seed_placeholder)
{
    game_voxel_chunk chunk;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_metadata_identifies_cached_chunk)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint64_t seed_value;
    uint32_t configuration_signature;
    const game_voxel_generation_metadata *metadata;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_FALSE, chunk.has_generation_metadata());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 32, 64,
        "metadata-seed", config));
    metadata = &chunk.get_generation_metadata();
    seed_value = metadata->seed_value;
    configuration_signature = terrain_generation_config_signature(config);
    FT_ASSERT_EQ(FT_TRUE, chunk.has_generation_metadata());
    FT_ASSERT_EQ(seed_value, metadata->seed_value);
    FT_ASSERT_EQ(32, metadata->world_block_origin_x);
    FT_ASSERT_EQ(64, metadata->world_block_origin_z);
    FT_ASSERT_EQ(configuration_signature, metadata->configuration_signature);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_VERSION, metadata->generator_version);
    FT_ASSERT_EQ(TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES
        | TERRAIN_STAGE_FLUIDS | TERRAIN_STAGE_DECORATION
        | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES,
        metadata->completed_stage_mask);
    FT_ASSERT_EQ(FT_TRUE, chunk.generation_metadata_matches(seed_value, 32,
        64, configuration_signature));
    FT_ASSERT_EQ(FT_FALSE, chunk.generation_metadata_matches(seed_value, 33,
        64, configuration_signature));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0,
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_FALSE, chunk.has_generation_metadata());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_big_world_coordinate_keeps_generation_local)
{
    terrain_world_chunk_coordinate coordinate;
    terrain_generation_config config;
    game_voxel_chunk first_chunk;
    game_voxel_chunk second_chunk;
    uint32_t first_block_id;
    uint32_t second_block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, coordinate.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coordinate.set_chunk_coordinates(
        "92233720368547758081234567890", "-184467440737095516161234567890"));
    FT_ASSERT_NEQ(0U, coordinate.hash());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk_at_world_coordinate(
        first_chunk, coordinate, "big-coordinate-test", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk_at_world_coordinate(
        second_chunk, coordinate, "big-coordinate-test", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.read_block(7, 80, 9,
        &first_block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.read_block(7, 80, 9,
        &second_block_id));
    FT_ASSERT_EQ(first_block_id, second_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, coordinate.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_metadata_survives_chunk_serialization)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk restored_chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    ft_byte_buffer buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(source_chunk, 48, 80,
        "metadata-serialization", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.deserialize(buffer));
    FT_ASSERT_EQ(FT_TRUE, restored_chunk.has_generation_metadata());
    FT_ASSERT_EQ(source_chunk.get_generation_metadata().seed_value,
        restored_chunk.get_generation_metadata().seed_value);
    FT_ASSERT_EQ(source_chunk.get_generation_metadata().configuration_signature,
        restored_chunk.get_generation_metadata().configuration_signature);
    FT_ASSERT_EQ(source_chunk.get_generation_metadata().completed_stage_mask,
        restored_chunk.get_generation_metadata().completed_stage_mask);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_preserves_player_protected_chunk)
{
    terrain_generation_config config;
    game_voxel_chunk chunk;
    game_voxel_chunk restored_chunk;
    ft_byte_buffer buffer;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "protected-chunk", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0,
        TERRAIN_GENERATOR_BEDROCK_BLOCK));
    FT_ASSERT_EQ(FT_TRUE, chunk.is_generation_protected());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.deserialize(buffer));
    FT_ASSERT_EQ(FT_TRUE, restored_chunk.is_generation_protected());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(restored_chunk, 1000,
        1000,
        "different-world", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.read_block(0, 0, 0,
        &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_BEDROCK_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_templates_are_owned_and_signature_is_content_based)
{
    terrain_generation_config first_config;
    terrain_generation_config *loaded_config;
    ft_byte_buffer buffer;
    uint32_t first_signature;
    terrain_tree_template_block first_blocks[2];
    terrain_tree_template first_template;

    first_blocks[0] = {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK};
    first_blocks[1] = {0, 1, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK};
    first_template.blocks = first_blocks;
    first_template.block_count = 2U;
    loaded_config = new terrain_generation_config();
    FT_ASSERT_NEQ(ft_nullptr, loaded_config);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(
        first_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_config
        .set_biome_tree_template_override(0U, &first_template));
    first_signature = terrain_generation_config_signature(first_config);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_serialize(
        first_config, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_deserialize(
        *loaded_config, buffer));
    FT_ASSERT_EQ(first_config.tree_template_count,
        loaded_config->tree_template_count);
    FT_ASSERT_EQ(first_config.biomes[0].tree_template->block_count,
        loaded_config->biomes[0].tree_template->block_count);
    FT_ASSERT_EQ(first_config.biomes[0].tree_template->blocks[0].block_id,
        loaded_config->biomes[0].tree_template->blocks[0].block_id);
    FT_ASSERT_EQ(first_signature,
        terrain_generation_config_signature(*loaded_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    delete loaded_config;
    return (1);
}

FT_TEST(test_terrain_generation_routes_edge_features_into_neighbor_chunks)
{
    game_voxel_region region;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    const terrain_tree_template &tree_template =
        terrain_small_oak_tree_template();
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 40, 0, 0);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_TRUE, 6U, 100U);
    config.set_biome_tree_template_override(0U, &tree_template);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, "."));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk_in_region(region,
        GAME_VOXEL_CHUNK_WIDTH, GAME_VOXEL_CHUNK_DEPTH, "edge-feature",
        config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(
        GAME_VOXEL_CHUNK_WIDTH - 1, 43,
        GAME_VOXEL_CHUNK_DEPTH - 1, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_OAK_LEAVES_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    return (1);
}

static uint32_t test_custom_biome_selector(uint64_t, int32_t, int32_t,
    uint32_t biome_count, void *) noexcept
{
    return (biome_count - 1U);
}

static uint32_t test_edge_biome_selector(uint64_t, int32_t world_block_x,
    int32_t, uint32_t biome_count, void *) noexcept
{
    if (biome_count < 2U || world_block_x < TERRAIN_BIOME_ZONE_WIDTH)
        return (0U);
    return (1U);
}

FT_TEST(test_terrain_generation_blends_biome_edge_materials)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    uint32_t block_id;
    int32_t local_x;
    int32_t first_biome_surface_count;
    int32_t second_biome_surface_count;

    terrain_default_generation_config(config);
    config.set_biome_count(2U);
    config.set_biome_selector(&test_edge_biome_selector, ft_nullptr);
    config.set_noise_scales(1, 1, 0);
    config.set_sea_level(0);
    config.set_water_chance_percent(0U);
    config.set_mountain_ridges_enabled(FT_FALSE);
    config.set_erosion_enabled(FT_FALSE);
    config.layers.set_enabled(FT_FALSE, FT_FALSE);
    config.set_biome_height_profile(0U, 40, 0, 2);
    config.set_biome_height_profile(1U, 40, 0, 2);
    config.set_biome_block_palette(0U, TERRAIN_GENERATOR_GRASS_BLOCK,
        TERRAIN_GENERATOR_DIRT_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_block_palette(1U, TERRAIN_GENERATOR_SAND_BLOCK,
        TERRAIN_GENERATOR_CANYON_ROCK_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 0U, 0U);
    config.set_biome_decoration_policy(1U, FT_FALSE, FT_FALSE, 0U, 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_transition_settings(8, 35U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 112, 0,
        "biome-edge-materials", config));
    first_biome_surface_count = 0;
    second_biome_surface_count = 0;
    local_x = 0;
    while (local_x < GAME_VOXEL_CHUNK_WIDTH)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(local_x, 40, 0,
            &block_id));
        if (block_id == TERRAIN_GENERATOR_GRASS_BLOCK)
            first_biome_surface_count += 1;
        if (block_id == TERRAIN_GENERATOR_SAND_BLOCK)
            second_biome_surface_count += 1;
        local_x += 1;
    }
    FT_ASSERT_NEQ(0, first_biome_surface_count);
    FT_ASSERT_NEQ(0, second_biome_surface_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_controls_custom_flat_biome_and_water)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    config.set_noise_scales(1, 1, 0);
    config.set_sea_level(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 40, 0, 0);
    config.set_biome_block_palette(0U, TERRAIN_GENERATOR_SAND_BLOCK,
        TERRAIN_GENERATOR_SAND_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "custom-config", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 40, 0, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_SAND_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 41, 0, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_accepts_custom_feature_rule)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    const terrain_tree_template &cactus = terrain_small_cactus_tree_template();
    uint32_t block_id;
    int32_t cactus_count = 0;
    int32_t x;
    int32_t y;
    int32_t z;

    config.set_feature_count(1U);
    config.features[0].set_template(&cactus);
    config.features[0].set_biome_range(-1, 0, GAME_VOXEL_CHUNK_HEIGHT);
    config.features[0].set_chance(100U);
    config.features[0].set_requires_dry_land(FT_TRUE);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "feature-config", config));
    z = 0;
    while (z < GAME_VOXEL_CHUNK_DEPTH)
    {
        y = 0;
        while (y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            x = 0;
            while (x < GAME_VOXEL_CHUNK_WIDTH)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(x, y, z, &block_id));
                if (block_id == TERRAIN_GENERATOR_CACTUS_BLOCK)
                    cactus_count += 1;
                x += 1;
            }
            y += 1;
        }
        z += 1;
    }
    FT_ASSERT_NEQ(0, cactus_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_can_select_custom_biome_slot)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(6U));
    config.set_biome_selector(&test_custom_biome_selector, ft_nullptr);
    config.set_biome_height_profile(5U, 50, 0, 0);
    config.set_biome_block_palette(5U, TERRAIN_GENERATOR_CANYON_ROCK_BLOCK,
        TERRAIN_GENERATOR_CANYON_ROCK_BLOCK, TERRAIN_GENERATOR_SLATE_BLOCK);
    config.set_biome_decoration_policy(5U, FT_FALSE, FT_FALSE, 6U, 18U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    FT_ASSERT_EQ(5U, terrain_get_biome_index(config, 0, 0, "custom-biome"));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "custom-biome", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 50, 0, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CANYON_ROCK_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_snow_caps_follow_height_not_biome_identity)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint32_t block_id;
    int32_t local_y;
    ft_bool found_snow_cap;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 90, 0, 0);
    config.set_biome_block_palette(0U, TERRAIN_GENERATOR_STONE_BLOCK,
        TERRAIN_GENERATOR_STONE_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    config.layers.set_enabled(FT_TRUE, FT_TRUE);
    config.layers.set_snowline(84);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "high-plains-snow", config));
    found_snow_cap = FT_FALSE;
    local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
    while (local_y >= 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, local_y, 0,
            &block_id));
        if (block_id != GAME_VOXEL_AIR_BLOCK)
        {
            if (block_id == config.layers.snow_cap_block_id)
                found_snow_cap = FT_TRUE;
            break ;
        }
        local_y -= 1;
    }
    FT_ASSERT_EQ(FT_TRUE, found_snow_cap);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_snow_caps_are_biome_configurable)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint32_t block_id;
    int32_t local_y;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 90, 0, 0);
    config.set_biome_block_palette(0U, TERRAIN_GENERATOR_STONE_BLOCK,
        TERRAIN_GENERATOR_STONE_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    config.set_biome_snow_caps_enabled(0U, FT_FALSE);
    config.layers.set_enabled(FT_TRUE, FT_TRUE);
    config.layers.set_snowline(84);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "no-biome-snow", config));
    local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
    while (local_y >= 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, local_y, 0,
            &block_id));
        if (block_id != GAME_VOXEL_AIR_BLOCK)
            break ;
        local_y -= 1;
    }
    FT_ASSERT_NEQ(TERRAIN_GENERATOR_SNOW_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_accepts_custom_biome_without_tree_template)
{
    terrain_generation_config config;
    terrain_default_generation_config(config);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(6U));
    config.biomes[5].set_decoration_policy(FT_FALSE, FT_TRUE, 6U, 18U);
    config.set_biome_tree_template_override(5U, ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, terrain_generation_config_is_valid(config));
    return (1);
}

FT_TEST(test_terrain_generation_config_deserializes_standalone_override_owned)
{
    terrain_generation_config source_config;
    terrain_generation_config loaded_config;
    terrain_tree_template_block template_blocks[2];
    terrain_tree_template tree_template;
    ft_byte_buffer buffer;
    uint32_t index;

    terrain_default_generation_config(source_config);
    terrain_default_generation_config(loaded_config);
    template_blocks[0] = {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK};
    template_blocks[1] = {0, 1, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK};
    tree_template.blocks = template_blocks;
    tree_template.block_count = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_config
        .set_biome_tree_template_override(0U, &tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_serialize(
        source_config, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_deserialize(
        loaded_config, buffer));
    template_blocks[0].block_id = TERRAIN_GENERATOR_CACTUS_BLOCK;
    template_blocks[1].block_id = TERRAIN_GENERATOR_CACTUS_BLOCK;
    FT_ASSERT_EQ(&loaded_config.tree_templates[loaded_config.tree_template_count
        - 1U], loaded_config.biomes[0].tree_template);
    index = 0U;
    while (index < loaded_config.biomes[0].tree_template->block_count)
    {
        FT_ASSERT_EQ(source_config.biomes[0].tree_template->blocks[index]
            .block_id, loaded_config.biomes[0].tree_template->blocks[index]
            .block_id);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_template_removal_repairs_features)
{
    terrain_generation_config config;
    terrain_tree_template first_template;
    terrain_tree_template second_template;
    uint32_t first_index;
    uint32_t second_index;

    terrain_default_generation_config(config);
    first_template = terrain_small_oak_tree_template();
    second_template = terrain_small_cactus_tree_template();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_add_tree_template(
        config, first_template, &first_index));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_add_tree_template(
        config, second_template, &second_index));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.features[0].set_template(
        &config.tree_templates[second_index]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_remove_tree_template(
        config, first_index));
    FT_ASSERT_EQ(&config.tree_templates[first_index],
        config.features[0].template_data);
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK,
        config.features[0].template_data->blocks[0].block_id);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION,
        terrain_generation_config_remove_tree_template(config, first_index));
    terrain_feature_rule cleared_feature;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleared_feature.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature(0U, cleared_feature));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_clear_tree_templates(
        config));
    FT_ASSERT_EQ(ft_nullptr, config.features[0].template_data);
    return (1);
}

FT_TEST(test_terrain_generation_config_template_setters_reuse_owned_slots)
{
    terrain_generation_config config;
    terrain_tree_template tree_template;
    terrain_feature_rule feature;
    uint32_t initial_template_count;
    uint32_t index;

    terrain_default_generation_config(config);
    tree_template = terrain_small_cactus_tree_template();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
        0U, &tree_template));
    initial_template_count = config.tree_template_count;
    index = 0U;
    while (index < 65U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
            0U, ft_nullptr));
        FT_ASSERT_EQ(initial_template_count - 1U, config.tree_template_count);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
            0U, &tree_template));
        FT_ASSERT_EQ(initial_template_count, config.tree_template_count);
        index += 1U;
    }
    FT_ASSERT_EQ(initial_template_count, config.tree_template_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, feature.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, feature.set_template(&tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature(0U, feature));
    initial_template_count = config.tree_template_count;
    index = 0U;
    while (index < 65U)
    {
        terrain_feature_rule cleared_feature;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, cleared_feature.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature(0U,
            cleared_feature));
        FT_ASSERT_EQ(initial_template_count - 1U, config.tree_template_count);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature(0U, feature));
        FT_ASSERT_EQ(initial_template_count, config.tree_template_count);
        index += 1U;
    }
    FT_ASSERT_EQ(initial_template_count, config.tree_template_count);
    return (1);
}

FT_TEST(test_terrain_generation_config_clearing_shared_templates_detaches_only)
{
    terrain_generation_config config;
    terrain_tree_template tree_template;
    terrain_feature_rule cleared_feature;
    uint32_t initial_template_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.initialize());
    tree_template = terrain_small_cactus_tree_template();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
        0U, &tree_template));
    config.biomes[1].tree_template = config.biomes[0].tree_template;
    initial_template_count = config.tree_template_count;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
        0U, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, config.biomes[0].tree_template);
    FT_ASSERT_EQ(&config.tree_templates[initial_template_count - 1U],
        config.biomes[1].tree_template);
    FT_ASSERT_EQ(initial_template_count, config.tree_template_count);

    config.features[0].template_data = config.biomes[1].tree_template;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleared_feature.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_feature(0U, cleared_feature));
    FT_ASSERT_EQ(ft_nullptr, config.features[0].template_data);
    FT_ASSERT_EQ(&config.tree_templates[initial_template_count - 1U],
        config.biomes[1].tree_template);

    config.features[0].template_data = config.biomes[1].tree_template;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_tree_template_override(
        1U, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, config.biomes[1].tree_template);
    FT_ASSERT_EQ(&config.tree_templates[initial_template_count - 1U],
        config.features[0].template_data);
    return (1);
}

FT_TEST(test_terrain_generate_chunk_clears_previous_voxel_data)
{
    game_voxel_chunk regenerated_chunk;
    game_voxel_chunk fresh_chunk;
    terrain_generation_config high_config;
    terrain_generation_config low_config;
    terrain_default_generation_config(high_config);
    low_config.initialize(high_config);
    uint32_t regenerated_block_id;
    uint32_t fresh_block_id;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    high_config.set_biome_count(1U);
    high_config.set_sea_level(0);
    high_config.set_water_chance_percent(0U);
    high_config.set_biome_height_profile(0U, 120, 0, 0);
    high_config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    low_config.set_biome_height_profile(0U, 20, 0, 0);
    low_config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, regenerated_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(regenerated_chunk,
        0, 0, "regeneration-test", high_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(regenerated_chunk,
        0, 0, "regeneration-test", low_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(fresh_chunk,
        0, 0, "regeneration-test", low_config));
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_y = 0;
        while (local_y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            local_x = 0;
            while (local_x < GAME_VOXEL_CHUNK_WIDTH)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, regenerated_chunk.read_block(
                    local_x, local_y, local_z, &regenerated_block_id));
                FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh_chunk.read_block(local_x,
                    local_y, local_z, &fresh_block_id));
                FT_ASSERT_EQ(fresh_block_id, regenerated_block_id);
                local_x += 1;
            }
            local_y += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, regenerated_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_can_make_plains_uneven)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    int32_t x;
    int32_t z;
    int32_t surface_height;
    int32_t minimum_height = GAME_VOXEL_CHUNK_HEIGHT;
    int32_t maximum_height = 0;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 40, 12, 0);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "uneven-plains", config));
    z = 0;
    while (z < GAME_VOXEL_CHUNK_DEPTH)
    {
        x = 0;
        while (x < GAME_VOXEL_CHUNK_WIDTH)
        {
            surface_height = test_terrain_surface_height(chunk, x, z);
            FT_ASSERT_NEQ(-1, surface_height);
            if (surface_height < minimum_height)
                minimum_height = surface_height;
            if (surface_height > maximum_height)
                maximum_height = surface_height;
            x += 1;
        }
        z += 1;
    }
    FT_ASSERT_NEQ(minimum_height, maximum_height);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_rejects_invalid_ranges)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);

    config.biome_count = 0U;
    FT_ASSERT_EQ(FT_FALSE, terrain_generation_config_is_valid(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, terrain_generate_chunk(chunk, 0, 0,
        "invalid-config", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_lifecycle_and_mutator_methods)
{
    terrain_generation_config config;

    FT_ASSERT_EQ(FT_FALSE, config.is_initialised());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, config.set_sea_level(68));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_TRUE, config.is_initialised());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(68));
    FT_ASSERT_EQ(68, config.sea_level);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(55U));
    FT_ASSERT_EQ(55U, config.water_chance_percent);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.destroy());
    FT_ASSERT_EQ(FT_FALSE, config.is_initialised());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.destroy());
    return (1);
}

FT_TEST(test_terrain_nested_config_classes_have_lifecycle_contracts)
{
    terrain_biome_definition biome;
    terrain_feature_rule feature;
    terrain_ore_rule ore;
    terrain_underground_structure_config underground;
    terrain_fluid_config fluids;
    terrain_layer_config layers;
    terrain_biome_profile profile;

    profile.surface_height = 80;
    profile.height_variation = 4;
    profile.topsoil_depth = 3;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, biome.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, biome.set_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, biome.set_decoration_policy(
        FT_TRUE, FT_TRUE, 5U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, feature.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, feature.set_chance(50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore.set_range(4, 80));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore.set_vein(4U, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.set_chances(3U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.set_cave_shape(2U, 3U, 25U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.set_cave_entrances(10U, 1U));
    FT_ASSERT_EQ(2U, underground.cave_small_radius);
    FT_ASSERT_EQ(3U, underground.cave_large_radius);
    FT_ASSERT_EQ(25U, underground.cave_large_chance_percent);
    FT_ASSERT_EQ(10U, underground.cave_entrance_chance_percent);
    FT_ASSERT_EQ(1U, underground.cave_entrance_radius);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.set_cavern_rooms(FT_TRUE,
        4U, 5U));
    FT_ASSERT_EQ(FT_TRUE, underground.enable_cavern_rooms);
    FT_ASSERT_EQ(4U, underground.cavern_room_chance_percent);
    FT_ASSERT_EQ(5U, underground.cavern_room_radius);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.set_cavern_rooms(FT_FALSE,
        0U, 0U));
    FT_ASSERT_EQ(FT_FALSE, underground.enable_cavern_rooms);
    FT_ASSERT_EQ(0U, underground.cavern_room_chance_percent);
    FT_ASSERT_EQ(0U, underground.cavern_room_radius);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fluids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fluids.set_lake_settings(48, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, layers.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, layers.set_snowline(84));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, biome.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, feature.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ore.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, underground.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fluids.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, layers.destroy());
    return (1);
}

FT_TEST(test_terrain_caves_are_rounded_and_can_reach_surface)
{
    terrain_generation_config config;
    game_voxel_chunk chunk;
    uint32_t block_id;
    uint32_t interior_air_count;
    uint32_t entrance_air_count;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_height_profile(
        0U, 80, 0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_block_palette(0U,
        TERRAIN_GENERATOR_STONE_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK,
        TERRAIN_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_decoration_policy(0U,
        FT_FALSE, FT_FALSE, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.layers.set_enabled(FT_FALSE,
        FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.fluids.set_enabled(FT_FALSE,
        FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.underground_structures.set_enabled(
        FT_FALSE, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.underground_structures.set_chances(
        0U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.underground_structures.set_height_range(
        8, 70));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.underground_structures.set_cave_shape(
        2U, 3U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        config.underground_structures.set_cave_entrances(100U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        config.underground_structures.set_cavern_rooms(FT_TRUE, 100U, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(chunk, 0, 0,
        "rounded-caves", config));

    interior_air_count = 0U;
    entrance_air_count = 0U;
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            local_y = 8;
            while (local_y < 71)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(local_x,
                    local_y, local_z, &block_id));
                if (block_id == GAME_VOXEL_AIR_BLOCK)
                    interior_air_count += 1U;
                local_y += 1;
            }
            local_y = 73;
            while (local_y < 80)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(local_x,
                    local_y, local_z, &block_id));
                if (block_id == GAME_VOXEL_AIR_BLOCK)
                    entrance_air_count += 1U;
                local_y += 1;
            }
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_NEQ(0U, interior_air_count);
    FT_ASSERT_NEQ(0U, entrance_air_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_context_freezes_world_policy)
{
    game_voxel_chunk chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    terrain_generation_context context;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 90, 0, 0);
    config.set_biome_block_palette(0U, TERRAIN_GENERATOR_STONE_BLOCK,
        TERRAIN_GENERATOR_STONE_BLOCK, TERRAIN_GENERATOR_STONE_BLOCK);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    config.layers.enable_snow_caps = FT_TRUE;
    config.layers.snow_cap_minimum_height = 84;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_context_initialize(
        context, config));
    config.set_biome_height_profile(0U, 20, 0, 0);
    config.layers.set_enabled(FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_TRUE, terrain_generation_context_is_initialised(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk_with_context(chunk,
        0, 0, "frozen-policy", context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 90, 0, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_SNOW_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_file_round_trip)
{
    const char *file_path = "terrain_generation_config_test.bin";
    terrain_generation_config source_config;
    terrain_generation_config loaded_config;
    terrain_default_generation_config(source_config);

    source_config.set_sea_level(64);
    source_config.set_noise_scales(48, source_config.detail_noise_scale,
        source_config.detail_noise_percent);
    source_config.layers.set_snowline(78);
    source_config.underground_structures.ravine_width = 6U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_config.underground_structures.set_cave_shape(2U, 4U, 35U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_config.underground_structures.set_cave_entrances(12U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_config.underground_structures.set_cavern_rooms(FT_TRUE,
            7U, 5U));
    source_config.fluids.enable_lakes = FT_FALSE;
    source_config.ores[0].set_enabled(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_save_file(
        file_path, source_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generation_config_load_file(
        file_path, loaded_config));
    FT_ASSERT_EQ(source_config.sea_level, loaded_config.sea_level);
    FT_ASSERT_EQ(source_config.large_noise_scale,
        loaded_config.large_noise_scale);
    FT_ASSERT_EQ(source_config.layers.snow_cap_minimum_height,
        loaded_config.layers.snow_cap_minimum_height);
    FT_ASSERT_EQ(source_config.biomes[0].allow_snow_caps,
        loaded_config.biomes[0].allow_snow_caps);
    FT_ASSERT_EQ(source_config.biomes[0].allow_mountain_ridges,
        loaded_config.biomes[0].allow_mountain_ridges);
    FT_ASSERT_EQ(source_config.underground_structures.ravine_width,
        loaded_config.underground_structures.ravine_width);
    FT_ASSERT_EQ(source_config.underground_structures.cave_small_radius,
        loaded_config.underground_structures.cave_small_radius);
    FT_ASSERT_EQ(source_config.underground_structures.cave_large_radius,
        loaded_config.underground_structures.cave_large_radius);
    FT_ASSERT_EQ(source_config.underground_structures
        .cave_large_chance_percent,
        loaded_config.underground_structures.cave_large_chance_percent);
    FT_ASSERT_EQ(source_config.underground_structures
        .cave_entrance_chance_percent,
        loaded_config.underground_structures.cave_entrance_chance_percent);
    FT_ASSERT_EQ(source_config.underground_structures.cave_entrance_radius,
        loaded_config.underground_structures.cave_entrance_radius);
    FT_ASSERT_EQ(source_config.underground_structures.enable_cavern_rooms,
        loaded_config.underground_structures.enable_cavern_rooms);
    FT_ASSERT_EQ(source_config.underground_structures
        .cavern_room_chance_percent,
        loaded_config.underground_structures.cavern_room_chance_percent);
    FT_ASSERT_EQ(source_config.underground_structures.cavern_room_radius,
        loaded_config.underground_structures.cavern_room_radius);
    FT_ASSERT_EQ(source_config.fluids.enable_lakes,
        loaded_config.fluids.enable_lakes);
    FT_ASSERT_EQ(source_config.ores[0].enabled,
        loaded_config.ores[0].enabled);
    FT_ASSERT_EQ(source_config.biome_count, loaded_config.biome_count);
    FT_ASSERT_EQ(source_config.biomes[0].profile.surface_height,
        loaded_config.biomes[0].profile.surface_height);
    FT_ASSERT_EQ(0, std::remove(file_path));
    return (1);
}

FT_TEST(test_terrain_config_json_serialization_and_file_modes)
{
    const char *file_path = "terrain_config_test.jsonl";
    terrain_generation_config generation;
    ft_string output;
    ft_string file_contents;
    int32_t invalid_mode_value = 2;

    terrain_default_generation_config(generation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.biomes[0].serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_biome_definition"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.features[0].serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_feature_rule"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.ores[0].serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_ore_rule"));
    FT_ASSERT(ft_strstr(output.c_str(), "veins_per_chunk_min"));
    FT_ASSERT(ft_strstr(output.c_str(), "minimum_depth"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        generation.underground_structures.serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_underground_structures"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.fluids.serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_fluids"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.layers.serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "terrain_layers"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.serialize_json(output));
    FT_ASSERT(ft_strstr(output.c_str(), "biome_size_min"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_contents.initialize());
    (void)file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, generation.save_json_file(".",
        TERRAIN_JSON_FILE_CREATE_ONLY));
    FT_ASSERT(generation.save_json_file(
        "terrain_json_missing_parent_7f2e/config.json",
        TERRAIN_JSON_FILE_CREATE_ONLY) != FT_ERR_ALREADY_EXISTS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_CREATE_ONLY));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_CREATE_ONLY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, file_contents));
    FT_ASSERT(ft_strstr(file_contents.c_str(), "terrain_generation_config"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_REPLACE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, file_contents));
    FT_ASSERT_EQ(output.size(), file_contents.size());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, generation.save_json_file(file_path,
        static_cast<terrain_json_file_mode>(invalid_mode_value)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_terrain_json_unsigned_boundaries_and_transaction)
{
    terrain_generation_config generation;
    ft_string output;
    uint32_t values[6] = {0U, 1U, 2147483647U, 2147483648U,
        4294967294U, 4294967295U};
    const char *expected_values[6] = {"0", "1", "2147483647",
        "2147483648", "4294967294", "4294967295"};
    uint32_t index;
    int32_t error_code;

    terrain_default_generation_config(generation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("sentinel"));
    index = 0U;
    while (index < 6U)
    {
        generation.biomes[0].surface_block_id = values[index];
        error_code = generation.biomes[0].serialize_json(output);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT(ft_strstr(output.c_str(), expected_values[index])
            != ft_nullptr);
        FT_ASSERT(ft_strstr(output.c_str(), "\"surface_block_id\":-")
            == ft_nullptr);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.biomes[0].serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(static_cast<ft_size_t>(8U), output.size());
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.features[0].serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.ores[0].serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.underground_structures.serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.fluids.serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.layers.serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign("sentinel", 8U));
    cma_set_alloc_limit(1U);
    error_code = generation.serialize_json(output);
    cma_set_alloc_limit(0U);
    FT_ASSERT(error_code != FT_ERR_SUCCESS);
    FT_ASSERT(ft_strstr(output.c_str(), "sentinel") != ft_nullptr);
    return (1);
}

FT_TEST(test_terrain_json_file_failure_hooks_are_transactional)
{
    const char *file_path = "terrain_config_file_failure_hooks.jsonl";
    terrain_generation_config generation;
    ft_string file_contents;
    int32_t error_code;

    terrain_default_generation_config(generation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_contents.initialize());
    (void)file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_CREATE_ONLY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, file_contents));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_json_test_fail_file_operation(
        TERRAIN_JSON_TEST_FILE_OPEN_CREATE_ONLY, FT_ERR_PERMISSION_DENIED, 0U));
    error_code = generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_CREATE_ONLY);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, file_contents));
    FT_ASSERT(ft_strstr(file_contents.c_str(), "terrain_generation_config")
        != ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_json_test_fail_file_operation(
        TERRAIN_JSON_TEST_FILE_FIRST_WRITE, FT_ERR_DISK_FULL, 0U));
    FT_ASSERT_EQ(FT_ERR_DISK_FULL, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_REPLACE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_json_test_fail_file_operation(
        TERRAIN_JSON_TEST_FILE_PARTIAL_WRITE, FT_ERR_IO, 4U));
    FT_ASSERT_EQ(FT_ERR_IO, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_REPLACE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_json_test_fail_file_operation(
        TERRAIN_JSON_TEST_FILE_INTERRUPTED_WRITE, FT_ERR_INVALID_STATE, 0U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_REPLACE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_json_test_fail_file_operation(
        TERRAIN_JSON_TEST_FILE_CLOSE, FT_ERR_IO, 0U));
    FT_ASSERT_EQ(FT_ERR_IO, generation.save_json_file(file_path,
        TERRAIN_JSON_FILE_REPLACE));
    terrain_json_test_clear_file_failure();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_terrain_ore_rules_are_enabled_by_default_and_configurable)
{
    game_voxel_chunk disabled_chunk;
    game_voxel_chunk enabled_chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    uint32_t block_id;
    int32_t coal_count;
    int32_t coal_surface_count;
    int32_t x;
    int32_t y;
    int32_t z;

    FT_ASSERT_EQ(FT_TRUE, config.ores[0].enabled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_sea_level(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 100, 0, 0);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_FALSE, 6U, 18U);
    config.set_ore_rule_count(1U);
    config.ores[0].set_range(8, 90);
    config.ores[0].set_vein(config.ores[0].vein_size, 100U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        config.ores[0].set_depth_range(0, 10));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        config.ores[0].set_vein_size_range(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        config.ores[0].set_frequency_range(4U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.ores[0].set_depth_range(8, 90));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.ores[0].set_vein_size_range(3U, 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.ores[0].set_frequency_range(2U, 4U));
    FT_ASSERT_EQ(8, config.ores[0].minimum_depth);
    FT_ASSERT_EQ(90, config.ores[0].maximum_depth);
    FT_ASSERT_EQ(3U, config.ores[0].vein_size_min);
    FT_ASSERT_EQ(7U, config.ores[0].vein_size_max);
    FT_ASSERT_EQ(2U, config.ores[0].veins_per_chunk_min);
    FT_ASSERT_EQ(4U, config.ores[0].veins_per_chunk_max);
    config.ores[0].set_enabled(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, disabled_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, enabled_chunk.initialize());
    config.ores[0].set_enabled(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(disabled_chunk, 0, 0,
        "ore-config", config));
    config.ores[0].set_enabled(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(enabled_chunk, 0, 0,
        "ore-config", config));
    coal_count = 0;
    coal_surface_count = 0;
    z = 0;
    while (z < GAME_VOXEL_CHUNK_DEPTH)
    {
        y = 0;
        while (y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            x = 0;
            while (x < GAME_VOXEL_CHUNK_WIDTH)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, enabled_chunk.read_block(x, y, z,
                    &block_id));
                if (block_id == TERRAIN_GENERATOR_COAL_ORE_BLOCK)
                {
                    coal_count += 1;
                    if (y >= 94)
                        coal_surface_count += 1;
                }
                x += 1;
            }
            y += 1;
        }
        z += 1;
    }
    FT_ASSERT_NEQ(0, coal_count);
    FT_ASSERT_EQ(0, coal_surface_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, enabled_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, disabled_chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_generation_config_controls_tree_and_water_density)
{
    game_voxel_chunk dry_chunk;
    game_voxel_chunk wet_chunk;
    terrain_generation_config config;
    terrain_default_generation_config(config);
    const terrain_tree_template &oak = terrain_small_oak_tree_template();
    uint32_t block_id;
    int32_t tree_count = 0;
    int32_t dense_tree_count = 0;
    int32_t water_count = 0;
    int32_t x;
    int32_t y;
    int32_t z;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_biome_count(1U));
    config.set_sea_level(50);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, config.set_water_chance_percent(0U));
    config.set_biome_height_profile(0U, 40, 0, 0);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_TRUE, 6U, 0U);
    config.set_biome_tree_template_override(0U, &oak);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dry_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(dry_chunk, 0, 0,
        "density-config", config));
    config.set_water_chance_percent(100U);
    config.set_biome_decoration_policy(0U, FT_FALSE, FT_TRUE, 6U, 100U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wet_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_generate_chunk(wet_chunk, 0, 0,
        "density-config", config));
    z = 0;
    while (z < GAME_VOXEL_CHUNK_DEPTH)
    {
        y = 0;
        while (y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            x = 0;
            while (x < GAME_VOXEL_CHUNK_WIDTH)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, dry_chunk.read_block(x, y, z,
                    &block_id));
                if (block_id == TERRAIN_GENERATOR_OAK_LOG_BLOCK)
                    tree_count += 1;
                FT_ASSERT_EQ(FT_ERR_SUCCESS, wet_chunk.read_block(x, y, z,
                    &block_id));
                if (block_id == TERRAIN_GENERATOR_OAK_LOG_BLOCK)
                    dense_tree_count += 1;
                if (block_id == TERRAIN_GENERATOR_WATER_BLOCK)
                    water_count += 1;
                x += 1;
            }
            y += 1;
        }
        z += 1;
    }
    FT_ASSERT_EQ(0, tree_count);
    FT_ASSERT_NEQ(0, dense_tree_count);
    FT_ASSERT_NEQ(0, water_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wet_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dry_chunk.destroy());
    return (1);
}

#endif

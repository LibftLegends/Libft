#include <stdint.h>
#include "terrain_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../Game/game_voxel_region.hpp"

static const int32_t TERRAIN_HEIGHTMAP_LARGE_SCALE = 32;
static const int32_t TERRAIN_HEIGHTMAP_DETAIL_SCALE = 8;
static const int32_t TERRAIN_HEIGHTMAP_SMOOTH_RADIUS = 1;
static const int32_t TERRAIN_MOUNTAIN_CLIFF_SLOPE = 7;
static const int32_t TERRAIN_MOUNTAIN_SNOW_SLOPE_LIMIT = 5;
static const uint64_t TERRAIN_FEATURE_SHRUB_SALT = UINT64_C(0x2D9C1F4E8B3A6071);
static const int32_t TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET = 1;
static const uint64_t TERRAIN_FEATURE_GROUND_COVER_VARIANT_SALT =
    UINT64_C(0x1A2B3C4D5E6F7081);
static const uint64_t TERRAIN_FEATURE_TREE_SALT = UINT64_C(0x4F1E2D3C5B6A7980);
static const uint64_t TERRAIN_FEATURE_WATER_SALT = UINT64_C(0x9182736455463728);
static const uint64_t TERRAIN_FEATURE_AQUATIC_PLANT_SALT =
    UINT64_C(0x2F4E6D8C0A1B3547);
static const uint64_t TERRAIN_BIOME_SURFACE_TRANSITION_SALT = UINT64_C(0xBB67AE8584CAA73B);
static const uint64_t TERRAIN_BIOME_SUBSURFACE_TRANSITION_SALT = UINT64_C(0x3C6EF372FE94F82B);
static const uint64_t TERRAIN_CAVE_PRIMARY_SALT = UINT64_C(0x7C3A91E2D4B8560F);
static const uint64_t TERRAIN_CAVE_DETAIL_SALT = UINT64_C(0x1D6F80B3C9274A55);
static const int32_t TERRAIN_CAVE_PRIMARY_SCALE = 24;
static const int32_t TERRAIN_CAVE_DETAIL_SCALE = 9;
static const int32_t TERRAIN_BEDROCK_FLOOR_Y = 0;
static const int32_t TERRAIN_CAVE_SURFACE_MARGIN = 7;
static const int32_t TERRAIN_CAVE_CELL_SIZE = 12;
static const int32_t TERRAIN_COLUMN_CACHE_COUNT =
    GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH;
static const uint8_t TERRAIN_SURFACE_WATER_NONE = 0U;
static const uint8_t TERRAIN_SURFACE_WATER_RIVER = 1U;
static const uint8_t TERRAIN_SURFACE_WATER_LAKE = 2U;
static const uint8_t TERRAIN_SURFACE_WATER_LEGACY = 3U;

static ft_bool terrain_world_coordinate_on_grid(int32_t coordinate,
    int32_t spacing, int32_t offset) noexcept
{
    int32_t remainder;

    if (spacing <= 0)
        return (FT_FALSE);
    remainder = (coordinate - offset) % spacing;
    if (remainder < 0)
        remainder += spacing;
    return (remainder == 0 ? FT_TRUE : FT_FALSE);
}

static int32_t terrain_floor_division(int32_t value, int32_t divisor) noexcept
{
    int32_t quotient;

    if (divisor <= 0)
        return (0);
    quotient = value / divisor;
    if (value < 0 && value % divisor != 0)
        quotient -= 1;
    return (quotient);
}

struct terrain_biome_sample
{
    uint32_t biome_indices[4];
    double weights[4];
    uint32_t count;
};

struct terrain_column_cache
{
    uint32_t biome;
    terrain_biome_sample biome_sample;
    terrain_biome_profile biome_profile;
    int32_t column_height;
    int32_t slope_height;
    ft_bool has_surface_water;
    uint8_t surface_water_kind;
    uint64_t surface_water_feature_id;
    int32_t surface_water_level;
    uint32_t surface_water_depth;
    uint32_t surface_water_bank_distance;
    uint32_t deep_block_id;
    ft_bool can_place_shrubs;
    ft_bool can_place_trees;
    ft_bool can_place_snow;
    ft_bool can_place_mountain_ridges;
    uint32_t shrub_chance_percent;
    uint32_t tree_chance_percent;
};

static int32_t terrain_sample_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept;

static void terrain_sample_biomes(const terrain_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z,
    terrain_biome_sample *sample) noexcept;

static int32_t terrain_estimate_slope(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept;

static int32_t terrain_smooth_heightfield(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept;

static uint8_t terrain_surface_water_kind(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept;

static int32_t terrain_surface_water_bed_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z, int32_t column_height,
    const terrain_generation_config &config) noexcept;

static ft_bool terrain_stage_dependencies_are_met(uint32_t requested_mask,
    uint32_t previous_mask) noexcept;

static ft_bool terrain_underground_lake_geometry_is_valid(
    game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const terrain_generation_config &config) noexcept;

static int32_t terrain_read_generation_block(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const terrain_generation_config &config, uint32_t *block_id) noexcept
{
    if (local_x >= 0 && local_x < GAME_VOXEL_CHUNK_WIDTH
        && local_y >= 0 && local_y < GAME_VOXEL_CHUNK_HEIGHT
        && local_z >= 0 && local_z < GAME_VOXEL_CHUNK_DEPTH)
        return (chunk.read_block(local_x, local_y, local_z, block_id));
    if (config.cross_chunk_block_reader == ft_nullptr)
        return (FT_ERR_OUT_OF_RANGE);
    return (config.cross_chunk_block_reader(world_block_origin_x + local_x,
        local_y, world_block_origin_z + local_z, block_id,
        config.cross_chunk_block_reader_user_data));
}

static int32_t terrain_write_generation_block(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const terrain_generation_config &config, uint32_t block_id) noexcept
{
    if (local_x >= 0 && local_x < GAME_VOXEL_CHUNK_WIDTH
        && local_y >= 0 && local_y < GAME_VOXEL_CHUNK_HEIGHT
        && local_z >= 0 && local_z < GAME_VOXEL_CHUNK_DEPTH)
        return (chunk.write_generated_block(local_x, local_y, local_z,
            block_id));
    if (config.cross_chunk_block_writer == ft_nullptr)
        return (FT_ERR_OUT_OF_RANGE);
    return (config.cross_chunk_block_writer(world_block_origin_x + local_x,
        local_y, world_block_origin_z + local_z, block_id,
        config.cross_chunk_block_writer_user_data));
}

static ft_bool terrain_underground_lake_geometry_is_valid(
    game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const terrain_generation_config &config) noexcept
{
    int32_t offset;
    int32_t level;
    uint32_t block_id;

    offset = 0;
    while (offset < static_cast<int32_t>(
        config.fluids.underground_lake_floor_thickness))
    {
        int32_t z = -1;
        while (z <= 1)
        {
            int32_t x = -1;
            while (x <= 1)
            {
                if (terrain_read_generation_block(chunk, local_x + x,
                        local_y - 1 - offset, local_z + z,
                        world_block_origin_x, world_block_origin_z, config,
                        &block_id) != FT_ERR_SUCCESS
                    || terrain_block_is_solid(block_id) == FT_FALSE)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        offset += 1;
    }
    level = 0;
    while (level <= static_cast<int32_t>(config.fluids
            .underground_lake_depth))
    {
        int32_t z = -2;
        while (z <= 2)
        {
            int32_t x = -2;
            while (x <= 2)
            {
                if (std::abs(x) == 2 || std::abs(z) == 2)
                {
                    if (terrain_read_generation_block(chunk, local_x + x,
                            local_y + level, local_z + z,
                            world_block_origin_x, world_block_origin_z,
                            config, &block_id) != FT_ERR_SUCCESS
                        || terrain_block_is_solid(block_id) == FT_FALSE)
                        return (FT_FALSE);
                }
                x += 1;
            }
            z += 1;
        }
        level += 1;
    }
    level = 0;
    while (level <= static_cast<int32_t>(config.fluids
            .underground_lake_depth))
    {
        int32_t z = -1;
        while (z <= 1)
        {
            int32_t x = -1;
            while (x <= 1)
            {
                if (terrain_read_generation_block(chunk, local_x + x,
                        local_y + level, local_z + z, world_block_origin_x,
                        world_block_origin_z, config, &block_id)
                        != FT_ERR_SUCCESS
                    || block_id != TERRAIN_GENERATOR_AIR_BLOCK)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        level += 1;
    }
    offset = 1;
    while (offset <= static_cast<int32_t>(
        config.fluids.underground_lake_roof_thickness))
    {
        int32_t z = -1;
        while (z <= 1)
        {
            int32_t x = -1;
            while (x <= 1)
            {
                if (terrain_read_generation_block(chunk, local_x + x,
                        local_y + static_cast<int32_t>(config.fluids
                            .underground_lake_depth) + offset, local_z + z,
                        world_block_origin_x, world_block_origin_z, config,
                        &block_id) != FT_ERR_SUCCESS
                    || terrain_block_is_solid(block_id) == FT_FALSE)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        offset += 1;
    }
    return (FT_TRUE);
}

static int32_t terrain_stage_clear_chunk(game_voxel_chunk &chunk) noexcept
{
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    int32_t error_code;

    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            local_y = 0;
            while (local_y < GAME_VOXEL_CHUNK_HEIGHT)
            {
                error_code = chunk.write_generated_block(local_x, local_y, local_z,
                    GAME_VOXEL_AIR_BLOCK);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                local_y += 1;
            }
            local_x += 1;
        }
        local_z += 1;
    }
    return (FT_ERR_SUCCESS);
}

static void terrain_stage_prepare_columns(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const terrain_generation_config &config,
    terrain_column_cache *column_cache) noexcept
{
    int32_t local_x;
    int32_t local_z;
    int32_t world_block_x;
    int32_t world_block_z;
    int32_t column_index;
    uint32_t sample_index;
    double shrub_chance;
    double tree_chance;
    double mountain_weight;
    double snow_weight;

    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        world_block_z = world_block_origin_z + local_z;
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            world_block_x = world_block_origin_x + local_x;
            terrain_sample_biomes(config, seed_value, world_block_x,
                world_block_z, &column_cache[column_index].biome_sample);
            column_cache[column_index].biome = column_cache[column_index]
                .biome_sample.biome_indices[0];
            column_cache[column_index].biome_profile.surface_height = 0;
            column_cache[column_index].biome_profile.height_variation = 0;
            column_cache[column_index].biome_profile.topsoil_depth = 0;
            shrub_chance = 0.0;
            tree_chance = 0.0;
            mountain_weight = 0.0;
            snow_weight = 0.0;
            sample_index = 0U;
            while (sample_index < column_cache[column_index].biome_sample.count)
            {
                uint32_t sampled_biome;
                double sampled_weight;

                sampled_biome = column_cache[column_index].biome_sample
                    .biome_indices[sample_index];
                sampled_weight = column_cache[column_index].biome_sample
                    .weights[sample_index];
                column_cache[column_index].biome_profile.surface_height +=
                    static_cast<int32_t>(static_cast<double>(config.biomes[
                        sampled_biome].profile.surface_height) * sampled_weight);
                column_cache[column_index].biome_profile.height_variation +=
                    static_cast<int32_t>(static_cast<double>(config.biomes[
                        sampled_biome].profile.height_variation) * sampled_weight);
                column_cache[column_index].biome_profile.topsoil_depth +=
                    static_cast<int32_t>(static_cast<double>(config.biomes[
                        sampled_biome].profile.topsoil_depth) * sampled_weight);
                shrub_chance += sampled_weight * static_cast<double>(config.biomes[
                    sampled_biome].shrub_chance_percent);
                tree_chance += sampled_weight * static_cast<double>(config.biomes[
                    sampled_biome].tree_chance_percent);
                if (config.biomes[sampled_biome].allow_mountain_ridges == FT_TRUE)
                    mountain_weight += sampled_weight;
                if (config.biomes[sampled_biome].allow_snow_caps == FT_TRUE)
                    snow_weight += sampled_weight;
                sample_index += 1U;
            }
            column_cache[column_index].column_height
                = terrain_smooth_heightfield(seed_value, world_block_x,
                    world_block_z, config);
            column_cache[column_index].surface_water_kind =
                terrain_surface_water_kind(seed_value, world_block_x,
                    world_block_z, config);
            column_cache[column_index].has_surface_water =
                column_cache[column_index].surface_water_kind
                    != TERRAIN_SURFACE_WATER_NONE ? FT_TRUE : FT_FALSE;
            column_cache[column_index].surface_water_feature_id = 0U;
            column_cache[column_index].surface_water_bank_distance = 0U;
            column_cache[column_index].surface_water_level = config.sea_level;
            if (column_cache[column_index].surface_water_kind
                    == TERRAIN_SURFACE_WATER_RIVER)
                column_cache[column_index].surface_water_level -= 1;
            else if (column_cache[column_index].surface_water_kind
                    == TERRAIN_SURFACE_WATER_LAKE)
                column_cache[column_index].surface_water_level -= 2;
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t original_height =
                    column_cache[column_index].column_height;
                if (original_height > column_cache[column_index]
                        .surface_water_level + 4)
                    column_cache[column_index].has_surface_water = FT_FALSE;
                else
                    column_cache[column_index].column_height =
                        terrain_surface_water_bed_height(seed_value,
                            world_block_x, world_block_z, original_height,
                            config);
                if (column_cache[column_index].column_height
                        >= column_cache[column_index].surface_water_level)
                    column_cache[column_index].has_surface_water = FT_FALSE;
            }
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t feature_cell_size =
                    column_cache[column_index].surface_water_kind
                        == TERRAIN_SURFACE_WATER_RIVER
                        ? config.fluids.river_noise_scale
                        : config.fluids.lake_noise_scale;
                const int32_t feature_cell_x = feature_cell_size > 0
                    ? terrain_floor_division(world_block_x,
                        feature_cell_size) * feature_cell_size
                    : world_block_x;
                const int32_t feature_cell_z = feature_cell_size > 0
                    ? terrain_floor_division(world_block_z,
                        feature_cell_size) * feature_cell_size
                    : world_block_z;
                column_cache[column_index].surface_water_feature_id =
                    terrain_feature_seed(seed_value, feature_cell_x,
                        feature_cell_z, TERRAIN_FEATURE_WATER_SALT
                            ^ static_cast<uint64_t>(column_cache[column_index]
                                .surface_water_kind));
                column_cache[column_index].surface_water_depth =
                    static_cast<uint32_t>(column_cache[column_index]
                        .surface_water_level
                        - column_cache[column_index].column_height);
                column_cache[column_index].surface_water_bank_distance =
                    column_cache[column_index].surface_water_kind
                        == TERRAIN_SURFACE_WATER_RIVER ? 1U : 2U;
            }
            else
            {
                column_cache[column_index].surface_water_depth = 0U;
            }
            column_cache[column_index].slope_height
                = terrain_estimate_slope(seed_value, world_block_x,
                    world_block_z, config);
            column_cache[column_index].deep_block_id = config.biomes[
                column_cache[column_index].biome].deep_block_id;
            column_cache[column_index].can_place_shrubs = FT_FALSE;
            column_cache[column_index].can_place_trees = FT_FALSE;
            column_cache[column_index].can_place_snow = FT_FALSE;
            column_cache[column_index].can_place_mountain_ridges = FT_FALSE;
            sample_index = 0U;
            while (sample_index < column_cache[column_index].biome_sample.count)
            {
                uint32_t sampled_biome;

                sampled_biome = column_cache[column_index].biome_sample
                    .biome_indices[sample_index];
                if (config.biomes[sampled_biome].allow_shrubs == FT_TRUE)
                    column_cache[column_index].can_place_shrubs = FT_TRUE;
                if (config.biomes[sampled_biome].allow_trees == FT_TRUE)
                    column_cache[column_index].can_place_trees = FT_TRUE;
                sample_index += 1U;
            }
            if (snow_weight >= 0.25)
                column_cache[column_index].can_place_snow = FT_TRUE;
            if (mountain_weight >= 0.25)
                column_cache[column_index].can_place_mountain_ridges = FT_TRUE;
            if (shrub_chance > 100.0)
                shrub_chance = 100.0;
            if (tree_chance > 100.0)
                tree_chance = 100.0;
            column_cache[column_index].shrub_chance_percent =
                static_cast<uint32_t>(shrub_chance);
            column_cache[column_index].tree_chance_percent =
                static_cast<uint32_t>(tree_chance);
            local_x += 1;
        }
        local_z += 1;
    }
    return ;
}

static ft_bool terrain_can_place_tree_with_writer(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const terrain_tree_template &tree_template,
    const terrain_generation_config &config) noexcept
{
    uint32_t block_index;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    uint32_t block_id;

    if (tree_template.blocks == ft_nullptr)
        return (FT_FALSE);
    block_index = 0U;
    while (block_index < tree_template.block_count)
    {
        target_x = local_origin_x + tree_template.blocks[block_index].offset_x;
        target_y = local_origin_y + tree_template.blocks[block_index].offset_y;
        target_z = local_origin_z + tree_template.blocks[block_index].offset_z;
        if (target_x < 0 || target_x >= GAME_VOXEL_CHUNK_WIDTH
            || target_y < 0 || target_y >= GAME_VOXEL_CHUNK_HEIGHT
            || target_z < 0 || target_z >= GAME_VOXEL_CHUNK_DEPTH)
        {
            if (config.allow_cross_chunk_features == FT_FALSE
                || config.cross_chunk_block_writer == ft_nullptr)
                return (FT_FALSE);
        }
        else
        {
            if (chunk.read_block(target_x, target_y, target_z, &block_id)
                != FT_ERR_SUCCESS
                || terrain_block_is_replaceable(block_id) == FT_FALSE)
                return (FT_FALSE);
        }
        block_index += 1U;
    }
    return (FT_TRUE);
}

static int32_t terrain_place_tree_with_writer(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const terrain_tree_template &tree_template,
    const terrain_generation_config &config) noexcept
{
    uint32_t block_index;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    int32_t error_code;

    block_index = 0U;
    while (block_index < tree_template.block_count)
    {
        target_x = local_origin_x + tree_template.blocks[block_index].offset_x;
        target_y = local_origin_y + tree_template.blocks[block_index].offset_y;
        target_z = local_origin_z + tree_template.blocks[block_index].offset_z;
        if (target_x >= 0 && target_x < GAME_VOXEL_CHUNK_WIDTH
            && target_y >= 0 && target_y < GAME_VOXEL_CHUNK_HEIGHT
            && target_z >= 0 && target_z < GAME_VOXEL_CHUNK_DEPTH)
            error_code = chunk.write_generated_block(target_x, target_y, target_z,
                tree_template.blocks[block_index].block_id);
        else
            error_code = config.cross_chunk_block_writer(
                world_block_origin_x + target_x, target_y,
                world_block_origin_z + target_z,
                tree_template.blocks[block_index].block_id,
                config.cross_chunk_block_writer_user_data);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        block_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_region_cross_chunk_block_writer(int32_t world_block_x,
    int32_t world_block_y, int32_t world_block_z, uint32_t block_id,
    void *user_data) noexcept
{
    game_voxel_region *region;

    region = static_cast<game_voxel_region *>(user_data);
    if (region == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (region->write_generated_block(world_block_x, world_block_y,
        world_block_z, block_id));
}

static int32_t terrain_region_cross_chunk_block_reader(int32_t world_block_x,
    int32_t world_block_y, int32_t world_block_z, uint32_t *block_id,
    void *user_data) noexcept
{
    game_voxel_region *region;

    region = static_cast<game_voxel_region *>(user_data);
    if (region == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (region->read_block(world_block_x, world_block_y, world_block_z,
        block_id));
}

static int32_t terrain_column_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_biome_profile &biome_profile,
    ft_bool allow_mountain_ridges,
    const terrain_generation_config &config) noexcept
{
    double large_noise;
    double detail_noise;
    double total_noise;
    int32_t surface_height;
    int32_t variation;

    large_noise = terrain_value_noise(seed_value, world_block_x, world_block_z,
        config.large_noise_scale > 0 ? config.large_noise_scale : TERRAIN_HEIGHTMAP_LARGE_SCALE);
    detail_noise = terrain_value_noise(seed_value ^ UINT64_C(0xA5A5A5A5A5A5A5A5),
        world_block_x, world_block_z,
        config.detail_noise_scale > 0 ? config.detail_noise_scale : TERRAIN_HEIGHTMAP_DETAIL_SCALE);
    variation = biome_profile.height_variation;
    total_noise = (large_noise * static_cast<double>(variation))
        + (detail_noise * static_cast<double>(variation)
            * static_cast<double>(config.detail_noise_percent) / 100.0);
    if (config.enable_mountain_ridges == FT_TRUE
        && allow_mountain_ridges == FT_TRUE)
    {
        int32_t mountain_region_scale;
        int32_t mountain_detail_scale;
        int32_t warped_x;
        int32_t warped_z;
        int32_t warp_scale;
        int32_t warp_offset_x;
        int32_t warp_offset_z;
        double region_noise;
        double region_mask;
        double ridge_noise;
        double detail_ridge_noise;
        double valley_noise;
        double ridge_shape;
        double valley_shape;
        double mountain_strength;

        mountain_region_scale = config.mountain_ridge_scale * 4;
        if (mountain_region_scale < config.mountain_ridge_scale)
            mountain_region_scale = config.mountain_ridge_scale;
        mountain_detail_scale = config.mountain_ridge_scale / 2;
        if (mountain_detail_scale < 1)
            mountain_detail_scale = 1;
        warp_scale = mountain_region_scale / 2;
        if (warp_scale < 1)
            warp_scale = 1;
        warp_offset_x = static_cast<int32_t>(terrain_value_noise(
            seed_value ^ UINT64_C(0x243F6A8885A308D3), world_block_x,
            world_block_z, warp_scale) * static_cast<double>(warp_scale * 2));
        warp_offset_z = static_cast<int32_t>(terrain_value_noise(
            seed_value ^ UINT64_C(0x13198A2E03707344), world_block_x,
            world_block_z, warp_scale) * static_cast<double>(warp_scale * 2));
        warped_x = world_block_x + warp_offset_x;
        warped_z = world_block_z + warp_offset_z;
        region_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x510E527FADE682D1), warped_x, warped_z,
            mountain_region_scale);
        region_noise = (region_noise + 1.0) * 0.5;
        region_mask = (region_noise - 0.35) / 0.4;
        if (region_mask < 0.0)
            region_mask = 0.0;
        if (region_mask > 1.0)
            region_mask = 1.0;
        region_mask = terrain_smooth_factor(region_mask);
        ridge_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x6A09E667F3BCC909), warped_x, warped_z,
            config.mountain_ridge_scale);
        detail_ridge_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x3C6EF372FE94F82B), warped_x, warped_z,
            mountain_detail_scale);
        valley_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x1D6F80B3C9274A55), warped_x, warped_z,
            config.mountain_ridge_scale * 2);
        if (ridge_noise < 0.0)
            ridge_noise = -ridge_noise;
        if (detail_ridge_noise < 0.0)
            detail_ridge_noise = -detail_ridge_noise;
        if (valley_noise < 0.0)
            valley_noise = -valley_noise;
        ridge_shape = ((1.0 - ridge_noise) * 0.65)
            + ((1.0 - detail_ridge_noise) * 0.35);
        valley_shape = (1.0 - valley_noise) * region_mask;
        mountain_strength = static_cast<double>(
            config.mountain_ridge_strength);
        total_noise += region_mask * mountain_strength
            * (1.5 + (ridge_shape * 1.5));
        total_noise -= valley_shape * mountain_strength * 0.65;
    }
    if (config.enable_erosion == FT_TRUE)
    {
        double erosion_noise;

        erosion_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0xBB67AE8584CAA73B), world_block_x, world_block_z,
            config.erosion_noise_scale);
        if (erosion_noise > 0.0)
            total_noise -= erosion_noise
                * static_cast<double>(config.erosion_strength);
    }
    surface_height = biome_profile.surface_height
        + static_cast<int32_t>(total_noise);
    return (surface_height);
}

static void terrain_add_biome_sample(terrain_biome_sample *sample,
    uint32_t biome, double weight) noexcept
{
    uint32_t index;

    if (sample == ft_nullptr || weight <= 0.0)
        return ;
    index = 0U;
    while (index < sample->count)
    {
        if (sample->biome_indices[index] == biome)
        {
            sample->weights[index] += weight;
            return ;
        }
        index += 1U;
    }
    if (sample->count >= 4U)
        return ;
    sample->biome_indices[sample->count] = biome;
    sample->weights[sample->count] = weight;
    sample->count += 1U;
    return ;
}

static void terrain_sample_biomes(const terrain_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z,
    terrain_biome_sample *sample) noexcept
{
    int32_t cell_x;
    int32_t cell_z;
    int32_t neighbour_x;
    int32_t neighbour_z;
    int32_t cell_origin_x;
    int32_t cell_origin_z;
    int32_t biome_zone_width;
    int32_t site_x;
    int32_t site_z;
    uint32_t site_biome;
    uint32_t candidate_index;
    uint32_t selected_index;
    uint32_t selected_count;
    uint32_t selected_biomes[4];
    double selected_weights[4];
    double distance_x;
    double distance_z;
    double distance_squared;
    double site_weight;
    double weight_total;
    uint32_t index;

    if (sample == ft_nullptr)
        return ;
    sample->count = 0U;
    if (config.biome_count == 0U)
        return ;
    biome_zone_width = terrain_get_biome_zone_width(config, seed_value);
    cell_x = terrain_floor_div(world_block_x,
        biome_zone_width);
    cell_z = terrain_floor_div(world_block_z,
        biome_zone_width);
    if (config.enable_biome_transitions == FT_FALSE
        || config.biome_transition_noise_strength == 0U)
    {
        terrain_add_biome_sample(sample, terrain_select_biome(config,
            seed_value, world_block_x, world_block_z), 1.0);
        return ;
    }
    selected_count = 0U;
    neighbour_z = -1;
    while (neighbour_z <= 1)
    {
        neighbour_x = -1;
        while (neighbour_x <= 1)
        {
            cell_origin_x = (cell_x + neighbour_x)
                * biome_zone_width;
            cell_origin_z = (cell_z + neighbour_z)
                * biome_zone_width;
            site_x = cell_origin_x + biome_zone_width / 2
                + static_cast<int32_t>(terrain_signed_unit_noise(
                    seed_value ^ UINT64_C(0xA24BAED4963EE407),
                    cell_x + neighbour_x, cell_z + neighbour_z)
                    * static_cast<double>(biome_zone_width) * 0.35);
            site_z = cell_origin_z + biome_zone_width / 2
                + static_cast<int32_t>(terrain_signed_unit_noise(
                    seed_value ^ UINT64_C(0x9FB21C651E98DF25),
                    cell_x + neighbour_x, cell_z + neighbour_z)
                    * static_cast<double>(biome_zone_width) * 0.35);
            distance_x = static_cast<double>(world_block_x - site_x);
            distance_z = static_cast<double>(world_block_z - site_z);
            distance_squared = (distance_x * distance_x)
                + (distance_z * distance_z);
            site_weight = 1.0 / (distance_squared + 64.0);
            site_biome = terrain_select_biome(config, seed_value,
                cell_origin_x, cell_origin_z);
            candidate_index = 0U;
            while (candidate_index < selected_count
                && site_weight <= selected_weights[candidate_index])
                candidate_index += 1U;
            if (candidate_index < 4U)
            {
                if (selected_count < 4U)
                    selected_count += 1U;
                selected_index = selected_count - 1U;
                while (selected_index > candidate_index)
                {
                    selected_weights[selected_index]
                        = selected_weights[selected_index - 1U];
                    selected_biomes[selected_index]
                        = selected_biomes[selected_index - 1U];
                    selected_index -= 1U;
                }
                selected_weights[candidate_index] = site_weight;
                selected_biomes[candidate_index] = site_biome;
            }
            neighbour_x += 1;
        }
        neighbour_z += 1;
    }
    index = 0U;
    while (index < selected_count)
    {
        terrain_add_biome_sample(sample, selected_biomes[index],
            selected_weights[index]);
        index += 1U;
    }
    weight_total = 0.0;
    index = 0U;
    while (index < sample->count)
    {
        weight_total += sample->weights[index];
        index += 1U;
    }
    if (weight_total <= 0.0)
    {
        sample->count = 1U;
        sample->biome_indices[0] = terrain_select_biome(config, seed_value,
            world_block_x, world_block_z);
        sample->weights[0] = 1.0;
        return ;
    }
    index = 0U;
    while (index < sample->count)
    {
        sample->weights[index] /= weight_total;
        index += 1U;
    }
    return ;
}

static uint32_t terrain_sample_biome_block(
    const terrain_generation_config &config,
    const terrain_biome_sample &sample, uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_y, int32_t world_block_z,
    uint64_t material_salt, ft_bool subsurface) noexcept
{
    uint64_t sample_seed;
    double sample_value;
    double accumulated_weight;
    uint32_t index;
    uint32_t biome;

    sample_seed = seed_value ^ material_salt
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(world_block_x))
            * UINT64_C(0xA24BAED4963EE407))
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(world_block_y))
            * UINT64_C(0x9FB21C651E98DF25))
        ^ (static_cast<uint64_t>(static_cast<uint32_t>(world_block_z))
            * UINT64_C(0xC13FA9A902A6328F));
    sample_value = static_cast<double>(terrain_mix_u64(sample_seed)
        % 100000U) / 100000.0;
    accumulated_weight = 0.0;
    index = 0U;
    while (index < sample.count)
    {
        biome = sample.biome_indices[index];
        accumulated_weight += sample.weights[index];
        if (sample_value <= accumulated_weight || index + 1U == sample.count)
        {
            if (subsurface == FT_TRUE)
                return (config.biomes[biome].subsurface_block_id);
            return (config.biomes[biome].surface_block_id);
        }
        index += 1U;
    }
    return (config.biomes[sample.biome_indices[0]].surface_block_id);
}

static const terrain_tree_template *terrain_sample_tree_template(
    const terrain_generation_config &config,
    const terrain_biome_sample &sample, uint64_t seed_value)
{
    uint32_t index;
    uint32_t selected_biome;
    double tree_weight_total;
    double sample_value;
    double accumulated_weight;
    const terrain_tree_template *tree_template;

    tree_weight_total = 0.0;
    index = 0U;
    while (index < sample.count)
    {
        selected_biome = sample.biome_indices[index];
        if (config.biomes[selected_biome].allow_trees == FT_TRUE
            && (config.biomes[selected_biome].tree_template != ft_nullptr
                || config.biomes[selected_biome].tree_template_count > 0U))
            tree_weight_total += sample.weights[index];
        index += 1U;
    }
    if (tree_weight_total <= 0.0)
        return (ft_nullptr);
    selected_biome = sample.biome_indices[0];
    sample_value = static_cast<double>(terrain_mix_u64(seed_value)
        % 100000U) / 100000.0 * tree_weight_total;
    accumulated_weight = 0.0;
    index = 0U;
    while (index < sample.count)
    {
        selected_biome = sample.biome_indices[index];
        if (config.biomes[selected_biome].allow_trees == FT_TRUE
            && (config.biomes[selected_biome].tree_template != ft_nullptr
                || config.biomes[selected_biome].tree_template_count > 0U))
        {
            selected_biome = sample.biome_indices[index];
            accumulated_weight += sample.weights[index];
            if (sample_value <= accumulated_weight)
                break ;
        }
        index += 1U;
    }
    tree_template = config.biomes[selected_biome].tree_template;
    if (tree_template == ft_nullptr
        && config.biomes[selected_biome].tree_template_count > 0U)
    {
        tree_template = terrain_generation_config_get_tree_template(config,
            config.biomes[selected_biome].tree_template_indices[
                terrain_mix_u64(seed_value ^ TERRAIN_FEATURE_TREE_SALT)
                    % config.biomes[selected_biome].tree_template_count]);
    }
    return (tree_template);
}

static int32_t terrain_smooth_biome_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_biome_definition &biome_definition,
    const terrain_generation_config &config) noexcept
{
    terrain_biome_sample sample;
    int32_t height;
    uint32_t index;

    (void)biome_definition;
    terrain_sample_biomes(config, seed_value, world_block_x, world_block_z,
        &sample);
    height = 0;
    index = 0U;
    while (index < sample.count)
    {
        height += static_cast<int32_t>(terrain_column_height(seed_value,
            world_block_x, world_block_z,
            config.biomes[sample.biome_indices[index]].profile,
            config.biomes[sample.biome_indices[index]].allow_mountain_ridges,
            config) * sample.weights[index]);
        index += 1U;
    }
    return (height);
}

static int32_t terrain_estimate_slope(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept
{
    int32_t height_west;
    int32_t height_east;
    int32_t height_north;
    int32_t height_south;
    int32_t slope_x;
    int32_t slope_z;

    height_west = terrain_sample_height(seed_value, world_block_x - 1,
        world_block_z, config);
    height_east = terrain_sample_height(seed_value, world_block_x + 1,
        world_block_z, config);
    height_north = terrain_sample_height(seed_value, world_block_x,
        world_block_z - 1, config);
    height_south = terrain_sample_height(seed_value, world_block_x,
        world_block_z + 1, config);
    slope_x = height_east - height_west;
    if (slope_x < 0)
        slope_x = -slope_x;
    slope_z = height_south - height_north;
    if (slope_z < 0)
        slope_z = -slope_z;
    return (slope_x + slope_z);
}

static int32_t terrain_sample_height(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, const terrain_generation_config &config) noexcept
{
    uint32_t biome;
    uint32_t biome_index;
    int32_t minimum_height;
    int32_t maximum_height;
    int32_t candidate_minimum;
    int32_t candidate_maximum;
    int32_t height;

    biome = terrain_select_biome(config, seed_value, world_block_x, world_block_z);
    height = terrain_smooth_biome_height(seed_value, world_block_x,
        world_block_z, config.biomes[biome], config);
    minimum_height = config.biomes[0].profile.surface_height
        - config.biomes[0].profile.height_variation
        - (config.biomes[0].profile.height_variation / 2);
    maximum_height = config.biomes[0].profile.surface_height
        + config.biomes[0].profile.height_variation
        + (config.biomes[0].profile.height_variation / 2);
    biome_index = 1U;
    while (biome_index < config.biome_count)
    {
        candidate_minimum = config.biomes[biome_index].profile.surface_height
            - config.biomes[biome_index].profile.height_variation
            - (config.biomes[biome_index].profile.height_variation / 2);
        candidate_maximum = config.biomes[biome_index].profile.surface_height
            + config.biomes[biome_index].profile.height_variation
            + (config.biomes[biome_index].profile.height_variation / 2);
        if (candidate_minimum < minimum_height)
            minimum_height = candidate_minimum;
        if (candidate_maximum > maximum_height)
            maximum_height = candidate_maximum;
        biome_index += 1U;
    }
    if (height < minimum_height)
        return (minimum_height);
    if (height > maximum_height)
        return (maximum_height);
    return (height);
}

static int32_t terrain_smooth_heightfield(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept
{
    int32_t offset_x;
    int32_t offset_z;
    int32_t sample_count;
    int32_t weighted_height;
    int32_t sample_height;
    int32_t sample_weight;

    offset_z = -TERRAIN_HEIGHTMAP_SMOOTH_RADIUS;
    sample_count = 0;
    weighted_height = 0;
    while (offset_z <= TERRAIN_HEIGHTMAP_SMOOTH_RADIUS)
    {
        offset_x = -TERRAIN_HEIGHTMAP_SMOOTH_RADIUS;
        while (offset_x <= TERRAIN_HEIGHTMAP_SMOOTH_RADIUS)
        {
            sample_height = terrain_sample_height(seed_value,
                world_block_x + offset_x, world_block_z + offset_z, config);
            if (offset_x == 0 && offset_z == 0)
                sample_weight = 4;
            else if (offset_x == 0 || offset_z == 0)
                sample_weight = 2;
            else
                sample_weight = 1;
            weighted_height += sample_height * sample_weight;
            sample_count += sample_weight;
            offset_x += 1;
        }
        offset_z += 1;
    }
    if (sample_count <= 0)
        return (terrain_sample_height(seed_value, world_block_x,
            world_block_z, config));
    return (weighted_height / sample_count);
}

static ft_bool terrain_should_place_feature(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z, uint64_t salt,
    uint64_t threshold) noexcept
{
    uint64_t feature_seed;

    feature_seed = terrain_feature_seed(seed_value, world_block_x,
        world_block_z, salt);
    if ((feature_seed % 100U) < threshold)
        return (FT_TRUE);
    return (FT_FALSE);
}

static uint32_t terrain_ground_cover_block_for_biome(uint32_t biome,
    uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z) noexcept
{
    uint64_t variant_seed;
    uint32_t variant;

    variant_seed = terrain_feature_seed(seed_value, world_block_x,
        world_block_z, TERRAIN_FEATURE_GROUND_COVER_VARIANT_SALT);
    variant = static_cast<uint32_t>(variant_seed % 100U);
    if (biome == TERRAIN_BIOME_DESERT)
    {
        if (variant < 60U)
            return (TERRAIN_GENERATOR_DEAD_BUSH_BLOCK);
        return (TERRAIN_GENERATOR_SHRUB_BLOCK);
    }
    if (biome == TERRAIN_BIOME_SNOW || biome == TERRAIN_BIOME_MOUNTAINS)
        return (TERRAIN_GENERATOR_DEAD_BUSH_BLOCK);
    if (variant < 25U)
        return (TERRAIN_GENERATOR_RED_FLOWER_BLOCK);
    if (variant < 45U)
        return (TERRAIN_GENERATOR_YELLOW_FLOWER_BLOCK);
    if (variant < 65U)
        return (TERRAIN_GENERATOR_TALL_GRASS_BLOCK);
    if (variant < 80U)
        return (TERRAIN_GENERATOR_FERN_BLOCK);
    if (variant < 88U)
        return (TERRAIN_GENERATOR_RED_MUSHROOM_BLOCK);
    if (variant < 96U)
        return (TERRAIN_GENERATOR_BROWN_MUSHROOM_BLOCK);
    return (TERRAIN_GENERATOR_SHRUB_BLOCK);
}

static double terrain_cave_noise(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_y, int32_t world_block_z,
    int32_t scale, uint64_t salt) noexcept
{
    uint64_t layer_seed;
    int32_t layer_y;

    layer_y = terrain_floor_div(world_block_y, scale);
    layer_seed = seed_value ^ salt
        ^ (static_cast<uint64_t>(layer_y) * UINT64_C(0x9E3779B97F4A7C15));
    return (terrain_value_noise(layer_seed,
        world_block_x + (world_block_y * 13),
        world_block_z - (world_block_y * 7), scale));
}

static ft_bool terrain_should_carve_cave(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_y, int32_t world_block_z,
    int32_t surface_height,
    const terrain_generation_config &config) noexcept
{
    double primary_noise;
    double detail_noise;
    double ravine_detail_threshold;
    int32_t cave_surface_margin;
    int32_t cell_x;
    int32_t cell_y;
    int32_t cell_z;
    int32_t offset_x;
    int32_t offset_y;
    int32_t offset_z;
    int32_t candidate_cell_x;
    int32_t candidate_cell_y;
    int32_t candidate_cell_z;
    int32_t center_x;
    int32_t center_y;
    int32_t center_z;
    int32_t distance_x;
    int32_t distance_y;
    int32_t distance_z;
    int32_t radius;
    int32_t height_range;
    uint64_t candidate_seed;
    ft_bool large_cave;
    ft_bool cavern_room;
    ft_bool entrance_candidate;

    if (world_block_y < config.underground_structures.minimum_height
        || world_block_y > surface_height)
        return (FT_FALSE);
    if (config.underground_structures.enable_ravines == FT_TRUE)
    {
        cave_surface_margin = TERRAIN_CAVE_SURFACE_MARGIN;
        if (config.underground_structures.ravine_depth > 0U)
            cave_surface_margin = static_cast<int32_t>(
                config.underground_structures.ravine_depth);
        if (world_block_y >= config.underground_structures.minimum_height
            && world_block_y <= config.underground_structures.maximum_height
            && world_block_y < surface_height - cave_surface_margin)
        {
            primary_noise = terrain_cave_noise(seed_value, world_block_x,
                world_block_y, world_block_z, TERRAIN_CAVE_PRIMARY_SCALE,
                TERRAIN_CAVE_PRIMARY_SALT);
            detail_noise = terrain_cave_noise(seed_value, world_block_x,
                world_block_y, world_block_z, TERRAIN_CAVE_DETAIL_SCALE,
                TERRAIN_CAVE_DETAIL_SALT);
            ravine_detail_threshold = -0.12
                + (static_cast<double>(config.underground_structures
                    .ravine_width) * 0.04);
            if (terrain_should_place_feature(seed_value, world_block_x,
                    world_block_z, UINT64_C(0xD1CEB00C),
                    config.underground_structures.ravine_chance_percent)
                    == FT_TRUE
                && primary_noise > 0.34
                && detail_noise > ravine_detail_threshold)
                return (FT_TRUE);
        }
    }
    if (config.underground_structures.enable_cave_rooms == FT_FALSE
        || config.underground_structures.cave_room_chance_percent == 0U
        || config.underground_structures.cave_small_radius == 0U)
        return (FT_FALSE);
    cell_x = terrain_floor_div(world_block_x, TERRAIN_CAVE_CELL_SIZE);
    cell_y = terrain_floor_div(world_block_y, TERRAIN_CAVE_CELL_SIZE);
    cell_z = terrain_floor_div(world_block_z, TERRAIN_CAVE_CELL_SIZE);
    offset_x = -1;
    while (offset_x <= 1)
    {
        offset_y = -1;
        while (offset_y <= 1)
        {
            offset_z = -1;
            while (offset_z <= 1)
            {
                candidate_cell_x = cell_x + offset_x;
                candidate_cell_y = cell_y + offset_y;
                candidate_cell_z = cell_z + offset_z;
                candidate_seed = terrain_mix_u64(seed_value
                    ^ static_cast<uint64_t>(static_cast<int64_t>(
                        candidate_cell_x)) * UINT64_C(0x9E3779B97F4A7C15)
                    ^ static_cast<uint64_t>(static_cast<int64_t>(
                        candidate_cell_y)) * UINT64_C(0xC2B2AE3D27D4EB4F)
                    ^ static_cast<uint64_t>(static_cast<int64_t>(
                        candidate_cell_z)) * UINT64_C(0x165667B19E3779F9)
                    ^ UINT64_C(0xCA7E700D));
                if ((candidate_seed % 100U)
                    < config.underground_structures.cave_room_chance_percent)
                {
                    center_x = candidate_cell_x * TERRAIN_CAVE_CELL_SIZE
                        + 2 + static_cast<int32_t>((candidate_seed >> 8)
                            % (TERRAIN_CAVE_CELL_SIZE - 4));
                    center_z = candidate_cell_z * TERRAIN_CAVE_CELL_SIZE
                        + 2 + static_cast<int32_t>((candidate_seed >> 16)
                            % (TERRAIN_CAVE_CELL_SIZE - 4));
                    height_range = config.underground_structures.maximum_height
                        - config.underground_structures.minimum_height + 1;
                    if (height_range < 1)
                        height_range = 1;
                    center_y = config.underground_structures.minimum_height
                        + static_cast<int32_t>((candidate_seed >> 24)
                            % static_cast<uint64_t>(height_range));
                    large_cave = ((candidate_seed >> 32) % 100U
                        < config.underground_structures
                            .cave_large_chance_percent);
                    cavern_room = (config.underground_structures
                        .enable_cavern_rooms == FT_TRUE
                        && ((candidate_seed >> 48) % 100U
                            < config.underground_structures
                                .cavern_room_chance_percent));
                    if (cavern_room == FT_TRUE)
                        radius = static_cast<int32_t>(config
                            .underground_structures.cavern_room_radius);
                    else if (large_cave == FT_TRUE)
                        radius = static_cast<int32_t>(config
                            .underground_structures.cave_large_radius);
                    else
                        radius = static_cast<int32_t>(config
                            .underground_structures.cave_small_radius);
                    distance_x = world_block_x - center_x;
                    distance_y = world_block_y - center_y;
                    distance_z = world_block_z - center_z;
                    if (distance_x * distance_x + distance_y * distance_y
                        + distance_z * distance_z <= radius * radius
                        && world_block_y < surface_height
                            - TERRAIN_CAVE_SURFACE_MARGIN)
                        return (FT_TRUE);
                    entrance_candidate = ((candidate_seed >> 40) % 100U
                        < config.underground_structures
                            .cave_entrance_chance_percent);
                    if (entrance_candidate == FT_TRUE
                        && world_block_y <= surface_height
                        && world_block_y >= center_y
                        && world_block_y < surface_height
                        && (distance_x * distance_x + distance_z * distance_z
                            <= static_cast<int32_t>(config
                                .underground_structures.cave_entrance_radius)
                                * static_cast<int32_t>(config
                                    .underground_structures
                                    .cave_entrance_radius)))
                        return (FT_TRUE);
                }
                offset_z += 1;
            }
            offset_y += 1;
        }
        offset_x += 1;
    }
    return (FT_FALSE);
}

static uint8_t terrain_surface_water_kind(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept
{
    double river_noise;
    uint64_t lake_region_seed;
    int32_t lake_region_size;
    int32_t lake_region_x;
    int32_t lake_region_z;
    int32_t lake_center_x;
    int32_t lake_center_z;
    int32_t lake_radius;
    int32_t lake_jitter_range;
    int32_t distance_x;
    int32_t distance_z;

    if (terrain_should_place_feature(seed_value, world_block_x, world_block_z,
            TERRAIN_FEATURE_WATER_SALT, config.water_chance_percent)
        == FT_TRUE)
        return (TERRAIN_SURFACE_WATER_LEGACY);
    if (config.fluids.enable_rivers == FT_TRUE)
    {
        river_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x3C6EF372FE94F82B), world_block_x, world_block_z,
            config.fluids.river_noise_scale);
        if (river_noise < 0.0)
            river_noise = -river_noise;
        if (river_noise < (0.04 + (static_cast<double>(
                config.fluids.river_width) * 0.01)))
            return (TERRAIN_SURFACE_WATER_RIVER);
    }
    if (config.fluids.enable_lakes == FT_TRUE)
    {
        lake_region_size = config.fluids.lake_noise_scale;
        if (lake_region_size < 8)
            lake_region_size = 8;
        lake_region_x = terrain_floor_division(world_block_x,
            lake_region_size) * lake_region_size;
        lake_region_z = terrain_floor_division(world_block_z,
            lake_region_size) * lake_region_size;
        lake_region_seed = terrain_feature_seed(seed_value, lake_region_x,
            lake_region_z, TERRAIN_FEATURE_WATER_SALT
                ^ UINT64_C(0xA54FF53A5F1D36F1));
        if ((lake_region_seed % 100U) < config.fluids.lake_chance_percent)
        {
            lake_jitter_range = lake_region_size / 5;
            if (lake_jitter_range < 1)
                lake_jitter_range = 1;
            lake_center_x = lake_region_x + (lake_region_size / 2)
                + static_cast<int32_t>((lake_region_seed >> 8)
                    % static_cast<uint64_t>(lake_jitter_range * 2 + 1))
                - lake_jitter_range;
            lake_center_z = lake_region_z + (lake_region_size / 2)
                + static_cast<int32_t>((lake_region_seed >> 16)
                    % static_cast<uint64_t>(lake_jitter_range * 2 + 1))
                - lake_jitter_range;
            lake_radius = lake_region_size / 4
                + static_cast<int32_t>((lake_region_seed >> 24) % 3U);
            if (lake_radius < 3)
                lake_radius = 3;
            distance_x = world_block_x - lake_center_x;
            distance_z = world_block_z - lake_center_z;
            if (distance_x * distance_x + distance_z * distance_z
                    <= lake_radius * lake_radius)
                return (TERRAIN_SURFACE_WATER_LAKE);
        }
    }
    return (TERRAIN_SURFACE_WATER_NONE);
}

static int32_t terrain_surface_water_bed_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z, int32_t column_height,
    const terrain_generation_config &config) noexcept
{
    uint8_t kind;
    int32_t bed_height;

    kind = terrain_surface_water_kind(seed_value, world_block_x,
        world_block_z, config);
    bed_height = column_height;
    if (kind == TERRAIN_SURFACE_WATER_RIVER)
    {
        /* A river is a channel feature, not a random low-column fill. Keep
         * the carve bounded so it cannot flatten mountains or expose the
         * bottom of the world. */
        bed_height = config.sea_level - 3;
    }
    else if (kind == TERRAIN_SURFACE_WATER_LAKE)
    {
        bed_height = config.sea_level - 4;
    }
    else if (kind == TERRAIN_SURFACE_WATER_LEGACY)
        bed_height = config.sea_level - 3;
    if (bed_height < TERRAIN_BEDROCK_FLOOR_Y + 1)
        bed_height = TERRAIN_BEDROCK_FLOOR_Y + 1;
    return (bed_height);
}

static ft_bool terrain_stage_dependencies_are_met(uint32_t requested_mask,
    uint32_t previous_mask) noexcept
{
    uint32_t available_mask = requested_mask | previous_mask;
    const uint32_t base_and_caves = TERRAIN_STAGE_BASE_TERRAIN
        | TERRAIN_STAGE_CAVES;

    if ((requested_mask & TERRAIN_STAGE_FLUIDS) != 0U
        && (available_mask & TERRAIN_STAGE_BASE_TERRAIN) == 0U)
        return (FT_FALSE);
    if ((requested_mask & TERRAIN_STAGE_FLUIDS) != 0U
        && (previous_mask & (TERRAIN_STAGE_DECORATION
            | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES)) != 0U)
        return (FT_FALSE);
    if ((requested_mask & TERRAIN_STAGE_DECORATION) != 0U
        && (available_mask & (base_and_caves | TERRAIN_STAGE_FLUIDS))
            != (base_and_caves | TERRAIN_STAGE_FLUIDS))
        return (FT_FALSE);
    if ((requested_mask & TERRAIN_STAGE_STRUCTURES) != 0U
        && (available_mask & base_and_caves) != base_and_caves)
        return (FT_FALSE);
    if ((requested_mask & TERRAIN_STAGE_ORES) != 0U
        && (available_mask & base_and_caves) != base_and_caves)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool terrain_is_enclosed_underground_lake_site(
    game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const terrain_generation_config &config) noexcept
{
    if (terrain_underground_lake_geometry_is_valid(chunk, local_x, local_y,
            local_z, world_block_origin_x, world_block_origin_z,
            config) == FT_FALSE)
        return (FT_FALSE);
    if (config.cross_chunk_block_reader != ft_nullptr)
        return (FT_TRUE);
    if (config.fluids.underground_lake_depth != 1U)
        return (FT_TRUE);
    int32_t boundary_z = -2;
    while (boundary_z <= 2)
    {
        int32_t boundary_x = -2;
        while (boundary_x <= 2)
        {
            if (std::abs(boundary_x) == 2 || std::abs(boundary_z) == 2)
            {
                uint32_t boundary_block;
                if (chunk.read_block(local_x + boundary_x, local_y,
                        local_z + boundary_z, &boundary_block)
                        != FT_ERR_SUCCESS
                    || terrain_block_is_solid(boundary_block) == FT_FALSE)
                    return (FT_FALSE);
            }
            boundary_x += 1;
        }
        boundary_z += 1;
    }
    int32_t offset_z = -1;
    while (offset_z <= 1)
    {
        int32_t offset_x = -1;
        while (offset_x <= 1)
        {
            uint32_t block_id;
            uint32_t upper_block_id;
            if (chunk.read_block(local_x + offset_x, local_y,
                    local_z + offset_z, &block_id) != FT_ERR_SUCCESS
                || chunk.read_block(local_x + offset_x, local_y + 1,
                    local_z + offset_z, &upper_block_id) != FT_ERR_SUCCESS
                || block_id != TERRAIN_GENERATOR_AIR_BLOCK
                || upper_block_id != TERRAIN_GENERATOR_AIR_BLOCK)
                return (FT_FALSE);
            offset_x += 1;
        }
        offset_z += 1;
    }
    offset_z = -1;
    while (offset_z <= 1)
    {
        int32_t offset_x = -1;
        while (offset_x <= 1)
        {
            uint32_t floor_block;
            uint32_t roof_block;
            if (chunk.read_block(local_x + offset_x, local_y - 1,
                    local_z + offset_z, &floor_block) != FT_ERR_SUCCESS
                || chunk.read_block(local_x + offset_x, local_y + 2,
                    local_z + offset_z, &roof_block) != FT_ERR_SUCCESS
                || terrain_block_is_solid(floor_block) == FT_FALSE
                || terrain_block_is_solid(roof_block) == FT_FALSE)
                return (FT_FALSE);
            offset_x += 1;
        }
        offset_z += 1;
    }
    return (FT_TRUE);
}

static ft_bool terrain_can_create_underground_lake_site(
    game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const terrain_generation_config &config) noexcept
{
    if (terrain_underground_lake_geometry_is_valid(chunk, local_x, local_y,
            local_z, world_block_origin_x, world_block_origin_z,
            config) == FT_FALSE)
        return (FT_FALSE);
    if (config.cross_chunk_block_reader != ft_nullptr)
        return (FT_TRUE);
    if (config.fluids.underground_lake_depth != 1U)
        return (FT_TRUE);
    int32_t offset_z = -2;
    while (offset_z <= 2)
    {
        int32_t offset_x = -2;
        while (offset_x <= 2)
        {
            if (std::abs(offset_x) == 2 || std::abs(offset_z) == 2)
            {
                uint32_t wall_block;
                uint32_t upper_wall_block;
                if (chunk.read_block(local_x + offset_x, local_y,
                        local_z + offset_z, &wall_block) != FT_ERR_SUCCESS
                    || chunk.read_block(local_x + offset_x, local_y + 1,
                        local_z + offset_z, &upper_wall_block) != FT_ERR_SUCCESS
                    || terrain_block_is_solid(wall_block) == FT_FALSE
                    || terrain_block_is_solid(upper_wall_block) == FT_FALSE)
                    return (FT_FALSE);
            }
            offset_x += 1;
        }
        offset_z += 1;
    }
    offset_z = -1;
    while (offset_z <= 1)
    {
        int32_t offset_x = -1;
        while (offset_x <= 1)
        {
            uint32_t lower_block;
            uint32_t upper_block;
            if (chunk.read_block(local_x + offset_x, local_y,
                    local_z + offset_z, &lower_block) != FT_ERR_SUCCESS
                || chunk.read_block(local_x + offset_x, local_y + 1,
                    local_z + offset_z, &upper_block) != FT_ERR_SUCCESS
                || lower_block != TERRAIN_GENERATOR_AIR_BLOCK
                || upper_block != TERRAIN_GENERATOR_AIR_BLOCK)
                return (FT_FALSE);
            offset_x += 1;
        }
        offset_z += 1;
    }
    offset_z = -1;
    while (offset_z <= 1)
    {
        int32_t offset_x = -1;
        while (offset_x <= 1)
        {
            uint32_t floor_block;
            uint32_t roof_block;
            if (chunk.read_block(local_x + offset_x, local_y - 1,
                    local_z + offset_z, &floor_block) != FT_ERR_SUCCESS
                || chunk.read_block(local_x + offset_x, local_y + 2,
                    local_z + offset_z, &roof_block) != FT_ERR_SUCCESS
                || terrain_block_is_solid(floor_block) == FT_FALSE
                || terrain_block_is_solid(roof_block) == FT_FALSE)
                return (FT_FALSE);
            offset_x += 1;
        }
        offset_z += 1;
    }
    return (FT_TRUE);
}

static ft_bool terrain_block_is_ore_host(uint32_t block_id,
    const terrain_generation_config &config) noexcept
{
    if (terrain_block_is_solid(block_id) == FT_FALSE)
        return (FT_FALSE);
    if (block_id == TERRAIN_GENERATOR_BEDROCK_BLOCK)
        return (FT_FALSE);
    if (config.layers.enable_snow_caps == FT_TRUE
        && block_id == config.layers.snow_cap_block_id)
        return (FT_FALSE);
    if (config.layers.enable_beaches == FT_TRUE
        && (block_id == config.layers.beach_block_id
            || block_id == config.layers.underwater_block_id))
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t terrain_find_surface_height(const game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_z) noexcept
{
    int32_t local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
    uint32_t block_id;

    while (local_y >= 0)
    {
        if (chunk.read_block(local_x, local_y, local_z, &block_id)
            != FT_ERR_SUCCESS)
            return (-1);
        if (terrain_block_is_solid(block_id) == FT_TRUE)
            return (local_y);
        local_y -= 1;
    }
    return (-1);
}

static int32_t terrain_place_ore_vein(game_voxel_chunk &chunk,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_y,
    int32_t world_block_z, int32_t local_x, int32_t local_y, int32_t local_z,
    const terrain_ore_rule &ore_rule, uint32_t vein_size,
    const terrain_generation_config &config) noexcept
{
    uint32_t vein_index;
    uint64_t vein_seed;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    uint32_t block_id;
    int32_t target_surface;
    int32_t target_depth;
    int32_t error_code;

    vein_index = 0U;
    while (vein_index < vein_size)
    {
        vein_seed = terrain_mix_u64(seed_value
            ^ static_cast<uint64_t>(world_block_x)
            ^ (static_cast<uint64_t>(world_block_y) << 21)
            ^ (static_cast<uint64_t>(world_block_z) << 42)
            ^ static_cast<uint64_t>(vein_index));
        target_x = local_x + static_cast<int32_t>((vein_seed >> 3) % 3U) - 1;
        target_y = local_y + static_cast<int32_t>((vein_seed >> 7) % 3U) - 1;
        target_z = local_z + static_cast<int32_t>((vein_seed >> 11) % 3U) - 1;
        if (target_x >= 0 && target_x < GAME_VOXEL_CHUNK_WIDTH
            && target_y >= 0 && target_y < GAME_VOXEL_CHUNK_HEIGHT
            && target_z >= 0 && target_z < GAME_VOXEL_CHUNK_DEPTH)
        {
            error_code = chunk.read_block(target_x, target_y, target_z,
                &block_id);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            target_surface = terrain_find_surface_height(chunk,
                target_x, target_z);
            target_depth = target_surface - target_y;
            if (target_surface >= 0
                && target_depth >= ore_rule.minimum_depth
                && target_depth <= ore_rule.maximum_depth
                && (terrain_block_is_ore_host(block_id, config) == FT_TRUE
                    || (ore_rule.allow_ore_replacement == FT_TRUE
                        && terrain_block_is_ore(block_id) == FT_TRUE)))
            {
                error_code = chunk.write_generated_block(target_x, target_y, target_z,
                    ore_rule.block_id);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
        }
        vein_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_generate_ores(game_voxel_chunk &chunk,
    uint64_t seed_value, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const terrain_generation_config &config) noexcept
{
    uint32_t ore_index;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    int32_t surface_height;
    uint32_t vein_count;
    uint32_t vein_size;
    uint32_t vein_index;
    uint64_t ore_seed;
    uint64_t vein_seed;
    int32_t error_code;

    ore_index = 0U;
    while (ore_index < config.ore_rule_count
        && ore_index < TERRAIN_MAX_ORE_RULES)
    {
        if (config.ores[ore_index].enabled == FT_TRUE)
        {
            if (config.ores[ore_index].vein_size_min == 0U
                || config.ores[ore_index].vein_size_max == 0U
                || config.ores[ore_index].veins_per_chunk_max == 0U
                || config.ores[ore_index].minimum_depth < 1)
            {
                ore_index += 1U;
                continue ;
            }
            ore_seed = terrain_mix_u64(seed_value
                ^ static_cast<uint64_t>(ore_index)
                    * UINT64_C(0x9E3779B97F4A7C15));
            vein_count = config.ores[ore_index].veins_per_chunk_min;
            if (config.ores[ore_index].veins_per_chunk_max
                > config.ores[ore_index].veins_per_chunk_min)
                vein_count += static_cast<uint32_t>(ore_seed
                    % (config.ores[ore_index].veins_per_chunk_max
                        - config.ores[ore_index].veins_per_chunk_min + 1U));
            vein_index = 0U;
            while (vein_index < vein_count)
            {
                vein_seed = terrain_mix_u64(ore_seed
                    ^ static_cast<uint64_t>(vein_index));
                local_x = static_cast<int32_t>((vein_seed >> 8)
                    % GAME_VOXEL_CHUNK_WIDTH);
                local_z = static_cast<int32_t>((vein_seed >> 24)
                    % GAME_VOXEL_CHUNK_DEPTH);
                surface_height = terrain_find_surface_height(chunk,
                    local_x, local_z);
                if (surface_height >= 0)
                {
                    local_y = surface_height
                        - config.ores[ore_index].minimum_depth;
                    if (config.ores[ore_index].maximum_depth
                        > config.ores[ore_index].minimum_depth)
                        local_y -= static_cast<int32_t>((vein_seed >> 40)
                            % (config.ores[ore_index].maximum_depth
                                - config.ores[ore_index].minimum_depth + 1U));
                    vein_size = config.ores[ore_index].vein_size_min;
                    if (config.ores[ore_index].vein_size_max
                        > config.ores[ore_index].vein_size_min)
                        vein_size += static_cast<uint32_t>((vein_seed >> 52)
                            % (config.ores[ore_index].vein_size_max
                                - config.ores[ore_index].vein_size_min + 1U));
                    error_code = terrain_place_ore_vein(chunk, seed_value,
                        world_block_origin_x + local_x, local_y,
                        world_block_origin_z + local_z, local_x, local_y,
                        local_z, config.ores[ore_index], vein_size, config);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
                vein_index += 1U;
            }
        }
        ore_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    const char *seed_string) noexcept
{
    return (terrain_generate_chunk(chunk, 0, 0, seed_string));
}

int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string) noexcept
{
    terrain_generation_config config;

    if (terrain_default_generation_config(config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (terrain_generate_chunk(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config));
}

static int32_t terrain_generate_chunk_snapshot(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string,
    const terrain_generation_config &requested_config,
    ft_bool configuration_validated, ft_bool signature_precomputed,
    uint32_t precomputed_signature, ft_bool coordinate_seed_overridden,
    uint64_t coordinate_seed, uint32_t requested_stage_mask) noexcept
{
    terrain_generation_config config;

    if (config.initialize(requested_config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    int32_t error_code;
    int32_t column_height;
    int32_t world_block_x;
    int32_t world_block_z;
    uint32_t biome;
    terrain_biome_profile biome_profile;
    uint32_t deep_block_id;
    uint64_t seed_value;
    uint32_t configuration_signature;
    ft_bool place_shrub;
    ft_bool mountain_active;
    const terrain_tree_template *tree_template;
    uint64_t tree_feature_seed;
    uint32_t feature_index;
    uint64_t feature_seed;
    const terrain_feature_rule *feature_rule;
    game_voxel_generation_metadata generation_metadata;
    terrain_column_cache column_cache[TERRAIN_COLUMN_CACHE_COUNT];
    int32_t column_index;
    int32_t feature_margin;
    uint32_t previous_stage_mask;
    const uint32_t all_stage_mask = TERRAIN_STAGE_BASE_TERRAIN
        | TERRAIN_STAGE_CAVES | TERRAIN_STAGE_FLUIDS
        | TERRAIN_STAGE_DECORATION | TERRAIN_STAGE_STRUCTURES
        | TERRAIN_STAGE_ORES;

    if (chunk.is_generation_protected() == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (requested_stage_mask == 0U || (requested_stage_mask & ~all_stage_mask) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (configuration_validated == FT_FALSE
        && terrain_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);

    seed_value = terrain_seed_value(seed_string);
    if (coordinate_seed_overridden == FT_TRUE)
        seed_value = terrain_mix_u64(seed_value ^ coordinate_seed);
    if (signature_precomputed == FT_TRUE)
        configuration_signature = precomputed_signature;
    else
        configuration_signature = terrain_generation_config_signature(config);
    previous_stage_mask = 0U;
    if (chunk.generation_metadata_matches(seed_value, world_block_origin_x,
            world_block_origin_z, configuration_signature) == FT_TRUE
        && chunk.get_generation_metadata().generator_version == TERRAIN_GENERATOR_VERSION)
    {
        previous_stage_mask = chunk.get_generation_metadata().completed_stage_mask;
        if ((previous_stage_mask & requested_stage_mask) == requested_stage_mask)
        {
            chunk.clear_dirty();
            return (FT_ERR_SUCCESS);
        }
    }
    if (terrain_stage_dependencies_are_met(requested_stage_mask,
            previous_stage_mask) == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    if ((requested_stage_mask & TERRAIN_STAGE_BASE_TERRAIN) != 0U)
    {
        error_code = terrain_stage_clear_chunk(chunk);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        previous_stage_mask = 0U;
    }
    terrain_stage_prepare_columns(seed_value, world_block_origin_x,
        world_block_origin_z, config, column_cache);
    /* Stage: base terrain, caves, terrain-aware layers, and fluids. */
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        world_block_z = world_block_origin_z + local_z;
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            world_block_x = world_block_origin_x + local_x;
            biome = column_cache[column_index].biome;
            biome_profile = column_cache[column_index].biome_profile;
            deep_block_id = column_cache[column_index].deep_block_id;
            place_shrub = column_cache[column_index].can_place_shrubs;
            column_height = column_cache[column_index].column_height;
            if (column_height < 0)
                column_height = 0;
            if (column_height >= GAME_VOXEL_CHUNK_HEIGHT)
                column_height = GAME_VOXEL_CHUNK_HEIGHT - 1;
            mountain_active = FT_FALSE;
            if (config.enable_mountain_ridges == FT_TRUE
                && column_cache[column_index].can_place_mountain_ridges == FT_TRUE
                && column_height > biome_profile.surface_height
                    + biome_profile.height_variation + 4)
                mountain_active = FT_TRUE;
            local_y = 0;
            while (local_y <= column_height)
            {
                if ((requested_stage_mask & TERRAIN_STAGE_CAVES) != 0U
                    && local_y > TERRAIN_BEDROCK_FLOOR_Y
                    && terrain_should_carve_cave(seed_value, world_block_x,
                        local_y, world_block_z, column_height, config)
                    == FT_TRUE)
                {
                    error_code = chunk.write_generated_block(local_x, local_y,
                        local_z, TERRAIN_GENERATOR_AIR_BLOCK);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y += 1;
                    continue ;
                }
                if ((requested_stage_mask & TERRAIN_STAGE_BASE_TERRAIN) == 0U)
                {
                    local_y += 1;
                    continue ;
                }
                if (local_y <= TERRAIN_BEDROCK_FLOOR_Y)
                    block_id = TERRAIN_GENERATOR_BEDROCK_BLOCK;
                else if (local_y == column_height)
                    block_id = terrain_sample_biome_block(config,
                        column_cache[column_index].biome_sample, seed_value,
                        world_block_x, local_y, world_block_z,
                        TERRAIN_BIOME_SURFACE_TRANSITION_SALT, FT_FALSE);
                else if (local_y >= column_height - biome_profile.topsoil_depth)
                    block_id = terrain_sample_biome_block(config,
                        column_cache[column_index].biome_sample, seed_value,
                        world_block_x, local_y, world_block_z,
                        TERRAIN_BIOME_SUBSURFACE_TRANSITION_SALT, FT_TRUE);
                else
                    block_id = deep_block_id;
                if (mountain_active == FT_TRUE
                    && column_cache[column_index].slope_height
                        >= TERRAIN_MOUNTAIN_CLIFF_SLOPE
                    && local_y >= column_height
                        - biome_profile.topsoil_depth)
                    block_id = deep_block_id;
                error_code = chunk.write_generated_block(local_x, local_y, local_z,
                    block_id);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                local_y += 1;
            }
            if ((requested_stage_mask & TERRAIN_STAGE_BASE_TERRAIN) != 0U
                && config.layers.enable_beaches == FT_TRUE
                && column_cache[column_index].has_surface_water == FT_TRUE
                && column_height < config.sea_level)
            {
                local_y = column_height;
                while (local_y >= 0 && local_y > column_height
                    - static_cast<int32_t>(config.layers.beach_depth))
                {
                    error_code = chunk.write_generated_block(local_x, local_y, local_z,
                        config.layers.beach_block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y -= 1;
                }
                local_y = column_height
                    - static_cast<int32_t>(config.layers.beach_depth);
                while (local_y >= 0 && local_y > column_height
                    - static_cast<int32_t>(config.layers.beach_depth)
                    - static_cast<int32_t>(config.layers.underwater_depth))
                {
                    error_code = chunk.write_generated_block(local_x, local_y, local_z,
                        config.layers.underwater_block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y -= 1;
                }
            }
            if ((requested_stage_mask & TERRAIN_STAGE_FLUIDS) != 0U
                && column_cache[column_index].has_surface_water == FT_TRUE
                && column_height < column_cache[column_index].surface_water_level)
            {
                uint32_t bed_block;
                if (chunk.read_block(local_x, column_height, local_z,
                        &bed_block) != FT_ERR_SUCCESS
                    || terrain_block_is_solid(bed_block) == FT_FALSE)
                {
                    local_x += 1;
                    continue ;
                }
                local_y = column_height + 1;
                while (local_y <= column_cache[column_index].surface_water_level
                    && local_y < GAME_VOXEL_CHUNK_HEIGHT)
                {
                    uint32_t existing_block;
                    if (chunk.read_block(local_x, local_y, local_z,
                            &existing_block) != FT_ERR_SUCCESS
                        || (existing_block != TERRAIN_GENERATOR_AIR_BLOCK
                            && terrain_block_is_liquid(existing_block) == FT_FALSE))
                        break ;
                    block_id = TERRAIN_GENERATOR_WATER_BLOCK;
                    if (local_y == config.sea_level
                        && biome == TERRAIN_BIOME_SNOW)
                        block_id = TERRAIN_GENERATOR_ICE_BLOCK;
                    error_code = chunk.write_generated_block(local_x, local_y,
                        local_z, block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y += 1;
                }
            }
            if ((requested_stage_mask & TERRAIN_STAGE_DECORATION) != 0U
                && config.layers.enable_snow_caps == FT_TRUE
                && column_cache[column_index].can_place_snow == FT_TRUE
                && column_height >= config.layers.snow_cap_minimum_height
                && (mountain_active == FT_FALSE
                    || (column_cache[column_index].slope_height
                        <= TERRAIN_MOUNTAIN_SNOW_SLOPE_LIMIT
                        && terrain_value_noise(seed_value ^ UINT64_C(
                            0xD1B54A32D192ED03), world_block_x,
                            world_block_z, config.detail_noise_scale) > -0.35)))
            {
                local_y = column_height;
                while (local_y >= 0 && local_y > column_height
                    - static_cast<int32_t>(config.layers.snow_cap_depth))
                {
                    error_code = chunk.write_generated_block(local_x, local_y, local_z,
                        config.layers.snow_cap_block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y -= 1;
                }
            }
            local_x += 1;
        }
        local_z += 1;
    }
    /* Stage: underground fluids. Surface fluids were planned from the final
     * heightfield and committed with each terrain column above. */
    if ((requested_stage_mask & TERRAIN_STAGE_FLUIDS) != 0U
        && config.fluids.enable_underground_lakes == FT_TRUE
        && config.fluids.underground_lake_chance_percent > 0U)
    {
        /* Fill only air pockets with a solid floor and roof: small enclosed
         * underground lakes, rather than exposed water hanging in terrain. */
        local_z = 2;
        while (local_z + 2 < GAME_VOXEL_CHUNK_DEPTH)
        {
            local_x = 2;
            while (local_x + 2 < GAME_VOXEL_CHUNK_WIDTH)
            {
                column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
                column_height = column_cache[column_index].column_height;
        local_y = config.fluids.underground_lake_minimum_y;
        while (local_y + 3 < column_height
            && local_y <= config.fluids.underground_lake_maximum_y
            && local_y + 3 < GAME_VOXEL_CHUNK_HEIGHT)
                {
                    if (local_y % (static_cast<int32_t>(config.fluids
                            .underground_lake_depth) + 7) == 4
                        && terrain_world_coordinate_on_grid(
                            world_block_origin_x + local_x, 4, 2) == FT_TRUE
                        && terrain_world_coordinate_on_grid(
                            world_block_origin_z + local_z, 4, 2) == FT_TRUE
                        && terrain_should_place_feature(seed_value
                            ^ static_cast<uint64_t>(local_y),
                            world_block_origin_x + local_x,
                            world_block_origin_z + local_z,
                            TERRAIN_FEATURE_WATER_SALT
                                ^ UINT64_C(0x6A09E667F3BCC909),
                            config.fluids.underground_lake_chance_percent) == FT_TRUE
                    )
                    {
                        const ft_bool can_create_lake =
                            terrain_can_create_underground_lake_site(chunk,
                            local_x, local_y, local_z, world_block_origin_x,
                            world_block_origin_z, config);
                        const ft_bool is_enclosed_lake =
                            terrain_is_enclosed_underground_lake_site(chunk,
                            local_x, local_y, local_z, world_block_origin_x,
                            world_block_origin_z, config);
                        if (can_create_lake == FT_FALSE
                            && is_enclosed_lake == FT_FALSE)
                        {
                            local_y += 1;
                            continue ;
                        }
                        if (can_create_lake == FT_TRUE)
                        {
                            int32_t clear_z = -1;
                            while (clear_z <= 1)
                            {
                                int32_t clear_x = -1;
                                while (clear_x <= 1)
                                {
                                    error_code = terrain_write_generation_block(
                                        chunk, local_x + clear_x, local_y
                                            + static_cast<int32_t>(config.fluids
                                                .underground_lake_depth),
                                        local_z + clear_z, world_block_origin_x,
                                        world_block_origin_z, config,
                                        TERRAIN_GENERATOR_AIR_BLOCK);
                                    if (error_code != FT_ERR_SUCCESS)
                                        return (error_code);
                                    clear_x += 1;
                                }
                                clear_z += 1;
                            }
                        }
                        int32_t fill_depth = 0;
                        while (fill_depth < static_cast<int32_t>(config.fluids
                                .underground_lake_depth))
                        {
                            int32_t fill_z = -1;
                            while (fill_z <= 1)
                            {
                                int32_t fill_x = -1;
                                while (fill_x <= 1)
                                {
                                    error_code = terrain_write_generation_block(
                                        chunk, local_x + fill_x,
                                        local_y + fill_depth, local_z + fill_z,
                                        world_block_origin_x,
                                        world_block_origin_z, config,
                                        TERRAIN_GENERATOR_WATER_BLOCK);
                                    if (error_code != FT_ERR_SUCCESS)
                                        return (error_code);
                                    fill_x += 1;
                                }
                                fill_z += 1;
                            }
                            fill_depth += 1;
                        }
                        local_y += static_cast<int32_t>(config.fluids
                            .underground_lake_depth) + 1;
                    }
                    local_y += 1;
                }
                local_x += 1;
            }
            local_z += 1;
        }
    }
    /* Stage: biome decorations and configured structures. */
    feature_margin = 2;
    if (config.allow_cross_chunk_features == FT_TRUE
        && config.cross_chunk_block_writer != ft_nullptr)
        feature_margin = 0;
    local_z = feature_margin;
    while (local_z + feature_margin < GAME_VOXEL_CHUNK_DEPTH)
    {
        world_block_z = world_block_origin_z + local_z;
        local_x = feature_margin;
        while (local_x + feature_margin < GAME_VOXEL_CHUNK_WIDTH)
        {
            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            world_block_x = world_block_origin_x + local_x;
            biome = column_cache[column_index].biome;
            column_height = column_cache[column_index].column_height;
            place_shrub = column_cache[column_index].can_place_shrubs;
            if ((requested_stage_mask & TERRAIN_STAGE_DECORATION) != 0U
                && column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t water_level =
                    column_cache[column_index].surface_water_level;
                uint32_t above_water_block;
                if (water_level + 1 < GAME_VOXEL_CHUNK_HEIGHT
                    && chunk.read_block(local_x, water_level + 1, local_z,
                        &above_water_block) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_OPERATION);
                if ((biome == TERRAIN_BIOME_PLAINS
                        || biome == TERRAIN_BIOME_HILLS)
                    && water_level + 1 < GAME_VOXEL_CHUNK_HEIGHT
                    && terrain_should_place_feature(seed_value,
                        world_block_x, world_block_z,
                        TERRAIN_FEATURE_AQUATIC_PLANT_SALT, 10U) == FT_TRUE
                    && above_water_block == TERRAIN_GENERATOR_AIR_BLOCK)
                {
                    error_code = chunk.write_generated_block(local_x,
                        water_level + 1, local_z,
                        TERRAIN_GENERATOR_LILY_PAD_BLOCK);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
                else if (column_height + 1 < water_level
                    && terrain_should_place_feature(seed_value,
                        world_block_x, world_block_z,
                        TERRAIN_FEATURE_AQUATIC_PLANT_SALT, 15U) == FT_TRUE)
                {
                    uint32_t submerged_block;
                    if (chunk.read_block(local_x, column_height + 1,
                            local_z, &submerged_block) != FT_ERR_SUCCESS)
                        return (FT_ERR_INVALID_OPERATION);
                    if (terrain_block_is_liquid(submerged_block) == FT_TRUE)
                    {
                        error_code = chunk.write_generated_block(local_x,
                            column_height + 1, local_z,
                            TERRAIN_GENERATOR_SEAGRASS_BLOCK);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            if ((requested_stage_mask & TERRAIN_STAGE_DECORATION) != 0U
                && place_shrub == FT_TRUE
                && column_cache[column_index].has_surface_water == FT_FALSE
                && column_height + TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET
                    < GAME_VOXEL_CHUNK_HEIGHT
                && terrain_should_place_feature(seed_value, world_block_x,
                    world_block_z, TERRAIN_FEATURE_SHRUB_SALT,
                    column_cache[column_index].shrub_chance_percent) == FT_TRUE)
            {
                uint32_t above_surface_block;
                if (chunk.read_block(local_x, column_height + 1, local_z,
                        &above_surface_block) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_OPERATION);
                if (terrain_block_is_replaceable(above_surface_block) == FT_TRUE)
                {
                    error_code = chunk.write_generated_block(local_x,
                        column_height + TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET,
                        local_z, terrain_ground_cover_block_for_biome(biome,
                            seed_value, world_block_x, world_block_z));
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
            }
            if ((requested_stage_mask & TERRAIN_STAGE_DECORATION) != 0U
                && column_cache[column_index].can_place_trees == FT_TRUE)
            {
                if (column_cache[column_index].has_surface_water == FT_TRUE)
                {
                    local_x += 4;
                    continue ;
                }
                tree_feature_seed = terrain_feature_seed(seed_value,
                    world_block_x, world_block_z, TERRAIN_FEATURE_TREE_SALT);
                if ((tree_feature_seed % 100U)
                    < column_cache[column_index].tree_chance_percent)
                {
                    tree_template = terrain_sample_tree_template(config,
                        column_cache[column_index].biome_sample,
                        tree_feature_seed);
                    column_height = column_cache[column_index].column_height;
                    if (tree_template != ft_nullptr
                        && terrain_can_place_tree_with_writer(chunk, local_x,
                            column_height + 1, local_z, *tree_template,
                            config) == FT_TRUE)
                    {
                        error_code = terrain_place_tree_with_writer(chunk,
                            local_x, column_height + 1, local_z,
                            world_block_origin_x, world_block_origin_z,
                            *tree_template, config);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            local_x += 4;
        }
        local_z += 4;
    }
    feature_index = 0U;
    while (feature_index < config.feature_count
        && feature_index < TERRAIN_MAX_FEATURE_RULES)
    {
        feature_rule = &config.features[feature_index];
        if ((requested_stage_mask & TERRAIN_STAGE_STRUCTURES) != 0U
            && feature_rule->template_data != ft_nullptr)
        {
            local_z = feature_margin;
            while (local_z + feature_margin < GAME_VOXEL_CHUNK_DEPTH)
            {
                local_x = feature_margin;
                while (local_x + feature_margin < GAME_VOXEL_CHUNK_WIDTH)
                {
                    column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
                    biome = column_cache[column_index].biome;
                    column_height = column_cache[column_index].column_height;
                    if ((feature_rule->biome_index < 0
                            || feature_rule->biome_index
                                == static_cast<int32_t>(biome))
                        && column_height >= feature_rule->minimum_height
                        && column_height <= feature_rule->maximum_height
                        && (feature_rule->requires_dry_land == FT_FALSE
                            || (column_cache[column_index].has_surface_water
                                == FT_FALSE)))
                    {
                        feature_seed = terrain_feature_seed(seed_value,
                            world_block_origin_x + local_x,
                            world_block_origin_z + local_z,
                            TERRAIN_FEATURE_TREE_SALT
                                ^ static_cast<uint64_t>(feature_index + 1U));
                        if ((feature_seed % 100U)
                            < feature_rule->chance_percent
                            && terrain_can_place_tree_with_writer(chunk,
                                local_x, column_height + 1, local_z,
                                *feature_rule->template_data, config)
                                == FT_TRUE)
                        {
                            error_code = terrain_place_tree_with_writer(chunk,
                                local_x, column_height + 1, local_z,
                                world_block_origin_x, world_block_origin_z,
                                *feature_rule->template_data, config);
                            if (error_code != FT_ERR_SUCCESS)
                                return (error_code);
                        }
                    }
                    local_x += 4;
                }
                local_z += 4;
            }
        }
        feature_index += 1U;
    }
    /* Stage: configured underground ore deposits. */
    if ((requested_stage_mask & TERRAIN_STAGE_ORES) != 0U)
    {
        error_code = terrain_generate_ores(chunk, seed_value,
            world_block_origin_x, world_block_origin_z, config);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    chunk.clear_dirty();
    generation_metadata.seed_value = seed_value;
    generation_metadata.world_block_origin_x = world_block_origin_x;
    generation_metadata.world_block_origin_z = world_block_origin_z;
    generation_metadata.configuration_signature = configuration_signature;
    generation_metadata.completed_stage_mask = previous_stage_mask
        | requested_stage_mask;
    generation_metadata.generator_version = TERRAIN_GENERATOR_VERSION;
    generation_metadata.valid = FT_TRUE;
    error_code = chunk.set_generation_metadata(generation_metadata);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_config &config) noexcept
{
    return (terrain_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config, FT_FALSE, FT_FALSE, 0U,
        FT_FALSE, 0U, TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES
        | TERRAIN_STAGE_FLUIDS | TERRAIN_STAGE_DECORATION
        | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES));
}

int32_t terrain_generate_chunk_with_stage_mask(
    game_voxel_chunk &chunk, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const terrain_generation_config &config, uint32_t stage_mask) noexcept
{
    return (terrain_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config, FT_FALSE, FT_FALSE, 0U,
        FT_FALSE, 0U, stage_mask));
}

int32_t terrain_generate_chunk_with_context(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_context &context) noexcept
{
    if (context.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (terrain_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, context.config(), FT_TRUE, FT_TRUE,
        context.configuration_signature(), FT_FALSE, 0U,
        TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES | TERRAIN_STAGE_FLUIDS
        | TERRAIN_STAGE_DECORATION | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES));
}

int32_t terrain_generate_chunk_at_world_coordinate(game_voxel_chunk &chunk,
    const terrain_world_chunk_coordinate &coordinate,
    const char *seed_string, const terrain_generation_config &config) noexcept
{
    if (coordinate.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (terrain_generate_chunk_snapshot(chunk, 0, 0, seed_string, config,
        FT_FALSE, FT_FALSE, 0U, FT_TRUE, coordinate.hash(),
        TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES | TERRAIN_STAGE_FLUIDS
        | TERRAIN_STAGE_DECORATION | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES));
}

int32_t terrain_generate_chunk_in_region(game_voxel_region &region,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_config &config) noexcept
{
    terrain_generation_context context;

    if (terrain_generation_context_initialize(context, config)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (terrain_generate_chunk_in_region_with_context(region,
        world_block_origin_x, world_block_origin_z, seed_string, context));
}

int32_t terrain_generate_chunk_in_region_with_context(
    game_voxel_region &region, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const terrain_generation_context &context) noexcept
{
    terrain_generation_config region_config;
    game_voxel_chunk *chunk;
    int32_t error_code;

    if (context.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    error_code = region.load_chunk(terrain_floor_div(world_block_origin_x,
            GAME_VOXEL_CHUNK_WIDTH), terrain_floor_div(world_block_origin_z,
            GAME_VOXEL_CHUNK_DEPTH), &chunk);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (region_config.initialize(context.config()) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (region_config.set_cross_chunk_features_enabled(FT_TRUE)
            != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (region_config.set_cross_chunk_writer(
            &terrain_region_cross_chunk_block_writer, &region)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (region_config.set_cross_chunk_reader(
            &terrain_region_cross_chunk_block_reader, &region)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (terrain_generate_chunk_snapshot(*chunk, world_block_origin_x,
        world_block_origin_z, seed_string, region_config, FT_TRUE, FT_FALSE,
        0U, FT_FALSE, 0U, TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES
        | TERRAIN_STAGE_FLUIDS | TERRAIN_STAGE_DECORATION
        | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES));
}

#endif

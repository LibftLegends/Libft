#include <stdint.h>
#include <algorithm>
#include <cmath>
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../Game/game_voxel_region.hpp"

static const int32_t VOXEL_HEIGHTMAP_LARGE_SCALE = 32;
static const int32_t VOXEL_HEIGHTMAP_DETAIL_SCALE = 8;
static const int32_t VOXEL_HEIGHTMAP_SMOOTH_RADIUS = 1;
static const int32_t VOXEL_MOUNTAIN_CLIFF_SLOPE = 7;
static const int32_t VOXEL_MOUNTAIN_SNOW_SLOPE_LIMIT = 5;
static const uint64_t VOXEL_FEATURE_SHRUB_SALT = UINT64_C(0x2D9C1F4E8B3A6071);
static const int32_t VOXEL_FEATURE_SHRUB_HEIGHT_OFFSET = 1;
static const uint64_t VOXEL_FEATURE_GROUND_COVER_VARIANT_SALT =
    UINT64_C(0x1A2B3C4D5E6F7081);
static const uint64_t VOXEL_FEATURE_TREE_SALT = UINT64_C(0x4F1E2D3C5B6A7980);
static const uint64_t VOXEL_FEATURE_WATER_SALT = UINT64_C(0x9182736455463728);
static const uint64_t VOXEL_FEATURE_AQUATIC_PLANT_SALT =
    UINT64_C(0x2F4E6D8C0A1B3547);
static const uint64_t VOXEL_FEATURE_PAPER_REED_SALT =
    UINT64_C(0xC4A57B193E2608DF);
static const uint64_t VOXEL_BIOME_SURFACE_TRANSITION_SALT = UINT64_C(0xBB67AE8584CAA73B);
static const uint64_t VOXEL_BIOME_SUBSURFACE_TRANSITION_SALT = UINT64_C(0x3C6EF372FE94F82B);
static const uint64_t VOXEL_CAVE_PRIMARY_SALT = UINT64_C(0x7C3A91E2D4B8560F);
static const uint64_t VOXEL_CAVE_DETAIL_SALT = UINT64_C(0x1D6F80B3C9274A55);
static const int32_t VOXEL_CAVE_PRIMARY_SCALE = 24;
static const int32_t VOXEL_CAVE_DETAIL_SCALE = 9;
static const int32_t VOXEL_BEDROCK_FLOOR_Y = 0;
static const int32_t VOXEL_CAVE_SURFACE_MARGIN = 7;
static const int32_t VOXEL_CAVE_CELL_SIZE = 12;
static const int32_t VOXEL_COLUMN_CACHE_COUNT =
    GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH;
static constexpr int32_t VOXEL_HEIGHT_SAMPLE_WIDTH = GAME_VOXEL_CHUNK_WIDTH + 2;
static constexpr int32_t VOXEL_HEIGHT_SAMPLE_DEPTH = GAME_VOXEL_CHUNK_DEPTH + 2;
static const uint8_t VOXEL_SURFACE_WATER_NONE = 0U;
static const uint8_t VOXEL_SURFACE_WATER_RIVER = 1U;
static const uint8_t VOXEL_SURFACE_WATER_LAKE = 2U;
static const uint8_t VOXEL_SURFACE_WATER_LEGACY = 3U;

static int32_t voxel_floor_division(int32_t value, int32_t divisor) noexcept
{
    if (divisor <= 0)
        return (0);
    return (voxel_floor_div(value, divisor));
}

struct voxel_biome_sample
{
    uint32_t biome_indices[4];
    double weights[4];
    uint32_t count;
};

struct voxel_column_cache
{
    uint32_t biome;
    voxel_biome_sample biome_sample;
    voxel_biome_profile biome_profile;
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

struct voxel_cross_chunk_callbacks
{
    ft_bool features_enabled;
    voxel_cross_chunk_block_writer writer;
    void *writer_data;
    voxel_cross_chunk_block_reader reader;
    void *reader_data;
};

static voxel_cross_chunk_callbacks voxel_get_cross_chunk_callbacks(
    const voxel_generation_config &config) noexcept
{
    return (voxel_cross_chunk_callbacks{
        config.allow_cross_chunk_features,
        config.cross_chunk_block_writer, config.cross_chunk_block_writer_user_data,
        config.cross_chunk_block_reader, config.cross_chunk_block_reader_user_data
    });
}

static int32_t voxel_sample_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept;

static void voxel_sample_biomes(const voxel_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z,
    voxel_biome_sample *sample) noexcept;

static void voxel_prepare_height_samples(uint64_t seed_value,
    int32_t origin_x, int32_t origin_z, const voxel_generation_config &config,
    voxel_column_cache *columns, int32_t *heights) noexcept;

static int32_t voxel_weighted_height(const int32_t *center) noexcept
{
    constexpr int32_t STRIDE = VOXEL_HEIGHT_SAMPLE_WIDTH;
    const int32_t north = center[-STRIDE - 1] + 2 * center[-STRIDE]
        + center[-STRIDE + 1];
    const int32_t middle = 2 * (center[-1] + 2 * center[0] + center[1]);
    const int32_t south = center[STRIDE - 1] + 2 * center[STRIDE]
        + center[STRIDE + 1];
    return ((north + middle + south) / 16);
}

static int32_t voxel_estimate_slope(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept;

static int32_t voxel_smooth_heightfield(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept;

static uint8_t voxel_surface_water_kind(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept;

static int32_t voxel_surface_water_level(uint8_t water_kind,
    int32_t sea_level) noexcept;

static uint32_t voxel_surface_water_max_depth(uint8_t water_kind,
    const voxel_generation_config &config) noexcept;

static ft_bool voxel_surface_water_candidate_is_valid(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config, uint8_t *water_kind,
    uint64_t *feature_id) noexcept;

static ft_bool voxel_surface_water_neighbor_matches(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    int32_t local_x, int32_t local_z, int32_t offset_x, int32_t offset_z,
    const voxel_generation_config &config, voxel_column_cache *column_cache,
    uint8_t expected_kind, uint64_t expected_feature_id) noexcept;

static ft_bool voxel_stage_dependencies_are_met(uint32_t requested_mask,
    uint32_t previous_mask) noexcept;

static int32_t voxel_read_generation_block(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const voxel_cross_chunk_callbacks &callbacks, uint32_t *block_id) noexcept
{
    if (local_x >= 0 && local_x < GAME_VOXEL_CHUNK_WIDTH
        && local_y >= 0 && local_y < GAME_VOXEL_CHUNK_HEIGHT
        && local_z >= 0 && local_z < GAME_VOXEL_CHUNK_DEPTH)
        return (chunk.read_block(local_x, local_y, local_z, block_id));
    if (callbacks.reader == ft_nullptr)
        return (FT_ERR_OUT_OF_RANGE);
    return (callbacks.reader(world_block_origin_x + local_x,
        local_y, world_block_origin_z + local_z, block_id,
        callbacks.reader_data));
}

static ft_bool voxel_underground_lake_vertical_bounds_are_valid(
    int32_t local_y, const voxel_generation_config &config) noexcept
{
    int32_t water_end;
    int32_t roof_end;

    if (local_y < 0 || config.fluids.underground_lake_depth == 0U)
        return (FT_FALSE);
    water_end = local_y + static_cast<int32_t>(config.fluids
        .underground_lake_depth) - 1;
    roof_end = local_y + static_cast<int32_t>(config.fluids
        .underground_lake_depth)
        + static_cast<int32_t>(config.fluids.underground_lake_roof_thickness);
    if (water_end > config.fluids.underground_lake_maximum_y
        || water_end >= GAME_VOXEL_CHUNK_HEIGHT
        || local_y + static_cast<int32_t>(config.fluids.underground_lake_depth)
            >= GAME_VOXEL_CHUNK_HEIGHT
        || roof_end >= GAME_VOXEL_CHUNK_HEIGHT)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t voxel_write_generation_block(game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const voxel_cross_chunk_callbacks &callbacks, uint32_t block_id) noexcept
{
    if (local_x >= 0 && local_x < GAME_VOXEL_CHUNK_WIDTH
        && local_y >= 0 && local_y < GAME_VOXEL_CHUNK_HEIGHT
        && local_z >= 0 && local_z < GAME_VOXEL_CHUNK_DEPTH)
        return (chunk.write_generated_block(local_x, local_y, local_z,
            block_id));
    if (callbacks.writer == ft_nullptr)
        return (FT_ERR_OUT_OF_RANGE);
    return (callbacks.writer(world_block_origin_x + local_x,
        local_y, world_block_origin_z + local_z, block_id,
        callbacks.writer_data));
}

static ft_bool voxel_underground_lake_geometry_is_valid(
    game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const voxel_generation_config &config,
    const voxel_cross_chunk_callbacks &callbacks) noexcept
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
                if (voxel_read_generation_block(chunk, local_x + x,
                        local_y - 1 - offset, local_z + z,
                        world_block_origin_x, world_block_origin_z, callbacks,
                        &block_id) != FT_ERR_SUCCESS
                    || voxel_block_is_solid(block_id) == FT_FALSE)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        offset += 1;
    }
    if (voxel_underground_lake_vertical_bounds_are_valid(local_y, config)
        == FT_FALSE)
        return (FT_FALSE);
    level = 0;
    while (level < static_cast<int32_t>(config.fluids
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
                    if (voxel_read_generation_block(chunk, local_x + x,
                            local_y + level, local_z + z,
                            world_block_origin_x, world_block_origin_z,
                            callbacks, &block_id) != FT_ERR_SUCCESS
                        || voxel_block_is_solid(block_id) == FT_FALSE)
                        return (FT_FALSE);
                }
                x += 1;
            }
            z += 1;
        }
        level += 1;
    }
    level = 0;
    while (level < static_cast<int32_t>(config.fluids
            .underground_lake_depth))
    {
        int32_t z = -1;
        while (z <= 1)
        {
            int32_t x = -1;
            while (x <= 1)
            {
                if (voxel_read_generation_block(chunk, local_x + x,
                        local_y + level, local_z + z, world_block_origin_x,
                        world_block_origin_z, callbacks, &block_id)
                        != FT_ERR_SUCCESS
                    || block_id != VOXEL_GENERATOR_AIR_BLOCK)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        level += 1;
    }
    {
        int32_t z = -1;
        while (z <= 1)
        {
            int32_t x = -1;
            while (x <= 1)
            {
                if (voxel_read_generation_block(chunk, local_x + x,
                        local_y + static_cast<int32_t>(config.fluids
                            .underground_lake_depth), local_z + z,
                        world_block_origin_x, world_block_origin_z, callbacks,
                        &block_id) != FT_ERR_SUCCESS
                    || block_id != VOXEL_GENERATOR_AIR_BLOCK)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
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
                if (voxel_read_generation_block(chunk, local_x + x,
                        local_y + static_cast<int32_t>(config.fluids
                            .underground_lake_depth) + offset, local_z + z,
                        world_block_origin_x, world_block_origin_z, callbacks,
                        &block_id) != FT_ERR_SUCCESS
                    || voxel_block_is_solid(block_id) == FT_FALSE)
                    return (FT_FALSE);
                x += 1;
            }
            z += 1;
        }
        offset += 1;
    }
    return (FT_TRUE);
}

static void voxel_stage_prepare_columns(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const voxel_generation_config &config,
    voxel_column_cache *column_cache) noexcept
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
    int32_t heights[VOXEL_HEIGHT_SAMPLE_WIDTH * VOXEL_HEIGHT_SAMPLE_DEPTH];
    // A custom selector can observe call order through its user data.
    const ft_bool cache_heights = config.biome_selector == ft_nullptr;
    if (cache_heights != FT_FALSE)
        voxel_prepare_height_samples(seed_value, world_block_origin_x, world_block_origin_z,
            config, column_cache, heights);

    /* Surface water is sampled per column.  The second pass removes detached
     * candidates, including candidates on a chunk border.  Border columns
     * are checked against deterministic world-coordinate samples so a pond
     * can cross a chunk boundary without creating a floating one-column
     * fragment when the neighboring chunk is generated later. */

    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        world_block_z = world_block_origin_z + local_z;
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            world_block_x = world_block_origin_x + local_x;
            if (cache_heights == FT_FALSE)
                voxel_sample_biomes(config, seed_value, world_block_x,
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
            if (cache_heights != FT_FALSE)
            {
                const int32_t *center = heights
                    + (local_z + 1) * VOXEL_HEIGHT_SAMPLE_WIDTH + local_x + 1;
                column_cache[column_index].column_height = voxel_weighted_height(center);
                column_cache[column_index].slope_height = std::abs(center[1] - center[-1])
                    + std::abs(center[VOXEL_HEIGHT_SAMPLE_WIDTH] - center[-VOXEL_HEIGHT_SAMPLE_WIDTH]);
            }
            else
                column_cache[column_index].column_height = voxel_smooth_heightfield(
                    seed_value, world_block_x, world_block_z, config);
            column_cache[column_index].surface_water_kind =
                voxel_surface_water_kind(seed_value, world_block_x,
                    world_block_z, config);
            column_cache[column_index].has_surface_water =
                column_cache[column_index].surface_water_kind
                    != VOXEL_SURFACE_WATER_NONE ? FT_TRUE : FT_FALSE;
            column_cache[column_index].surface_water_feature_id = 0U;
            column_cache[column_index].surface_water_bank_distance = 0U;
            column_cache[column_index].surface_water_level =
                voxel_surface_water_level(column_cache[column_index]
                    .surface_water_kind, config.sea_level);
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t natural_height =
                    column_cache[column_index].column_height;
                const int32_t water_depth =
                    column_cache[column_index].surface_water_level
                    - natural_height;
                if (natural_height >= column_cache[column_index]
                        .surface_water_level
                    || water_depth <= 0
                    || static_cast<uint32_t>(water_depth)
                        > voxel_surface_water_max_depth(
                            column_cache[column_index].surface_water_kind,
                            config))
                    column_cache[column_index].has_surface_water = FT_FALSE;
            }
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t feature_cell_size =
                    column_cache[column_index].surface_water_kind
                        == VOXEL_SURFACE_WATER_RIVER
                        ? config.fluids.river_noise_scale
                        : config.fluids.lake_noise_scale;
                const int32_t feature_cell_x = feature_cell_size > 0
                    ? voxel_floor_division(world_block_x,
                        feature_cell_size) * feature_cell_size
                    : world_block_x;
                const int32_t feature_cell_z = feature_cell_size > 0
                    ? voxel_floor_division(world_block_z,
                        feature_cell_size) * feature_cell_size
                    : world_block_z;
                column_cache[column_index].surface_water_feature_id =
                    voxel_feature_seed(seed_value, feature_cell_x,
                        feature_cell_z, VOXEL_FEATURE_WATER_SALT
                            ^ static_cast<uint64_t>(column_cache[column_index]
                                .surface_water_kind));
                column_cache[column_index].surface_water_depth =
                    static_cast<uint32_t>(column_cache[column_index]
                        .surface_water_level
                        - column_cache[column_index].column_height);
                column_cache[column_index].surface_water_bank_distance =
                    column_cache[column_index].surface_water_kind
                        == VOXEL_SURFACE_WATER_RIVER ? 1U : 2U;
            }
            else
            {
                column_cache[column_index].surface_water_depth = 0U;
            }
            if (cache_heights == FT_FALSE)
                column_cache[column_index].slope_height = voxel_estimate_slope(
                    seed_value, world_block_x, world_block_z, config);
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
    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            ft_bool has_compatible_neighbor;

            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            has_compatible_neighbor = FT_FALSE;
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                if (voxel_surface_water_neighbor_matches(seed_value,
                        world_block_origin_x, world_block_origin_z, local_x,
                        local_z, -1, 0, config, column_cache,
                        column_cache[column_index].surface_water_kind,
                        column_cache[column_index].surface_water_feature_id)
                        == FT_TRUE)
                    has_compatible_neighbor = FT_TRUE;
                if (has_compatible_neighbor == FT_FALSE
                    && voxel_surface_water_neighbor_matches(seed_value,
                        world_block_origin_x, world_block_origin_z, local_x,
                        local_z, 1, 0, config, column_cache,
                        column_cache[column_index].surface_water_kind,
                        column_cache[column_index].surface_water_feature_id)
                        == FT_TRUE)
                    has_compatible_neighbor = FT_TRUE;
                if (has_compatible_neighbor == FT_FALSE
                    && voxel_surface_water_neighbor_matches(seed_value,
                        world_block_origin_x, world_block_origin_z, local_x,
                        local_z, 0, -1, config, column_cache,
                        column_cache[column_index].surface_water_kind,
                        column_cache[column_index].surface_water_feature_id)
                        == FT_TRUE)
                    has_compatible_neighbor = FT_TRUE;
                if (has_compatible_neighbor == FT_FALSE
                    && voxel_surface_water_neighbor_matches(seed_value,
                        world_block_origin_x, world_block_origin_z, local_x,
                        local_z, 0, 1, config, column_cache,
                        column_cache[column_index].surface_water_kind,
                        column_cache[column_index].surface_water_feature_id)
                        == FT_TRUE)
                    has_compatible_neighbor = FT_TRUE;
                if (has_compatible_neighbor == FT_FALSE)
                {
                    column_cache[column_index].has_surface_water = FT_FALSE;
                    column_cache[column_index].surface_water_depth = 0U;
                    column_cache[column_index].surface_water_feature_id = 0U;
                }
            }
            local_x += 1;
        }
        local_z += 1;
    }
    return ;
}

static ft_bool voxel_can_place_tree_with_writer(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const voxel_tree_template &tree_template,
    const voxel_cross_chunk_callbacks &callbacks) noexcept
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
            if (callbacks.features_enabled == FT_FALSE
                || callbacks.writer == ft_nullptr)
                return (FT_FALSE);
        }
        else
        {
            if (chunk.read_block(target_x, target_y, target_z, &block_id)
                != FT_ERR_SUCCESS
                || voxel_block_is_replaceable(block_id) == FT_FALSE)
                return (FT_FALSE);
        }
        block_index += 1U;
    }
    return (FT_TRUE);
}

static int32_t voxel_place_tree_with_writer(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const voxel_tree_template &tree_template,
    const voxel_cross_chunk_callbacks &callbacks) noexcept
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
            error_code = callbacks.writer(
                world_block_origin_x + target_x, target_y,
                world_block_origin_z + target_z,
                tree_template.blocks[block_index].block_id,
                callbacks.writer_data);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        block_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_apply_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const voxel_tree_template &tree_template,
    const voxel_cross_chunk_callbacks &callbacks) noexcept
{
    constexpr uint32_t LOCAL_BATCH_CAPACITY = 64U;
    game_voxel_generated_block blocks[LOCAL_BATCH_CAPACITY];

    if (tree_template.blocks == ft_nullptr || tree_template.block_count == 0U)
        return (FT_ERR_SUCCESS);
    if (tree_template.block_count <= LOCAL_BATCH_CAPACITY)
    {
        uint32_t index = 0U;
        while (index < tree_template.block_count)
        {
            const voxel_tree_template_block &block = tree_template.blocks[index];
            const int64_t target_x = static_cast<int64_t>(local_origin_x) + block.offset_x;
            const int64_t target_y = static_cast<int64_t>(local_origin_y) + block.offset_y;
            const int64_t target_z = static_cast<int64_t>(local_origin_z) + block.offset_z;
            if (target_x < 0 || target_x >= GAME_VOXEL_CHUNK_WIDTH
                || target_y < 0 || target_y >= GAME_VOXEL_CHUNK_HEIGHT
                || target_z < 0 || target_z >= GAME_VOXEL_CHUNK_DEPTH)
            {
                if (callbacks.features_enabled == FT_FALSE
                    || callbacks.writer == ft_nullptr)
                    return (FT_ERR_SUCCESS);
                break ;
            }
            blocks[index] = {static_cast<int32_t>(target_x),
                static_cast<int32_t>(target_y), static_cast<int32_t>(target_z),
                block.block_id};
            index += 1U;
        }
        if (index == tree_template.block_count)
            return (chunk.write_generated_blocks_if(blocks, index,
                voxel_block_is_replaceable));
    }
    if (voxel_can_place_tree_with_writer(chunk, local_origin_x, local_origin_y,
            local_origin_z, tree_template, callbacks) == FT_FALSE)
        return (FT_ERR_SUCCESS);
    return (voxel_place_tree_with_writer(chunk, local_origin_x, local_origin_y,
        local_origin_z, world_block_origin_x, world_block_origin_z,
        tree_template, callbacks));
}

static int32_t voxel_region_cross_chunk_block_writer(int32_t world_block_x,
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

static int32_t voxel_region_cross_chunk_block_reader(int32_t world_block_x,
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

static int32_t voxel_column_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_biome_profile &biome_profile,
    ft_bool allow_mountain_ridges,
    const voxel_generation_config &config) noexcept
{
    double large_noise;
    double detail_noise;
    double total_noise;
    int32_t surface_height;
    int32_t variation;

    large_noise = voxel_value_noise(seed_value, world_block_x, world_block_z,
        config.large_noise_scale > 0 ? config.large_noise_scale : VOXEL_HEIGHTMAP_LARGE_SCALE);
    detail_noise = voxel_value_noise(seed_value ^ UINT64_C(0xA5A5A5A5A5A5A5A5),
        world_block_x, world_block_z,
        config.detail_noise_scale > 0 ? config.detail_noise_scale : VOXEL_HEIGHTMAP_DETAIL_SCALE);
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
        warp_offset_x = static_cast<int32_t>(voxel_value_noise(
            seed_value ^ UINT64_C(0x243F6A8885A308D3), world_block_x,
            world_block_z, warp_scale) * static_cast<double>(warp_scale * 2));
        warp_offset_z = static_cast<int32_t>(voxel_value_noise(
            seed_value ^ UINT64_C(0x13198A2E03707344), world_block_x,
            world_block_z, warp_scale) * static_cast<double>(warp_scale * 2));
        warped_x = world_block_x + warp_offset_x;
        warped_z = world_block_z + warp_offset_z;
        region_noise = voxel_value_noise(seed_value ^ UINT64_C(
            0x510E527FADE682D1), warped_x, warped_z,
            mountain_region_scale);
        region_noise = (region_noise + 1.0) * 0.5;
        region_mask = (region_noise - 0.35) / 0.4;
        if (region_mask < 0.0)
            region_mask = 0.0;
        if (region_mask > 1.0)
            region_mask = 1.0;
        region_mask = voxel_smooth_factor(region_mask);
        ridge_noise = voxel_value_noise(seed_value ^ UINT64_C(
            0x6A09E667F3BCC909), warped_x, warped_z,
            config.mountain_ridge_scale);
        detail_ridge_noise = voxel_value_noise(seed_value ^ UINT64_C(
            0x3C6EF372FE94F82B), warped_x, warped_z,
            mountain_detail_scale);
        valley_noise = voxel_value_noise(seed_value ^ UINT64_C(
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

        erosion_noise = voxel_value_noise(seed_value ^ UINT64_C(
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

static void voxel_add_biome_sample(voxel_biome_sample *sample,
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

static void voxel_sample_biomes(const voxel_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z,
    voxel_biome_sample *sample) noexcept
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
    biome_zone_width = voxel_get_biome_zone_width(config, seed_value);
    cell_x = voxel_floor_div(world_block_x,
        biome_zone_width);
    cell_z = voxel_floor_div(world_block_z,
        biome_zone_width);
    if (config.enable_biome_transitions == FT_FALSE
        || config.biome_transition_noise_strength == 0U)
    {
        voxel_add_biome_sample(sample, voxel_select_biome(config,
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
                + static_cast<int32_t>(voxel_signed_unit_noise(
                    seed_value ^ UINT64_C(0xA24BAED4963EE407),
                    cell_x + neighbour_x, cell_z + neighbour_z)
                    * static_cast<double>(biome_zone_width) * 0.35);
            site_z = cell_origin_z + biome_zone_width / 2
                + static_cast<int32_t>(voxel_signed_unit_noise(
                    seed_value ^ UINT64_C(0x9FB21C651E98DF25),
                    cell_x + neighbour_x, cell_z + neighbour_z)
                    * static_cast<double>(biome_zone_width) * 0.35);
            distance_x = static_cast<double>(world_block_x - site_x);
            distance_z = static_cast<double>(world_block_z - site_z);
            distance_squared = (distance_x * distance_x)
                + (distance_z * distance_z);
            site_weight = 1.0 / (distance_squared + 64.0);
            site_biome = voxel_select_biome(config, seed_value,
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
        voxel_add_biome_sample(sample, selected_biomes[index],
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
        sample->biome_indices[0] = voxel_select_biome(config, seed_value,
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

static uint32_t voxel_sample_biome_block(
    const voxel_generation_config &config,
    const voxel_biome_sample &sample, uint64_t seed_value,
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
    sample_value = static_cast<double>(voxel_mix_u64(sample_seed)
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

static const voxel_tree_template *voxel_sample_tree_template(
    const voxel_generation_config &config,
    const voxel_biome_sample &sample, uint64_t seed_value)
{
    uint32_t index;
    uint32_t selected_biome;
    double tree_weight_total;
    double sample_value;
    double accumulated_weight;
    const voxel_tree_template *tree_template;

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
    sample_value = static_cast<double>(voxel_mix_u64(seed_value)
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
        tree_template = voxel_generation_config_get_tree_template(config,
            config.biomes[selected_biome].tree_template_indices[
                voxel_mix_u64(seed_value ^ VOXEL_FEATURE_TREE_SALT)
                    % config.biomes[selected_biome].tree_template_count]);
    }
    return (tree_template);
}

static int32_t voxel_height_from_biomes(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_biome_sample &sample,
    const voxel_generation_config &config) noexcept
{
    int32_t height;
    uint32_t index;

    height = 0;
    index = 0U;
    while (index < sample.count)
    {
        height += static_cast<int32_t>(voxel_column_height(seed_value,
            world_block_x, world_block_z,
            config.biomes[sample.biome_indices[index]].profile,
            config.biomes[sample.biome_indices[index]].allow_mountain_ridges,
            config) * sample.weights[index]);
        index += 1U;
    }
    return (height);
}

static int32_t voxel_estimate_slope(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept
{
    int32_t height_west;
    int32_t height_east;
    int32_t height_north;
    int32_t height_south;
    int32_t slope_x;
    int32_t slope_z;

    height_west = voxel_sample_height(seed_value, world_block_x - 1,
        world_block_z, config);
    height_east = voxel_sample_height(seed_value, world_block_x + 1,
        world_block_z, config);
    height_north = voxel_sample_height(seed_value, world_block_x,
        world_block_z - 1, config);
    height_south = voxel_sample_height(seed_value, world_block_x,
        world_block_z + 1, config);
    slope_x = height_east - height_west;
    if (slope_x < 0)
        slope_x = -slope_x;
    slope_z = height_south - height_north;
    if (slope_z < 0)
        slope_z = -slope_z;
    return (slope_x + slope_z);
}

struct voxel_height_range
{
    int32_t minimum;
    int32_t maximum;
};

static voxel_height_range voxel_height_range_for_config(
    const voxel_generation_config &config) noexcept
{
    uint32_t biome_index;
    int32_t minimum_height;
    int32_t maximum_height;
    int32_t candidate_minimum;
    int32_t candidate_maximum;

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
    return (voxel_height_range{minimum_height, maximum_height});
}

static int32_t voxel_sample_height(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, const voxel_generation_config &config) noexcept
{
    if (config.biome_selector != ft_nullptr)
        (void)voxel_select_biome(config, seed_value, world_block_x, world_block_z);
    voxel_biome_sample sample;
    voxel_sample_biomes(config, seed_value, world_block_x, world_block_z, &sample);
    const int32_t height = voxel_height_from_biomes(seed_value, world_block_x, world_block_z, sample, config);
    const voxel_height_range range = voxel_height_range_for_config(config);
    return (std::clamp(height, range.minimum, range.maximum));
}

static void voxel_prepare_height_samples(uint64_t seed_value,
    int32_t origin_x, int32_t origin_z, const voxel_generation_config &config,
    voxel_column_cache *columns, int32_t *heights) noexcept
{
    const voxel_height_range range = voxel_height_range_for_config(config);
    int32_t z = -1;
    while (z <= GAME_VOXEL_CHUNK_DEPTH)
    {
        int32_t x = -1;
        while (x <= GAME_VOXEL_CHUNK_WIDTH)
        {
            voxel_biome_sample border_sample;
            voxel_biome_sample *sample = &border_sample;
            if (x >= 0 && x < GAME_VOXEL_CHUNK_WIDTH && z >= 0 && z < GAME_VOXEL_CHUNK_DEPTH)
                sample = &columns[z * GAME_VOXEL_CHUNK_WIDTH + x].biome_sample;
            voxel_sample_biomes(config, seed_value, origin_x + x, origin_z + z, sample);
            const int32_t height = voxel_height_from_biomes(seed_value, origin_x + x,
                origin_z + z, *sample, config);
            heights[(z + 1) * VOXEL_HEIGHT_SAMPLE_WIDTH + x + 1]
                = std::clamp(height, range.minimum, range.maximum);
            x += 1;
        }
        z += 1;
    }
    return ;
}

static int32_t voxel_smooth_heightfield(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept
{
    int32_t offset_x;
    int32_t offset_z;
    int32_t sample_count;
    int32_t weighted_height;
    int32_t sample_height;
    int32_t sample_weight;

    offset_z = -VOXEL_HEIGHTMAP_SMOOTH_RADIUS;
    sample_count = 0;
    weighted_height = 0;
    while (offset_z <= VOXEL_HEIGHTMAP_SMOOTH_RADIUS)
    {
        offset_x = -VOXEL_HEIGHTMAP_SMOOTH_RADIUS;
        while (offset_x <= VOXEL_HEIGHTMAP_SMOOTH_RADIUS)
        {
            sample_height = voxel_sample_height(seed_value,
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
        return (voxel_sample_height(seed_value, world_block_x,
            world_block_z, config));
    return (weighted_height / sample_count);
}

static ft_bool voxel_should_place_feature(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z, uint64_t salt,
    uint64_t threshold) noexcept
{
    uint64_t feature_seed;

    if (threshold == 0U)
        return (FT_FALSE);
    if (threshold >= 100U)
        return (FT_TRUE);
    feature_seed = voxel_feature_seed(seed_value, world_block_x,
        world_block_z, salt);
    if ((feature_seed % 100U) < threshold)
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t voxel_write_snow_cap(game_voxel_chunk &chunk, int32_t local_x,
    int32_t column_height, int32_t local_z, const voxel_layer_config &layers,
    game_voxel_generated_column_block *column_blocks) noexcept
{
    if (column_height < 0)
        return (FT_ERR_SUCCESS);
    if (column_height >= GAME_VOXEL_CHUNK_HEIGHT)
        return (FT_ERR_OUT_OF_RANGE);
    const uint32_t count = std::min(layers.snow_cap_depth,
        static_cast<uint32_t>(column_height + 1));
    uint32_t index = 0U;
    while (index < count)
    {
        column_blocks[index] = {column_height - static_cast<int32_t>(index),
            layers.snow_cap_block_id};
        index += 1U;
    }
    return (chunk.write_generated_column(local_x, local_z, column_blocks, count));
}

static uint32_t voxel_ground_cover_block_for_biome(uint32_t biome,
    uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z) noexcept
{
    uint64_t variant_seed;
    uint32_t variant;

    variant_seed = voxel_feature_seed(seed_value, world_block_x,
        world_block_z, VOXEL_FEATURE_GROUND_COVER_VARIANT_SALT);
    variant = static_cast<uint32_t>(variant_seed % 100U);
    if (biome == VOXEL_BIOME_DESERT)
    {
        if (variant < 60U)
            return (VOXEL_GENERATOR_DEAD_BUSH_BLOCK);
        return (VOXEL_GENERATOR_SHRUB_BLOCK);
    }
    if (biome == VOXEL_BIOME_SNOW || biome == VOXEL_BIOME_MOUNTAINS)
        return (VOXEL_GENERATOR_DEAD_BUSH_BLOCK);
    if (variant < 25U)
        return (VOXEL_GENERATOR_RED_FLOWER_BLOCK);
    if (variant < 45U)
        return (VOXEL_GENERATOR_YELLOW_FLOWER_BLOCK);
    if (variant < 65U)
        return (VOXEL_GENERATOR_TALL_GRASS_BLOCK);
    if (variant < 80U)
        return (VOXEL_GENERATOR_FERN_BLOCK);
    if (variant < 88U)
        return (VOXEL_GENERATOR_RED_MUSHROOM_BLOCK);
    if (variant < 96U)
        return (VOXEL_GENERATOR_BROWN_MUSHROOM_BLOCK);
    return (VOXEL_GENERATOR_SHRUB_BLOCK);
}

static double voxel_cave_noise(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_y, int32_t world_block_z,
    int32_t scale, uint64_t salt) noexcept
{
    uint64_t layer_seed;
    int32_t layer_y;

    layer_y = voxel_floor_div(world_block_y, scale);
    layer_seed = seed_value ^ salt
        ^ (static_cast<uint64_t>(layer_y) * UINT64_C(0x9E3779B97F4A7C15));
    return (voxel_value_noise(layer_seed,
        world_block_x + (world_block_y * 13),
        world_block_z - (world_block_y * 7), scale));
}

static constexpr int32_t VOXEL_CAVE_CACHE_WIDTH =
    (GAME_VOXEL_CHUNK_WIDTH - 1 + VOXEL_CAVE_CELL_SIZE - 1) / VOXEL_CAVE_CELL_SIZE + 3;
static constexpr int32_t VOXEL_CAVE_CACHE_DEPTH =
    (GAME_VOXEL_CHUNK_DEPTH - 1 + VOXEL_CAVE_CELL_SIZE - 1) / VOXEL_CAVE_CELL_SIZE + 3;
static constexpr int32_t VOXEL_CAVE_CACHE_HEIGHT =
    (GAME_VOXEL_CHUNK_HEIGHT - 1) / VOXEL_CAVE_CELL_SIZE + 3;

struct voxel_cave_room
{
    int64_t center_x;
    int64_t center_z;
    int32_t center_y;
    uint8_t radius;
    ft_bool entrance;
};

struct voxel_cave_room_cache
{
    int32_t first_x;
    int32_t first_z;
    voxel_cave_room rooms[VOXEL_CAVE_CACHE_HEIGHT][VOXEL_CAVE_CACHE_DEPTH][VOXEL_CAVE_CACHE_WIDTH];
};

static void voxel_prepare_cave_rooms(voxel_cave_room_cache &cache,
    uint64_t seed, int32_t origin_x, int32_t origin_z,
    const voxel_underground_structure_config &config) noexcept
{
    cache.first_x = voxel_floor_div(origin_x, VOXEL_CAVE_CELL_SIZE) - 1;
    cache.first_z = voxel_floor_div(origin_z, VOXEL_CAVE_CELL_SIZE) - 1;
    const uint64_t height_range = static_cast<uint64_t>(
        static_cast<int64_t>(config.maximum_height) - config.minimum_height + 1);
    int32_t y = 0;
    while (y < VOXEL_CAVE_CACHE_HEIGHT)
    {
        int32_t z = 0;
        while (z < VOXEL_CAVE_CACHE_DEPTH)
        {
            int32_t x = 0;
            while (x < VOXEL_CAVE_CACHE_WIDTH)
            {
                voxel_cave_room &room = cache.rooms[y][z][x];
                const int32_t cell_x = cache.first_x + x;
                const int32_t cell_y = y - 1;
                const int32_t cell_z = cache.first_z + z;
                const uint64_t candidate = voxel_mix_u64(seed
                    ^ static_cast<uint64_t>(cell_x) * UINT64_C(0x9E3779B97F4A7C15)
                    ^ static_cast<uint64_t>(cell_y) * UINT64_C(0xC2B2AE3D27D4EB4F)
                    ^ static_cast<uint64_t>(cell_z) * UINT64_C(0x165667B19E3779F9)
                    ^ UINT64_C(0xCA7E700D));
                room.radius = 0U;
                if (candidate % 100U < config.cave_room_chance_percent)
                {
                    room.center_x = static_cast<int64_t>(cell_x) * VOXEL_CAVE_CELL_SIZE + 2
                        + static_cast<int32_t>((candidate >> 8U) % (VOXEL_CAVE_CELL_SIZE - 4));
                    room.center_z = static_cast<int64_t>(cell_z) * VOXEL_CAVE_CELL_SIZE + 2
                        + static_cast<int32_t>((candidate >> 16U) % (VOXEL_CAVE_CELL_SIZE - 4));
                    room.center_y = static_cast<int32_t>(config.minimum_height
                        + static_cast<int64_t>((candidate >> 24U) % height_range));
                    if (config.enable_cavern_rooms == FT_TRUE
                        && (candidate >> 48U) % 100U < config.cavern_room_chance_percent)
                        room.radius = static_cast<uint8_t>(config.cavern_room_radius);
                    else if ((candidate >> 32U) % 100U < config.cave_large_chance_percent)
                        room.radius = static_cast<uint8_t>(config.cave_large_radius);
                    else
                        room.radius = static_cast<uint8_t>(config.cave_small_radius);
                    room.entrance = ((candidate >> 40U) % 100U < config.cave_entrance_chance_percent);
                }
                x += 1;
            }
            z += 1;
        }
        y += 1;
    }
    return ;
}

static ft_bool voxel_should_carve_ravine(uint64_t seed,
    int32_t x, int32_t y, int32_t z, int32_t surface_height,
    const voxel_underground_structure_config &config) noexcept
{
    if (y < config.minimum_height || y > config.maximum_height)
        return (FT_FALSE);
    int32_t surface_margin = VOXEL_CAVE_SURFACE_MARGIN;
    if (config.ravine_depth > 0U)
        surface_margin = static_cast<int32_t>(config.ravine_depth);
    if (y < surface_height - surface_margin && voxel_cave_noise(seed, x, y, z,
            VOXEL_CAVE_PRIMARY_SCALE, VOXEL_CAVE_PRIMARY_SALT) > 0.34)
    {
        const double threshold = -0.12 + static_cast<double>(config.ravine_width) * 0.04;
        if (voxel_cave_noise(seed, x, y, z, VOXEL_CAVE_DETAIL_SCALE,
                VOXEL_CAVE_DETAIL_SALT) > threshold)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static void voxel_mark_room_column(ft_bool *carved, const voxel_cave_room &room,
    int32_t x, int32_t z, int32_t first_y, int32_t last_y, int32_t room_ceiling,
    uint32_t entrance_radius) noexcept
{
    if (room.radius == 0U)
        return ;
    const int64_t delta_x = x - room.center_x;
    const int64_t delta_z = z - room.center_z;
    const int64_t planar_distance = delta_x * delta_x + delta_z * delta_z;
    const int64_t radius_squared_at_column = room.radius * room.radius - planar_distance;
    if (radius_squared_at_column >= 0 && first_y <= room_ceiling)
    {
        const int32_t vertical_radius = static_cast<int32_t>(
            std::sqrt(static_cast<double>(radius_squared_at_column)));
        const int64_t first_room_y = std::max<int64_t>(first_y,
            static_cast<int64_t>(room.center_y) - vertical_radius);
        const int64_t last_room_y = std::min<int64_t>(std::min(last_y, room_ceiling),
            static_cast<int64_t>(room.center_y) + vertical_radius);
        if (first_room_y <= last_room_y)
            std::fill(carved + first_room_y, carved + last_room_y + 1, FT_TRUE);
    }
    if (room.entrance == FT_TRUE
        && planar_distance <= entrance_radius * entrance_radius)
    {
        const int32_t first_entrance_y = std::max(first_y, room.center_y);
        if (first_entrance_y <= last_y)
            std::fill(carved + first_entrance_y, carved + last_y + 1, FT_TRUE);
    }
    return ;
}

static void voxel_prepare_cave_column(ft_bool *carved, const voxel_cave_room_cache &cache,
    int32_t x, int32_t z, int32_t surface_height,
    const voxel_underground_structure_config &config) noexcept
{
    const int32_t minimum_y = std::max(VOXEL_BEDROCK_FLOOR_Y + 1, config.minimum_height);
    const int32_t maximum_y = surface_height - 1;
    if (minimum_y > maximum_y)
        return ;
    const int32_t first_x = voxel_floor_div(x, VOXEL_CAVE_CELL_SIZE) - 1 - cache.first_x;
    const int32_t first_z = voxel_floor_div(z, VOXEL_CAVE_CELL_SIZE) - 1 - cache.first_z;
    const int32_t last_room = maximum_y / VOXEL_CAVE_CELL_SIZE + 2;
    const int32_t room_ceiling = surface_height - VOXEL_CAVE_SURFACE_MARGIN - 1;
    int32_t index_y = minimum_y / VOXEL_CAVE_CELL_SIZE;
    while (index_y <= last_room)
    {
        // The room is considered from its own vertical cell and the adjacent cells.
        const int32_t first_y = std::max(minimum_y, (index_y - 2) * VOXEL_CAVE_CELL_SIZE);
        const int32_t last_y = std::min(maximum_y, (index_y + 1) * VOXEL_CAVE_CELL_SIZE - 1);
        int32_t index_z = first_z;
        while (index_z < first_z + 3)
        {
            int32_t index_x = first_x;
            while (index_x < first_x + 3)
            {
                voxel_mark_room_column(carved, cache.rooms[index_y][index_z][index_x],
                    x, z, first_y, last_y, room_ceiling, config.cave_entrance_radius);
                index_x += 1;
            }
            index_z += 1;
        }
        index_y += 1;
    }
    return ;
}

static uint8_t voxel_surface_water_kind(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config) noexcept
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

    if (voxel_should_place_feature(seed_value, world_block_x, world_block_z,
            VOXEL_FEATURE_WATER_SALT, config.water_chance_percent)
        == FT_TRUE)
        return (VOXEL_SURFACE_WATER_LEGACY);
    if (config.fluids.enable_rivers == FT_TRUE)
    {
        river_noise = voxel_value_noise(seed_value ^ UINT64_C(
            0x3C6EF372FE94F82B), world_block_x, world_block_z,
            config.fluids.river_noise_scale);
        if (river_noise < 0.0)
            river_noise = -river_noise;
        if (river_noise < (0.04 + (static_cast<double>(
                config.fluids.river_width) * 0.01)))
            return (VOXEL_SURFACE_WATER_RIVER);
    }
    if (config.fluids.enable_lakes == FT_TRUE)
    {
        lake_region_size = config.fluids.lake_noise_scale;
        if (lake_region_size < 8)
            lake_region_size = 8;
        lake_region_x = voxel_floor_division(world_block_x,
            lake_region_size) * lake_region_size;
        lake_region_z = voxel_floor_division(world_block_z,
            lake_region_size) * lake_region_size;
        lake_region_seed = voxel_feature_seed(seed_value, lake_region_x,
            lake_region_z, VOXEL_FEATURE_WATER_SALT
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
                return (VOXEL_SURFACE_WATER_LAKE);
        }
    }
    return (VOXEL_SURFACE_WATER_NONE);
}

static int32_t voxel_surface_water_level(uint8_t water_kind,
    int32_t sea_level) noexcept
{
    if (water_kind == VOXEL_SURFACE_WATER_RIVER)
        return (sea_level - 1);
    if (water_kind == VOXEL_SURFACE_WATER_LAKE)
        return (sea_level - 2);
    return (sea_level);
}

static uint32_t voxel_surface_water_max_depth(uint8_t water_kind,
    const voxel_generation_config &config) noexcept
{
    if (water_kind == VOXEL_SURFACE_WATER_RIVER)
        return (config.fluids.surface_river_depth);
    return (config.fluids.surface_lake_depth);
}

static ft_bool voxel_surface_water_accepts_block(int32_t y, uint32_t block_id,
    const void *user_data) noexcept
{
    if (y == *static_cast<const int32_t *>(user_data))
        return (voxel_block_is_solid(block_id));
    return (block_id == VOXEL_GENERATOR_AIR_BLOCK || voxel_block_is_liquid(block_id) == FT_TRUE);
}

static ft_bool voxel_surface_water_candidate_is_valid(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const voxel_generation_config &config, uint8_t *water_kind,
    uint64_t *feature_id) noexcept
{
    uint8_t kind;
    int32_t level;
    int32_t column_height;
    int32_t feature_cell_size;
    int32_t feature_cell_x;
    int32_t feature_cell_z;

    kind = voxel_surface_water_kind(seed_value, world_block_x,
        world_block_z, config);
    if (kind == VOXEL_SURFACE_WATER_NONE)
        return (FT_FALSE);
    level = voxel_surface_water_level(kind, config.sea_level);
    column_height = voxel_smooth_heightfield(seed_value, world_block_x,
        world_block_z, config);
    if (column_height >= level)
        return (FT_FALSE);
    if (level - column_height <= 0
        || static_cast<uint32_t>(level - column_height)
            > voxel_surface_water_max_depth(kind, config))
        return (FT_FALSE);
    if (kind == VOXEL_SURFACE_WATER_RIVER)
        feature_cell_size = config.fluids.river_noise_scale;
    else
        feature_cell_size = config.fluids.lake_noise_scale;
    if (feature_cell_size > 0)
    {
        feature_cell_x = voxel_floor_division(world_block_x,
            feature_cell_size) * feature_cell_size;
        feature_cell_z = voxel_floor_division(world_block_z,
            feature_cell_size) * feature_cell_size;
    }
    else
    {
        feature_cell_x = world_block_x;
        feature_cell_z = world_block_z;
    }
    *water_kind = kind;
    *feature_id = voxel_feature_seed(seed_value, feature_cell_x,
        feature_cell_z, VOXEL_FEATURE_WATER_SALT
            ^ static_cast<uint64_t>(kind));
    return (FT_TRUE);
}

static ft_bool voxel_surface_water_neighbor_matches(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    int32_t local_x, int32_t local_z, int32_t offset_x, int32_t offset_z,
    const voxel_generation_config &config, voxel_column_cache *column_cache,
    uint8_t expected_kind, uint64_t expected_feature_id) noexcept
{
    int32_t neighbor_x;
    int32_t neighbor_z;
    int32_t neighbor_index;
    uint8_t neighbor_kind;
    uint64_t neighbor_feature_id;

    neighbor_x = local_x + offset_x;
    neighbor_z = local_z + offset_z;
    if (neighbor_x >= 0 && neighbor_x < GAME_VOXEL_CHUNK_WIDTH
        && neighbor_z >= 0 && neighbor_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        neighbor_index = neighbor_z * GAME_VOXEL_CHUNK_WIDTH + neighbor_x;
        return (column_cache[neighbor_index].has_surface_water == FT_TRUE
            && column_cache[neighbor_index].surface_water_kind
                == expected_kind
            && column_cache[neighbor_index].surface_water_feature_id
                == expected_feature_id);
    }
    if (voxel_surface_water_candidate_is_valid(seed_value,
            world_block_origin_x + neighbor_x,
            world_block_origin_z + neighbor_z, config, &neighbor_kind,
            &neighbor_feature_id) == FT_FALSE)
        return (FT_FALSE);
    return (neighbor_kind == expected_kind
        && neighbor_feature_id == expected_feature_id);
}

static ft_bool voxel_stage_dependencies_are_met(uint32_t requested_mask,
    uint32_t previous_mask) noexcept
{
    uint32_t available_mask = requested_mask | previous_mask;
    const uint32_t base_and_caves = VOXEL_STAGE_BASE_TERRAIN
        | VOXEL_STAGE_CAVES;

    if ((requested_mask & VOXEL_STAGE_FLUIDS) != 0U
        && (available_mask & base_and_caves) != base_and_caves)
        return (FT_FALSE);
    if ((requested_mask & VOXEL_STAGE_FLUIDS) != 0U
        && (previous_mask & (VOXEL_STAGE_DECORATION
            | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES)) != 0U)
        return (FT_FALSE);
    if ((requested_mask & VOXEL_STAGE_DECORATION) != 0U
        && (available_mask & (base_and_caves | VOXEL_STAGE_FLUIDS))
            != (base_and_caves | VOXEL_STAGE_FLUIDS))
        return (FT_FALSE);
    if ((requested_mask & VOXEL_STAGE_STRUCTURES) != 0U
        && (available_mask & base_and_caves) != base_and_caves)
        return (FT_FALSE);
    if ((requested_mask & VOXEL_STAGE_ORES) != 0U
        && (available_mask & base_and_caves) != base_and_caves)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool voxel_underground_lake_can_clear_ceiling(
    const game_voxel_chunk &chunk, int32_t local_x, int32_t local_y,
    int32_t local_z, uint32_t lake_depth, ft_bool has_reader) noexcept
{
    if (has_reader == FT_TRUE || lake_depth != 1U)
        return (FT_TRUE);
    // The shared geometry check already validated the lower perimeter.
    int32_t z = -2;
    while (z <= 2)
    {
        int32_t x = -2;
        while (x <= 2)
        {
            if (std::abs(x) == 2 || std::abs(z) == 2)
            {
                uint32_t block_id;
                if (chunk.read_block(local_x + x, local_y + 1, local_z + z,
                        &block_id) != FT_ERR_SUCCESS
                    || voxel_block_is_solid(block_id) == FT_FALSE)
                    return (FT_FALSE);
            }
            x += 1;
        }
        z += 1;
    }
    return (FT_TRUE);
}

static ft_bool voxel_block_is_ore_host(uint32_t block_id,
    const voxel_generation_config &config) noexcept
{
    if (voxel_block_is_solid(block_id) == FT_FALSE)
        return (FT_FALSE);
    if (block_id == VOXEL_GENERATOR_BEDROCK_BLOCK)
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

static int32_t voxel_find_surface_height(const game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_z) noexcept
{
    uint32_t column[GAME_VOXEL_CHUNK_HEIGHT];
    if (chunk.copy_region(column, GAME_VOXEL_CHUNK_HEIGHT,
            static_cast<uint32_t>(local_x), static_cast<uint32_t>(local_z),
            1U, 1U) != FT_ERR_SUCCESS)
        return (-1);
    int32_t local_y = GAME_VOXEL_CHUNK_HEIGHT - 1;

    while (local_y >= 0)
    {
        if (voxel_block_is_solid(column[local_y]) == FT_TRUE)
            return (local_y);
        local_y -= 1;
    }
    return (-1);
}

static int32_t voxel_place_ore_vein(game_voxel_chunk &chunk,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_y,
    int32_t world_block_z, int32_t local_x, int32_t local_y, int32_t local_z,
    const voxel_ore_rule &ore_rule, uint32_t vein_size,
    const voxel_generation_config &config) noexcept
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
        vein_seed = voxel_mix_u64(seed_value
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
            if (voxel_block_is_ore_host(block_id, config) == FT_FALSE
                && (ore_rule.allow_ore_replacement == FT_FALSE
                    || voxel_block_is_ore(block_id) == FT_FALSE))
            {
                vein_index += 1U;
                continue ;
            }
            target_surface = voxel_find_surface_height(chunk,
                target_x, target_z);
            target_depth = target_surface - target_y;
            if (target_surface >= 0
                && target_depth >= ore_rule.minimum_depth
                && target_depth <= ore_rule.maximum_depth)
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

static int32_t voxel_generate_ores(game_voxel_chunk &chunk,
    uint64_t seed_value, int32_t world_block_origin_x,
    int32_t world_block_origin_z,
    const voxel_generation_config &config) noexcept
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
        && ore_index < VOXEL_MAX_ORE_RULES)
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
            ore_seed = voxel_mix_u64(seed_value
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
                vein_seed = voxel_mix_u64(ore_seed
                    ^ static_cast<uint64_t>(vein_index));
                local_x = static_cast<int32_t>((vein_seed >> 8)
                    % GAME_VOXEL_CHUNK_WIDTH);
                local_z = static_cast<int32_t>((vein_seed >> 24)
                    % GAME_VOXEL_CHUNK_DEPTH);
                surface_height = voxel_find_surface_height(chunk,
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
                    error_code = voxel_place_ore_vein(chunk, seed_value,
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

int32_t voxel_generate_chunk(game_voxel_chunk &chunk,
    const char *seed_string) noexcept
{
    return (voxel_generate_chunk(chunk, 0, 0, seed_string));
}

int32_t voxel_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string) noexcept
{
    voxel_generation_config config;

    if (voxel_default_generation_config(config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_generate_chunk(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config));
}

static int32_t voxel_generate_chunk_stages(game_voxel_chunk &chunk,
    const voxel_generation_config &config,
    const voxel_cross_chunk_callbacks &callbacks,
    const game_voxel_generation_metadata &generation_metadata,
    uint32_t requested_stage_mask) noexcept
{
    const int32_t world_block_origin_x = generation_metadata.world_block_origin_x;
    const int32_t world_block_origin_z = generation_metadata.world_block_origin_z;
    const uint64_t seed_value = generation_metadata.seed_value;

    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    int32_t error_code;
    int32_t column_height;
    int32_t world_block_x;
    int32_t world_block_z;
    uint32_t biome;
    voxel_biome_profile biome_profile;
    uint32_t deep_block_id;
    ft_bool place_shrub;
    ft_bool mountain_active;
    const voxel_tree_template *tree_template;
    uint64_t tree_feature_seed;
    uint32_t feature_index;
    const voxel_feature_rule *feature_rule;
    voxel_column_cache column_cache[VOXEL_COLUMN_CACHE_COUNT];
    int32_t column_index;
    int32_t feature_margin;
    voxel_stage_prepare_columns(seed_value, world_block_origin_x,
        world_block_origin_z, config, column_cache);
    /* Stage: base voxel, caves, voxel-aware layers, and fluids. */
    game_voxel_generated_column_block column_blocks[2U * GAME_VOXEL_CHUNK_HEIGHT];
    const auto &underground = config.underground_structures;
    const ft_bool carve_caves = (requested_stage_mask & VOXEL_STAGE_CAVES) != 0U;
    voxel_cave_room_cache cave_rooms;
    const voxel_cave_room_cache *room_cache = ft_nullptr;
    if (carve_caves == FT_TRUE && underground.enable_cave_rooms == FT_TRUE
        && underground.cave_room_chance_percent != 0U && underground.cave_small_radius != 0U)
    {
        voxel_prepare_cave_rooms(cave_rooms, seed_value,
            world_block_origin_x, world_block_origin_z, underground);
        room_cache = &cave_rooms;
    }
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
            uint32_t column_count = 0U;
            const ft_bool ravine_column = carve_caves == FT_TRUE && underground.enable_ravines == FT_TRUE
                && voxel_should_place_feature(seed_value, world_block_x, world_block_z,
                    UINT64_C(0xD1CEB00C), underground.ravine_chance_percent) == FT_TRUE;
            ft_bool cave_column[GAME_VOXEL_CHUNK_HEIGHT];
            if (room_cache != ft_nullptr)
            {
                std::fill_n(cave_column, column_height + 1, FT_FALSE);
                voxel_prepare_cave_column(cave_column, *room_cache, world_block_x,
                    world_block_z, column_height, underground);
            }
            local_y = 0;
            while (local_y <= column_height)
            {
                if (carve_caves == FT_TRUE && local_y > VOXEL_BEDROCK_FLOOR_Y
                    && ((room_cache != ft_nullptr && cave_column[local_y] == FT_TRUE)
                        || (ravine_column == FT_TRUE
                            && voxel_should_carve_ravine(seed_value, world_block_x,
                                local_y, world_block_z, column_height, underground) == FT_TRUE)))
                {
                    column_blocks[column_count++] = {local_y, VOXEL_GENERATOR_AIR_BLOCK};
                    local_y += 1;
                    continue ;
                }
                if ((requested_stage_mask & VOXEL_STAGE_BASE_TERRAIN) == 0U)
                {
                    local_y += 1;
                    continue ;
                }
                if (local_y <= VOXEL_BEDROCK_FLOOR_Y)
                    block_id = VOXEL_GENERATOR_BEDROCK_BLOCK;
                else if (local_y == column_height)
                    block_id = voxel_sample_biome_block(config,
                        column_cache[column_index].biome_sample, seed_value,
                        world_block_x, local_y, world_block_z,
                        VOXEL_BIOME_SURFACE_TRANSITION_SALT, FT_FALSE);
                else if (local_y >= column_height - biome_profile.topsoil_depth)
                    block_id = voxel_sample_biome_block(config,
                        column_cache[column_index].biome_sample, seed_value,
                        world_block_x, local_y, world_block_z,
                        VOXEL_BIOME_SUBSURFACE_TRANSITION_SALT, FT_TRUE);
                else
                    block_id = deep_block_id;
                if (mountain_active == FT_TRUE
                    && column_cache[column_index].slope_height
                        >= VOXEL_MOUNTAIN_CLIFF_SLOPE
                    && local_y >= column_height
                        - biome_profile.topsoil_depth)
                    block_id = deep_block_id;
                column_blocks[column_count++] = {local_y, block_id};
                local_y += 1;
            }
            if ((requested_stage_mask & VOXEL_STAGE_BASE_TERRAIN) != 0U
                && config.layers.enable_beaches == FT_TRUE
                && column_cache[column_index].has_surface_water == FT_FALSE
                && column_height < config.sea_level)
            {
                const int64_t beach_end = static_cast<int64_t>(column_height)
                    - config.layers.beach_depth;
                const int32_t beach_bottom = static_cast<int32_t>(std::max<int64_t>(-1, beach_end));
                const int32_t underwater_bottom = static_cast<int32_t>(
                    std::max<int64_t>(-1, beach_end - config.layers.underwater_depth));
                local_y = column_height;
                while (local_y > beach_bottom)
                {
                    column_blocks[column_count++] = {local_y, config.layers.beach_block_id};
                    local_y -= 1;
                }
                local_y = beach_bottom;
                while (local_y > underwater_bottom)
                {
                    column_blocks[column_count++] = {local_y, config.layers.underwater_block_id};
                    local_y -= 1;
                }
            }
            if (column_count != 0U)
            {
                error_code = chunk.write_generated_column(local_x, local_z, column_blocks, column_count);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
            local_x += 1;
        }
        local_z += 1;
    }
    /* Stage: surface fluids. Terrain and caves are complete before this
     * pass, and the cached column height remains the natural terrain height. */
    if ((requested_stage_mask & VOXEL_STAGE_FLUIDS) != 0U)
    {
        local_z = 0;
        while (local_z < GAME_VOXEL_CHUNK_DEPTH)
        {
            local_x = 0;
            while (local_x < GAME_VOXEL_CHUNK_WIDTH)
            {
                column_index = local_z * GAME_VOXEL_CHUNK_WIDTH + local_x;
                column_height = column_cache[column_index].column_height;
                if (column_cache[column_index].has_surface_water == FT_TRUE
                    && column_height >= 0
                    && column_height < column_cache[column_index].surface_water_level)
                {
                    if (column_height >= GAME_VOXEL_CHUNK_HEIGHT)
                    {
                        column_cache[column_index].has_surface_water = FT_FALSE;
                        local_x += 1;
                        continue ;
                    }
                    const int32_t top = std::min<int32_t>(GAME_VOXEL_CHUNK_HEIGHT - 1,
                        column_cache[column_index].surface_water_level);
                    uint32_t water_count = 0U;
                    local_y = column_height + 1;
                    while (local_y <= top)
                    {
                        block_id = VOXEL_GENERATOR_WATER_BLOCK;
                        if (local_y == config.sea_level
                            && column_cache[column_index].biome == VOXEL_BIOME_SNOW)
                            block_id = VOXEL_GENERATOR_ICE_BLOCK;
                        column_blocks[water_count++] = {local_y, block_id};
                        local_y += 1;
                    }
                    const game_voxel_generated_column_check check = {
                        column_height, top, voxel_surface_water_accepts_block, &column_height
                    };
                    error_code = chunk.write_generated_column(local_x, local_z,
                        column_blocks, water_count, &check,
                        &column_cache[column_index].has_surface_water);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
                local_x += 1;
            }
            local_z += 1;
        }
    }
    /* Stage: underground fluids. Surface and underground fluids are both
     * complete before any snow, aquatic feature, shrub, or tree pass. */
    if ((requested_stage_mask & VOXEL_STAGE_FLUIDS) != 0U
        && config.fluids.enable_underground_lakes == FT_TRUE
        && config.fluids.underground_lake_chance_percent > 0U)
    {
        /* Fill only air pockets with a solid floor and roof: small enclosed
         * underground lakes, rather than exposed water hanging in voxel. */
        local_z = 2;
        while (local_z + 2 < GAME_VOXEL_CHUNK_DEPTH)
        {
            local_x = 2;
            while (local_x + 2 < GAME_VOXEL_CHUNK_WIDTH)
            {
                column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
                column_height = column_cache[column_index].column_height;
                local_y = config.fluids.underground_lake_minimum_y;
                while (local_y <= config.fluids.underground_lake_maximum_y
                    && local_y < GAME_VOXEL_CHUNK_HEIGHT)
                {
                    if (voxel_underground_lake_vertical_bounds_are_valid(
                            local_y, config) == FT_TRUE
                        && local_y + static_cast<int32_t>(config.fluids
                            .underground_lake_depth)
                            + static_cast<int32_t>(config.fluids
                                .underground_lake_roof_thickness)
                            < column_height
                        && local_y % (static_cast<int32_t>(config.fluids
                            .underground_lake_depth) + 7) == 4
                        && voxel_should_place_feature(seed_value
                            ^ static_cast<uint64_t>(local_y),
                            world_block_origin_x + local_x,
                            world_block_origin_z + local_z,
                            VOXEL_FEATURE_WATER_SALT
                                ^ UINT64_C(0x6A09E667F3BCC909),
                            config.fluids.underground_lake_chance_percent) == FT_TRUE
                    )
                    {
                        if (voxel_underground_lake_geometry_is_valid(chunk,
                                local_x, local_y, local_z, world_block_origin_x,
                                world_block_origin_z, config, callbacks) == FT_FALSE)
                        {
                            local_y += 1;
                            continue ;
                        }
                        if (voxel_underground_lake_can_clear_ceiling(
                                chunk, local_x, local_y, local_z,
                                config.fluids.underground_lake_depth,
                                callbacks.reader != ft_nullptr) == FT_TRUE)
                        {
                            int32_t clear_z = -1;
                            while (clear_z <= 1)
                            {
                                int32_t clear_x = -1;
                                while (clear_x <= 1)
                                {
                                    error_code = voxel_write_generation_block(
                                        chunk, local_x + clear_x, local_y
                                            + static_cast<int32_t>(config.fluids
                                                .underground_lake_depth),
                                        local_z + clear_z, world_block_origin_x,
                                        world_block_origin_z, callbacks,
                                        VOXEL_GENERATOR_AIR_BLOCK);
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
                                    error_code = voxel_write_generation_block(
                                        chunk, local_x + fill_x,
                                        local_y + fill_depth, local_z + fill_z,
                                        world_block_origin_x,
                                        world_block_origin_z, callbacks,
                                        VOXEL_GENERATOR_WATER_BLOCK);
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
    /* Snow caps are local surface state and must also be generated on chunk
     * border columns.  Tree and shrub placement keeps its interior margin,
     * but excluding the border here makes valid snow disappear until a
     * neighboring chunk happens to be present. */
    const ft_bool decorate = (requested_stage_mask & VOXEL_STAGE_DECORATION) != 0U;
    const ft_bool place_snow_caps = config.layers.enable_snow_caps == FT_TRUE
        && config.layers.snow_cap_depth != 0U;
    if (decorate == FT_TRUE && place_snow_caps == FT_TRUE)
    {
        local_z = 0;
        while (local_z < GAME_VOXEL_CHUNK_DEPTH)
        {
            local_x = 0;
            while (local_x < GAME_VOXEL_CHUNK_WIDTH)
            {
                column_index = local_z * GAME_VOXEL_CHUNK_WIDTH + local_x;
                column_height = column_cache[column_index].column_height;
                if (column_cache[column_index].has_surface_water == FT_FALSE
                    && column_cache[column_index].can_place_snow == FT_TRUE
                    && column_height >= config.layers.snow_cap_minimum_height)
                {
                    error_code = voxel_write_snow_cap(chunk, local_x,
                        column_height, local_z, config.layers, column_blocks);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
                local_x += 1;
            }
            local_z += 1;
        }
    }
    /* Stage: biome decorations and configured structures. */
    feature_margin = 2;
    if (callbacks.features_enabled == FT_TRUE && callbacks.writer != ft_nullptr)
        feature_margin = 0;
    local_z = feature_margin;
    while (decorate == FT_TRUE
        && local_z + feature_margin < GAME_VOXEL_CHUNK_DEPTH)
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
            mountain_active = FT_FALSE;
            if (config.enable_mountain_ridges == FT_TRUE
                && column_cache[column_index].can_place_mountain_ridges
                    == FT_TRUE
                && column_height > column_cache[column_index]
                    .biome_profile.surface_height
                    + column_cache[column_index].biome_profile.height_variation
                    + 4)
                mountain_active = FT_TRUE;
            if (place_snow_caps == FT_TRUE
                && column_cache[column_index].has_surface_water == FT_FALSE
                && column_cache[column_index].can_place_snow == FT_TRUE
                && column_height >= config.layers.snow_cap_minimum_height
                && (mountain_active == FT_FALSE
                    || (column_cache[column_index].slope_height
                        <= VOXEL_MOUNTAIN_SNOW_SLOPE_LIMIT
                        && voxel_value_noise(seed_value ^ UINT64_C(
                            0xD1B54A32D192ED03), world_block_x,
                            world_block_z, config.detail_noise_scale) > -0.35)))
            {
                error_code = voxel_write_snow_cap(chunk, local_x,
                    column_height, local_z, config.layers, column_blocks);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
            if (column_cache[column_index].has_surface_water == FT_TRUE)
            {
                const int32_t water_level =
                    column_cache[column_index].surface_water_level;
                uint32_t above_water_block;
                if (water_level + 1 < GAME_VOXEL_CHUNK_HEIGHT
                    && chunk.read_block(local_x, water_level + 1, local_z,
                        &above_water_block) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_OPERATION);
                if ((biome == VOXEL_BIOME_PLAINS
                        || biome == VOXEL_BIOME_HILLS)
                    && water_level + 1 < GAME_VOXEL_CHUNK_HEIGHT
                    && voxel_should_place_feature(seed_value,
                        world_block_x, world_block_z,
                        VOXEL_FEATURE_AQUATIC_PLANT_SALT, 10U) == FT_TRUE
                    && above_water_block == VOXEL_GENERATOR_AIR_BLOCK)
                {
                    error_code = chunk.write_generated_block(local_x,
                        water_level + 1, local_z,
                        VOXEL_GENERATOR_LILY_PAD_BLOCK);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
                else if (column_height + 1 < water_level
                    && voxel_should_place_feature(seed_value,
                        world_block_x, world_block_z,
                        VOXEL_FEATURE_AQUATIC_PLANT_SALT, 15U) == FT_TRUE)
                {
                    uint32_t submerged_block;
                    if (chunk.read_block(local_x, column_height + 1,
                            local_z, &submerged_block) != FT_ERR_SUCCESS)
                        return (FT_ERR_INVALID_OPERATION);
                    if (voxel_block_is_liquid(submerged_block) == FT_TRUE)
                    {
                        error_code = chunk.write_generated_block(local_x,
                            column_height + 1, local_z,
                            VOXEL_GENERATOR_SEAGRASS_BLOCK);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            bool paper_reed_placed = false;
            if (column_cache[column_index].has_surface_water == FT_FALSE
                && column_height + 1 < GAME_VOXEL_CHUNK_HEIGHT
                && voxel_should_place_feature(seed_value, world_block_x,
                    world_block_z, VOXEL_FEATURE_PAPER_REED_SALT, 12U)
                    == FT_TRUE)
            {
                bool near_surface_water = false;
                for (int32_t water_z = std::max(0, local_z - 4);
                    water_z <= std::min(GAME_VOXEL_CHUNK_DEPTH - 1,
                        local_z + 4) && !near_surface_water; ++water_z)
                {
                    for (int32_t water_x = std::max(0, local_x - 4);
                        water_x <= std::min(GAME_VOXEL_CHUNK_WIDTH - 1,
                            local_x + 4); ++water_x)
                    {
                        const std::size_t neighbor_index =
                            static_cast<std::size_t>(water_z)
                                * GAME_VOXEL_CHUNK_WIDTH
                            + static_cast<std::size_t>(water_x);
                        const int32_t water_level = column_cache[
                            neighbor_index].surface_water_level;
                        if (column_cache[neighbor_index].has_surface_water
                                == FT_TRUE
                            && water_level >= column_height - 1
                            && water_level <= column_height + 1)
                        {
                            near_surface_water = true;
                            break;
                        }
                    }
                }
                uint32_t ground_block = VOXEL_GENERATOR_AIR_BLOCK;
                uint32_t above_ground_block = VOXEL_GENERATOR_AIR_BLOCK;
                if (near_surface_water
                    && chunk.read_block(local_x, column_height, local_z,
                        &ground_block) == FT_ERR_SUCCESS
                    && chunk.read_block(local_x, column_height + 1, local_z,
                        &above_ground_block) == FT_ERR_SUCCESS
                    && (ground_block == VOXEL_GENERATOR_GRASS_BLOCK
                        || ground_block == VOXEL_GENERATOR_DIRT_BLOCK
                        || ground_block == VOXEL_GENERATOR_MUD_BLOCK
                        || ground_block == VOXEL_GENERATOR_SAND_BLOCK)
                    && voxel_block_is_replaceable(above_ground_block)
                        == FT_TRUE)
                {
                    error_code = chunk.write_generated_block(local_x,
                        column_height + 1, local_z,
                        VOXEL_GENERATOR_PAPER_REED_IMMATURE_BLOCK);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    paper_reed_placed = true;
                }
            }
            if (paper_reed_placed == false
                && place_shrub == FT_TRUE
                && column_cache[column_index].has_surface_water == FT_FALSE
                && column_height + VOXEL_FEATURE_SHRUB_HEIGHT_OFFSET
                    < GAME_VOXEL_CHUNK_HEIGHT
                && voxel_should_place_feature(seed_value, world_block_x,
                    world_block_z, VOXEL_FEATURE_SHRUB_SALT,
                    column_cache[column_index].shrub_chance_percent) == FT_TRUE)
            {
                uint32_t above_surface_block;
                if (chunk.read_block(local_x, column_height + 1, local_z,
                        &above_surface_block) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_OPERATION);
                if (voxel_block_is_replaceable(above_surface_block) == FT_TRUE)
                {
                    error_code = chunk.write_generated_block(local_x,
                        column_height + VOXEL_FEATURE_SHRUB_HEIGHT_OFFSET,
                        local_z, voxel_ground_cover_block_for_biome(biome,
                            seed_value, world_block_x, world_block_z));
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                }
            }
            if (paper_reed_placed == false
                && column_cache[column_index].can_place_trees == FT_TRUE
                && column_cache[column_index].tree_chance_percent != 0U)
            {
                if (column_cache[column_index].has_surface_water == FT_TRUE)
                {
                    local_x += 4;
                    continue ;
                }
                tree_feature_seed = voxel_feature_seed(seed_value,
                    world_block_x, world_block_z, VOXEL_FEATURE_TREE_SALT);
                if ((tree_feature_seed % 100U)
                    < column_cache[column_index].tree_chance_percent)
                {
                    tree_template = voxel_sample_tree_template(config,
                        column_cache[column_index].biome_sample,
                        tree_feature_seed);
                    column_height = column_cache[column_index].column_height;
                    if (tree_template != ft_nullptr)
                    {
                        error_code = voxel_apply_tree_template(chunk,
                            local_x, column_height + 1, local_z,
                            world_block_origin_x, world_block_origin_z,
                            *tree_template, callbacks);
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
    while ((requested_stage_mask & VOXEL_STAGE_STRUCTURES) != 0U
        && feature_index < config.feature_count
        && feature_index < VOXEL_MAX_FEATURE_RULES)
    {
        feature_rule = &config.features[feature_index];
        if (feature_rule->template_data != ft_nullptr
            && feature_rule->chance_percent != 0U)
        {
            const uint64_t feature_salt = VOXEL_FEATURE_TREE_SALT
                ^ static_cast<uint64_t>(feature_index + 1U);
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
                        if (voxel_should_place_feature(seed_value,
                                world_block_origin_x + local_x,
                                world_block_origin_z + local_z, feature_salt,
                                feature_rule->chance_percent) == FT_TRUE)
                        {
                            error_code = voxel_apply_tree_template(chunk,
                                local_x, column_height + 1, local_z,
                                world_block_origin_x, world_block_origin_z,
                                *feature_rule->template_data, callbacks);
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
    if ((requested_stage_mask & VOXEL_STAGE_ORES) != 0U)
    {
        error_code = voxel_generate_ores(chunk, seed_value,
            world_block_origin_x, world_block_origin_z, config);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    chunk.clear_dirty();
    return (chunk.set_generation_metadata(generation_metadata));
}

static int32_t voxel_generate_chunk_from_config(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const voxel_generation_config &config,
    const voxel_cross_chunk_callbacks &callbacks, ft_bool configuration_validated,
    uint32_t precomputed_signature, ft_bool coordinate_seed_overridden,
    uint64_t coordinate_seed, uint32_t requested_stage_mask) noexcept
{
    constexpr uint32_t ALL_STAGES = VOXEL_STAGE_BASE_TERRAIN
        | VOXEL_STAGE_CAVES | VOXEL_STAGE_FLUIDS | VOXEL_STAGE_DECORATION
        | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES;
    if (chunk.is_generation_protected() == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (requested_stage_mask == 0U || (requested_stage_mask & ~ALL_STAGES) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (configuration_validated == FT_FALSE
        && voxel_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);

    uint64_t seed_value = voxel_seed_value(seed_string);
    if (coordinate_seed_overridden == FT_TRUE)
        seed_value = voxel_mix_u64(seed_value ^ coordinate_seed);
    uint32_t configuration_signature = precomputed_signature;
    if (configuration_validated == FT_FALSE)
        configuration_signature = voxel_generation_config_signature(config);
    uint32_t previous_stage_mask = 0U;
    if (chunk.generation_metadata_matches(seed_value, world_block_origin_x,
            world_block_origin_z, configuration_signature) == FT_TRUE
        && chunk.get_generation_metadata().generator_version == VOXEL_GENERATOR_VERSION)
    {
        previous_stage_mask = chunk.get_generation_metadata().completed_stage_mask;
        if ((previous_stage_mask & requested_stage_mask) == requested_stage_mask)
        {
            chunk.clear_dirty();
            return (FT_ERR_SUCCESS);
        }
    }
    if ((requested_stage_mask & VOXEL_STAGE_BASE_TERRAIN) != 0U)
    {
        const int32_t error_code = chunk.clear_generated_blocks();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        previous_stage_mask = 0U;
    }
    if (voxel_stage_dependencies_are_met(requested_stage_mask, previous_stage_mask) == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    const game_voxel_generation_metadata generation_metadata = {
        seed_value, world_block_origin_x, world_block_origin_z,
        configuration_signature, previous_stage_mask | requested_stage_mask,
        VOXEL_GENERATOR_VERSION, FT_TRUE
    };
    return (voxel_generate_chunk_stages(chunk, config, callbacks, generation_metadata,
        requested_stage_mask));
}

static int32_t voxel_generate_chunk_snapshot(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const voxel_generation_config &requested_config,
    ft_bool coordinate_seed_overridden, uint64_t coordinate_seed,
    uint32_t requested_stage_mask) noexcept
{
    voxel_generation_config config;
    if (config.initialize(requested_config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_generate_chunk_from_config(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config,
        voxel_get_cross_chunk_callbacks(config), FT_FALSE, 0U,
        coordinate_seed_overridden, coordinate_seed, requested_stage_mask));
}

int32_t voxel_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const voxel_generation_config &config) noexcept
{
    return (voxel_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config, FT_FALSE, 0U,
        VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES
        | VOXEL_STAGE_FLUIDS | VOXEL_STAGE_DECORATION
        | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES));
}

int32_t voxel_generate_chunk_with_stage_mask(
    game_voxel_chunk &chunk, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const voxel_generation_config &config, uint32_t stage_mask) noexcept
{
    return (voxel_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, config, FT_FALSE, 0U, stage_mask));
}

int32_t voxel_generate_chunk_with_context(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const voxel_generation_context &context) noexcept
{
    if (context.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (voxel_generate_chunk_from_config(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, context.config(),
        voxel_get_cross_chunk_callbacks(context.config()), FT_TRUE,
        context.configuration_signature(), FT_FALSE, 0U,
        VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES | VOXEL_STAGE_FLUIDS
        | VOXEL_STAGE_DECORATION | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES));
}

int32_t voxel_generate_chunk_at_world_coordinate(game_voxel_chunk &chunk,
    const voxel_world_chunk_coordinate &coordinate,
    const char *seed_string, const voxel_generation_config &config) noexcept
{
    if (coordinate.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_generate_chunk_snapshot(chunk, 0, 0, seed_string, config,
        FT_TRUE, coordinate.hash(),
        VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES | VOXEL_STAGE_FLUIDS
        | VOXEL_STAGE_DECORATION | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES));
}

int32_t voxel_generate_chunk_in_region(game_voxel_region &region,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const voxel_generation_config &config) noexcept
{
    voxel_generation_context context;

    if (voxel_generation_context_initialize(context, config)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_generate_chunk_in_region_with_context(region,
        world_block_origin_x, world_block_origin_z, seed_string, context));
}

int32_t voxel_generate_chunk_in_region_with_context(
    game_voxel_region &region, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const voxel_generation_context &context) noexcept
{
    game_voxel_chunk *chunk;
    int32_t error_code;

    if (context.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    error_code = region.load_chunk(voxel_floor_div(world_block_origin_x,
            GAME_VOXEL_CHUNK_WIDTH), voxel_floor_div(world_block_origin_z,
            GAME_VOXEL_CHUNK_DEPTH), &chunk);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const voxel_cross_chunk_callbacks callbacks = {
        FT_TRUE, voxel_region_cross_chunk_block_writer, &region,
        voxel_region_cross_chunk_block_reader, &region
    };
    return (voxel_generate_chunk_from_config(*chunk, world_block_origin_x,
        world_block_origin_z, seed_string, context.config(), callbacks, FT_TRUE,
        context.region_configuration_signature(), FT_FALSE, 0U,
        VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES
        | VOXEL_STAGE_FLUIDS | VOXEL_STAGE_DECORATION
        | VOXEL_STAGE_STRUCTURES | VOXEL_STAGE_ORES));
}

#endif

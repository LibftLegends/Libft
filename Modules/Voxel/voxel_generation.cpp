#include <stdint.h>
#include "voxel.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../Game/game_voxel_region.hpp"

static const int32_t TERRAIN_HEIGHTMAP_LARGE_SCALE = 32;
static const int32_t TERRAIN_HEIGHTMAP_DETAIL_SCALE = 8;
static const int32_t TERRAIN_BIOME_ZONE_BLEND_WIDTH = 16;
static const int32_t TERRAIN_HEIGHTMAP_SMOOTH_RADIUS = 1;
static const uint64_t TERRAIN_FEATURE_SHRUB_SALT = UINT64_C(0x2D9C1F4E8B3A6071);
static const int32_t TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET = 1;
static const uint64_t TERRAIN_FEATURE_SHRUB_THRESHOLD = 6U;
static const uint64_t TERRAIN_FEATURE_TREE_SALT = UINT64_C(0x4F1E2D3C5B6A7980);
static const uint64_t TERRAIN_FEATURE_WATER_SALT = UINT64_C(0x9182736455463728);
static const uint64_t TERRAIN_CAVE_PRIMARY_SALT = UINT64_C(0x7C3A91E2D4B8560F);
static const uint64_t TERRAIN_CAVE_DETAIL_SALT = UINT64_C(0x1D6F80B3C9274A55);
static const int32_t TERRAIN_CAVE_PRIMARY_SCALE = 24;
static const int32_t TERRAIN_CAVE_DETAIL_SCALE = 9;
static const int32_t TERRAIN_BEDROCK_FLOOR_Y = 0;
static const int32_t TERRAIN_CAVE_SURFACE_MARGIN = 7;
static const int32_t TERRAIN_CAVE_CELL_SIZE = 12;
static const int32_t TERRAIN_COLUMN_CACHE_COUNT =
    GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH;

struct terrain_column_cache
{
    uint32_t biome;
    terrain_biome_profile biome_profile;
    int32_t column_height;
    uint32_t surface_block_id;
    uint32_t subsurface_block_id;
    uint32_t deep_block_id;
    ft_bool can_place_shrubs;
    ft_bool can_place_trees;
};

static int32_t terrain_sample_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept;

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
                error_code = chunk.write_block(local_x, local_y, local_z,
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

    local_z = 0;
    while (local_z < GAME_VOXEL_CHUNK_DEPTH)
    {
        world_block_z = world_block_origin_z + local_z;
        local_x = 0;
        while (local_x < GAME_VOXEL_CHUNK_WIDTH)
        {
            column_index = (local_z * GAME_VOXEL_CHUNK_WIDTH) + local_x;
            world_block_x = world_block_origin_x + local_x;
            column_cache[column_index].biome = terrain_select_biome(config,
                seed_value, world_block_x, world_block_z);
            column_cache[column_index].biome_profile
                = config.biomes[column_cache[column_index].biome].profile;
            column_cache[column_index].column_height
                = terrain_sample_height(seed_value, world_block_x,
                    world_block_z, config);
            column_cache[column_index].surface_block_id = config.biomes[
                column_cache[column_index].biome].surface_block_id;
            column_cache[column_index].subsurface_block_id = config.biomes[
                column_cache[column_index].biome].subsurface_block_id;
            column_cache[column_index].deep_block_id = config.biomes[
                column_cache[column_index].biome].deep_block_id;
            column_cache[column_index].can_place_shrubs = config.biomes[
                column_cache[column_index].biome].allow_shrubs;
            column_cache[column_index].can_place_trees = config.biomes[
                column_cache[column_index].biome].allow_trees;
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
            error_code = chunk.write_block(target_x, target_y, target_z,
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
    return (region->write_block(world_block_x, world_block_y,
        world_block_z, block_id));
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
        double ridge_noise;

        ridge_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x6A09E667F3BCC909), world_block_x, world_block_z,
            config.mountain_ridge_scale);
        if (ridge_noise < 0.0)
            ridge_noise = -ridge_noise;
        total_noise += (1.0 - ridge_noise)
            * static_cast<double>(config.mountain_ridge_strength);
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

static double terrain_zone_blend_factor(int32_t local_coordinate) noexcept
{
    double blend_factor;

    blend_factor = static_cast<double>(local_coordinate)
        / static_cast<double>(TERRAIN_BIOME_ZONE_BLEND_WIDTH);
    if (blend_factor < 0.0)
        blend_factor = 0.0;
    if (blend_factor > 1.0)
        blend_factor = 1.0;
    return (terrain_smooth_factor(blend_factor));
}

static int32_t terrain_blend_height(int32_t left_height, int32_t right_height,
    double factor) noexcept
{
    return (static_cast<int32_t>(terrain_lerp(
        static_cast<double>(left_height),
        static_cast<double>(right_height), factor)));
}

static int32_t terrain_smooth_biome_height(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_biome_definition &biome_definition,
    const terrain_generation_config &config) noexcept
{
    int32_t biome_zone_x;
    int32_t biome_zone_z;
    int32_t zone_local_x;
    int32_t zone_local_z;
    int32_t height;
    int32_t neighbor_x;
    int32_t neighbor_z;
    uint32_t neighbor_biome;
    terrain_biome_profile neighbor_profile;
    int32_t neighbor_height;
    double blend_factor;

    biome_zone_x = terrain_floor_div(world_block_x, TERRAIN_BIOME_ZONE_WIDTH);
    biome_zone_z = terrain_floor_div(world_block_z, TERRAIN_BIOME_ZONE_WIDTH);
    zone_local_x = world_block_x - (biome_zone_x * TERRAIN_BIOME_ZONE_WIDTH);
    zone_local_z = world_block_z - (biome_zone_z * TERRAIN_BIOME_ZONE_WIDTH);
    height = terrain_column_height(seed_value, world_block_x, world_block_z,
        biome_definition.profile, biome_definition.allow_mountain_ridges,
        config);
    if (config.enable_biome_transitions == FT_TRUE
        && zone_local_x < TERRAIN_BIOME_ZONE_BLEND_WIDTH)
    {
        neighbor_x = world_block_x - TERRAIN_BIOME_ZONE_WIDTH;
        neighbor_biome = terrain_select_biome(config, seed_value, neighbor_x,
            world_block_z);
        neighbor_profile = config.biomes[neighbor_biome].profile;
        neighbor_height = terrain_column_height(seed_value, world_block_x,
            world_block_z, neighbor_profile,
            config.biomes[neighbor_biome].allow_mountain_ridges, config);
        blend_factor = terrain_zone_blend_factor(zone_local_x);
        height = terrain_blend_height(neighbor_height, height, blend_factor);
    }
    else if (config.enable_biome_transitions == FT_TRUE
        && zone_local_x >= (TERRAIN_BIOME_ZONE_WIDTH
            - TERRAIN_BIOME_ZONE_BLEND_WIDTH))
    {
        neighbor_x = world_block_x + TERRAIN_BIOME_ZONE_WIDTH;
        neighbor_biome = terrain_select_biome(config, seed_value, neighbor_x,
            world_block_z);
        neighbor_profile = config.biomes[neighbor_biome].profile;
        neighbor_height = terrain_column_height(seed_value, world_block_x,
            world_block_z, neighbor_profile,
            config.biomes[neighbor_biome].allow_mountain_ridges, config);
        blend_factor = terrain_zone_blend_factor(
            (TERRAIN_BIOME_ZONE_WIDTH - 1) - zone_local_x);
        height = terrain_blend_height(height, neighbor_height, blend_factor);
    }
    if (config.enable_biome_transitions == FT_TRUE
        && zone_local_z < TERRAIN_BIOME_ZONE_BLEND_WIDTH)
    {
        neighbor_z = world_block_z - TERRAIN_BIOME_ZONE_WIDTH;
        neighbor_biome = terrain_select_biome(config, seed_value, world_block_x,
            neighbor_z);
        neighbor_profile = config.biomes[neighbor_biome].profile;
        neighbor_height = terrain_column_height(seed_value, world_block_x,
            world_block_z, neighbor_profile,
            config.biomes[neighbor_biome].allow_mountain_ridges, config);
        blend_factor = terrain_zone_blend_factor(zone_local_z);
        height = terrain_blend_height(neighbor_height, height, blend_factor);
    }
    else if (config.enable_biome_transitions == FT_TRUE
        && zone_local_z >= (TERRAIN_BIOME_ZONE_WIDTH
            - TERRAIN_BIOME_ZONE_BLEND_WIDTH))
    {
        neighbor_z = world_block_z + TERRAIN_BIOME_ZONE_WIDTH;
        neighbor_biome = terrain_select_biome(config, seed_value, world_block_x,
            neighbor_z);
        neighbor_profile = config.biomes[neighbor_biome].profile;
        neighbor_height = terrain_column_height(seed_value, world_block_x,
            world_block_z, neighbor_profile,
            config.biomes[neighbor_biome].allow_mountain_ridges, config);
        blend_factor = terrain_zone_blend_factor(
            (TERRAIN_BIOME_ZONE_WIDTH - 1) - zone_local_z);
        height = terrain_blend_height(height, neighbor_height, blend_factor);
    }
    return (height);
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

static ft_bool terrain_should_fill_water(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    const terrain_generation_config &config) noexcept
{
    double river_noise;
    double lake_noise;

    if (terrain_should_place_feature(seed_value, world_block_x, world_block_z,
            TERRAIN_FEATURE_WATER_SALT, config.water_chance_percent)
        == FT_TRUE)
        return (FT_TRUE);
    if (config.fluids.enable_rivers == FT_TRUE)
    {
        river_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0x3C6EF372FE94F82B), world_block_x, world_block_z,
            config.fluids.river_noise_scale);
        if (river_noise < 0.0)
            river_noise = -river_noise;
        if (river_noise < (0.04 + (static_cast<double>(
                config.fluids.river_width) * 0.01)))
            return (FT_TRUE);
    }
    if (config.fluids.enable_lakes == FT_TRUE)
    {
        lake_noise = terrain_value_noise(seed_value ^ UINT64_C(
            0xA54FF53A5F1D36F1), world_block_x, world_block_z,
            config.fluids.lake_noise_scale);
        if (lake_noise < 0.0)
            lake_noise = -lake_noise;
        if (lake_noise < 0.08
            && terrain_should_place_feature(seed_value, world_block_x,
                world_block_z, TERRAIN_FEATURE_WATER_SALT
                    ^ UINT64_C(0xA11CE), config.fluids.lake_chance_percent)
                == FT_TRUE)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static ft_bool terrain_should_place_ore(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_y, int32_t world_block_z,
    const terrain_ore_rule &ore_rule) noexcept
{
    uint64_t ore_seed;

    if (ore_rule.enabled == FT_FALSE
        || world_block_y < ore_rule.minimum_height
        || world_block_y > ore_rule.maximum_height
        || ore_rule.chance_percent == 0U)
        return (FT_FALSE);
    ore_seed = seed_value ^ static_cast<uint64_t>(world_block_x)
        * UINT64_C(0x9E3779B97F4A7C15)
        ^ static_cast<uint64_t>(world_block_y) * UINT64_C(0xC2B2AE3D27D4EB4F)
        ^ static_cast<uint64_t>(world_block_z) * UINT64_C(0x165667B19E3779F9);
    ore_seed = terrain_mix_u64(ore_seed);
    if ((ore_seed % 100U) >= ore_rule.chance_percent)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t terrain_place_ore_vein(game_voxel_chunk &chunk,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_y,
    int32_t world_block_z, int32_t local_x, int32_t local_y, int32_t local_z,
    const terrain_ore_rule &ore_rule) noexcept
{
    uint32_t vein_index;
    uint64_t vein_seed;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    uint32_t block_id;
    int32_t error_code;

    vein_index = 0U;
    while (vein_index < ore_rule.vein_size)
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
            if (terrain_block_is_solid(block_id) == FT_TRUE
                && block_id != TERRAIN_GENERATOR_BEDROCK_BLOCK)
            {
                error_code = chunk.write_block(target_x, target_y, target_z,
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
    uint32_t block_id;
    int32_t error_code;

    ore_index = 0U;
    while (ore_index < config.ore_rule_count
        && ore_index < TERRAIN_MAX_ORE_RULES)
    {
        if (config.ores[ore_index].enabled == FT_TRUE)
        {
            local_z = 0;
            while (local_z < GAME_VOXEL_CHUNK_DEPTH)
            {
                local_y = config.ores[ore_index].minimum_height;
                while (local_y <= config.ores[ore_index].maximum_height
                    && local_y < GAME_VOXEL_CHUNK_HEIGHT)
                {
                    local_x = 0;
                    while (local_x < GAME_VOXEL_CHUNK_WIDTH)
                    {
                        error_code = chunk.read_block(local_x, local_y,
                            local_z, &block_id);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                        if (terrain_block_is_solid(block_id) == FT_TRUE
                            && block_id != TERRAIN_GENERATOR_BEDROCK_BLOCK
                            && terrain_should_place_ore(seed_value,
                                world_block_origin_x + local_x, local_y,
                                world_block_origin_z + local_z,
                                config.ores[ore_index])
                                == FT_TRUE)
                        {
                            error_code = terrain_place_ore_vein(chunk,
                                seed_value, world_block_origin_x + local_x,
                                local_y, world_block_origin_z + local_z,
                                local_x, local_y, local_z,
                                config.ores[ore_index]);
                            if (error_code != FT_ERR_SUCCESS)
                                return (error_code);
                        }
                        local_x += 1;
                    }
                    local_y += 1;
                }
                local_z += 1;
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
    uint64_t coordinate_seed) noexcept
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
    uint32_t surface_block_id;
    uint32_t subsurface_block_id;
    uint32_t deep_block_id;
    uint64_t seed_value;
    uint32_t configuration_signature;
    ft_bool place_shrub;
    const terrain_tree_template *tree_template;
    uint64_t tree_feature_seed;
    uint32_t feature_index;
    uint64_t feature_seed;
    const terrain_feature_rule *feature_rule;
    uint32_t tree_template_index;
    game_voxel_generation_metadata generation_metadata;
    terrain_column_cache column_cache[TERRAIN_COLUMN_CACHE_COUNT];
    int32_t column_index;
    int32_t feature_margin;

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
    if (chunk.generation_metadata_matches(seed_value, world_block_origin_x,
            world_block_origin_z, configuration_signature) == FT_TRUE
        && chunk.get_generation_metadata().generator_version
            == TERRAIN_GENERATOR_VERSION
        && chunk.get_generation_metadata().completed_stage_mask
            == (TERRAIN_STAGE_BASE_TERRAIN | TERRAIN_STAGE_CAVES
                | TERRAIN_STAGE_FLUIDS | TERRAIN_STAGE_DECORATION
                | TERRAIN_STAGE_STRUCTURES | TERRAIN_STAGE_ORES))
    {
        chunk.clear_dirty();
        return (FT_ERR_SUCCESS);
    }
    error_code = terrain_stage_clear_chunk(chunk);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
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
            surface_block_id = column_cache[column_index].surface_block_id;
            subsurface_block_id = column_cache[column_index].subsurface_block_id;
            deep_block_id = column_cache[column_index].deep_block_id;
            place_shrub = column_cache[column_index].can_place_shrubs;
            column_height = terrain_smooth_heightfield(seed_value,
                world_block_x, world_block_z, config);
            column_cache[column_index].column_height = column_height;
            if (column_height < 0)
                column_height = 0;
            if (column_height >= GAME_VOXEL_CHUNK_HEIGHT)
                column_height = GAME_VOXEL_CHUNK_HEIGHT - 1;
            local_y = 0;
            while (local_y <= column_height)
            {
                if (local_y > TERRAIN_BEDROCK_FLOOR_Y
                    && terrain_should_carve_cave(seed_value, world_block_x,
                        local_y, world_block_z, column_height, config)
                    == FT_TRUE)
                {
                    local_y += 1;
                    continue ;
                }
                if (local_y <= TERRAIN_BEDROCK_FLOOR_Y)
                    block_id = TERRAIN_GENERATOR_BEDROCK_BLOCK;
                else if (local_y == column_height)
                    block_id = surface_block_id;
                else if (local_y >= column_height - biome_profile.topsoil_depth)
                    block_id = subsurface_block_id;
                else
                    block_id = deep_block_id;
                error_code = chunk.write_block(local_x, local_y, local_z,
                    block_id);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                local_y += 1;
            }
            if (config.layers.enable_beaches == FT_TRUE
                && column_height < config.sea_level)
            {
                local_y = column_height;
                while (local_y >= 0 && local_y > column_height
                    - static_cast<int32_t>(config.layers.beach_depth))
                {
                    error_code = chunk.write_block(local_x, local_y, local_z,
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
                    error_code = chunk.write_block(local_x, local_y, local_z,
                        config.layers.underwater_block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y -= 1;
                }
            }
            if (config.layers.enable_snow_caps == FT_TRUE
                && config.biomes[biome].allow_snow_caps == FT_TRUE
                && column_height >= config.layers.snow_cap_minimum_height)
            {
                local_y = column_height;
                while (local_y >= 0 && local_y > column_height
                    - static_cast<int32_t>(config.layers.snow_cap_depth))
                {
                    error_code = chunk.write_block(local_x, local_y, local_z,
                        config.layers.snow_cap_block_id);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y -= 1;
                }
            }
            if (column_height + TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET
                < GAME_VOXEL_CHUNK_HEIGHT
                && place_shrub == FT_TRUE
                && terrain_should_place_feature(seed_value, world_block_x,
                    world_block_z, TERRAIN_FEATURE_SHRUB_SALT,
                    config.biomes[biome].shrub_chance_percent) == FT_TRUE)
            {
                error_code = chunk.write_block(local_x,
                    column_height + TERRAIN_FEATURE_SHRUB_HEIGHT_OFFSET,
                    local_z, TERRAIN_GENERATOR_SHRUB_BLOCK);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
            if (column_height < config.sea_level
                && terrain_should_fill_water(seed_value, world_block_x,
                    world_block_z, config) == FT_TRUE)
            {
                local_y = column_height + 1;
                while (local_y <= config.sea_level
                    && local_y < GAME_VOXEL_CHUNK_HEIGHT)
                {
                    error_code = chunk.write_block(local_x, local_y, local_z,
                        TERRAIN_GENERATOR_WATER_BLOCK);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    local_y += 1;
                }
            }
            local_x += 1;
        }
        local_z += 1;
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
            if (column_cache[column_index].can_place_trees == FT_TRUE)
            {
                tree_feature_seed = terrain_feature_seed(seed_value,
                    world_block_x, world_block_z, TERRAIN_FEATURE_TREE_SALT);
                if ((tree_feature_seed % 100U)
                    < config.biomes[biome].tree_chance_percent)
                {
                    tree_template = config.biomes[biome].tree_template;
                    if (tree_template == ft_nullptr)
                    {
                        if (config.biomes[biome].tree_template_count > 0U)
                        {
                            tree_template_index = config.biomes[biome]
                                .tree_template_indices[tree_feature_seed
                                    % config.biomes[biome]
                                        .tree_template_count];
                            tree_template =
                                terrain_generation_config_get_tree_template(
                                    config, tree_template_index);
                        }
                    }
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
        if (feature_rule->template_data != ft_nullptr)
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
                            || column_height >= config.sea_level))
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
    error_code = terrain_generate_ores(chunk, seed_value,
        world_block_origin_x, world_block_origin_z, config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    chunk.clear_dirty();
    generation_metadata.seed_value = seed_value;
    generation_metadata.world_block_origin_x = world_block_origin_x;
    generation_metadata.world_block_origin_z = world_block_origin_z;
    generation_metadata.configuration_signature = configuration_signature;
    generation_metadata.completed_stage_mask = TERRAIN_STAGE_BASE_TERRAIN
        | TERRAIN_STAGE_CAVES | TERRAIN_STAGE_FLUIDS
        | TERRAIN_STAGE_DECORATION | TERRAIN_STAGE_STRUCTURES
        | TERRAIN_STAGE_ORES;
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
        FT_FALSE, 0U));
}

int32_t terrain_generate_chunk_with_context(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_context &context) noexcept
{
    if (context.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (terrain_generate_chunk_snapshot(chunk, world_block_origin_x,
        world_block_origin_z, seed_string, context.config(), FT_TRUE, FT_TRUE,
        context.configuration_signature(), FT_FALSE, 0U));
}

int32_t terrain_generate_chunk_at_world_coordinate(game_voxel_chunk &chunk,
    const terrain_world_chunk_coordinate &coordinate,
    const char *seed_string, const terrain_generation_config &config) noexcept
{
    if (coordinate.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (terrain_generate_chunk_snapshot(chunk, 0, 0, seed_string, config,
        FT_FALSE, FT_FALSE, 0U, FT_TRUE, coordinate.hash()));
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
    return (terrain_generate_chunk_snapshot(*chunk, world_block_origin_x,
        world_block_origin_z, seed_string, region_config, FT_TRUE, FT_FALSE,
        0U, FT_FALSE, 0U));
}

#endif

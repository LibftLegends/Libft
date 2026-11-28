#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/Game/game_voxel_region.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

struct biome_selector_trace
{
    uint64_t checksum;
    uint64_t calls;
};

static uint32_t trace_biome_selector(uint64_t seed, int32_t x, int32_t z,
    uint32_t biome_count, void *user_data) noexcept
{
    auto *trace = static_cast<biome_selector_trace *>(user_data);
    trace->calls += 1U;
    trace->checksum = (trace->checksum * 31U + static_cast<uint32_t>(x)) * 31U
        + static_cast<uint32_t>(z);
    return (static_cast<uint32_t>((seed + trace->calls) % biome_count));
}

static int32_t measure_generation(const char *label, voxel_generation_config &config,
    uint32_t stage_mask, uint32_t iterations, uint32_t prior_stage_mask = 0U)
{
    constexpr uint32_t BLOCK_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT
        * GAME_VOXEL_CHUNK_DEPTH;
    game_voxel_chunk chunk;
    uint32_t blocks[BLOCK_COUNT];
    if (chunk.initialize() != FT_ERR_SUCCESS)
        return (1);
    double elapsed = 0.0;
    uint64_t checksum = 0U;
    uint64_t water_blocks = 0U;
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        const int32_t x = (static_cast<int32_t>(iteration) - 14) * GAME_VOXEL_CHUNK_WIDTH;
        if (prior_stage_mask != 0U
            && voxel_generate_chunk_with_stage_mask(chunk, x, -64, "terrain-benchmark",
                config, prior_stage_mask) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        if (voxel_generate_chunk_with_stage_mask(chunk, x, -64, "terrain-benchmark",
                config, stage_mask) != FT_ERR_SUCCESS)
            return (1);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        if (chunk.copy_blocks(blocks, BLOCK_COUNT) != FT_ERR_SUCCESS)
            return (1);
        uint32_t index = 0U;
        while (index < BLOCK_COUNT)
        {
            checksum = checksum * 31U + blocks[index];
            if (blocks[index] == VOXEL_GENERATOR_WATER_BLOCK)
                water_blocks += 1U;
            index += 1U;
        }
        iteration += 1U;
    }
    if ((stage_mask & VOXEL_STAGE_FLUIDS) != 0U && water_blocks == 0U)
        return (1);
    std::printf("%s: %.2f ns per build, checksum %" FT_PRIu64 ", water %" FT_PRIu64 "\n",
        label, elapsed / iterations, checksum, water_blocks);
    return (0);
}

static int32_t measure_underground_lakes(const char *label, uint32_t depth)
{
    constexpr uint32_t ITERATIONS = 24U;
    constexpr uint32_t BASE_STAGES = VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES;
    voxel_generation_config config;
    game_voxel_chunk chunk;
    if (voxel_default_generation_config(config) || chunk.initialize()
        || config.set_biome_count(1U) || config.set_biome_height_profile(0U, 100, 0, 0)
        || config.set_water_chance_percent(0U) || config.fluids.set_enabled(FT_FALSE, FT_FALSE)
        || config.underground_structures.set_enabled(FT_FALSE, FT_FALSE)
        || config.fluids.set_underground_lakes_enabled(FT_TRUE)
        || config.fluids.set_underground_lake_settings(100U, 8, 64, depth, 1U, 1U))
        return (1);
    const int32_t floor_y = 18 + static_cast<int32_t>(depth) * 2;
    double elapsed = 0.0;
    uint64_t checksum = 0U;
    uint64_t water_blocks = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        const int32_t origin = static_cast<int32_t>(iteration) * 16;
        if (voxel_generate_chunk_with_stage_mask(chunk, origin, 0, "underground-benchmark",
                config, BASE_STAGES))
            return (1);
        int32_t offset = 0;
        while (offset < 9)
        {
            const int32_t x = 7 + offset % 3;
            const int32_t z = 7 + offset / 3;
            uint32_t layer = 0U;
            while (layer <= depth)
            {
                if (chunk.write_generated_block(x, floor_y + static_cast<int32_t>(layer),
                        z, GAME_VOXEL_AIR_BLOCK))
                    return (1);
                layer += 1U;
            }
            offset += 1;
        }
        if (depth == 1U && chunk.write_generated_block(6, floor_y + 1, 8, GAME_VOXEL_AIR_BLOCK))
            return (1);
        const auto start = std::chrono::steady_clock::now();
        if (voxel_generate_chunk_with_stage_mask(chunk, origin, 0, "underground-benchmark",
                config, VOXEL_STAGE_FLUIDS))
            return (1);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        offset = 0;
        while (offset < 9)
        {
            uint32_t layer = 0U;
            while (layer <= depth)
            {
                uint32_t block_id = 0U;
                if (chunk.read_block(7 + offset % 3, floor_y + static_cast<int32_t>(layer),
                        7 + offset / 3, &block_id))
                    return (1);
                checksum = checksum * 31U + block_id;
                if (block_id == VOXEL_GENERATOR_WATER_BLOCK)
                    water_blocks += 1U;
                layer += 1U;
            }
            offset += 1;
        }
        iteration += 1U;
    }
    if (water_blocks != ITERATIONS * 9U * depth)
        return (1);
    std::printf("%s: %.2f ns per build, checksum %" FT_PRIu64 ", water %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS, checksum, water_blocks);
    return (0);
}

static int32_t measure_fresh_region_chunks(game_voxel_region &region,
    const voxel_generation_context &context, const char *seed)
{
    constexpr uint32_t CHUNK_COUNT = 20U;
    constexpr uint32_t BLOCK_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT
        * GAME_VOXEL_CHUNK_DEPTH;
    constexpr int32_t CHUNK_Z = 3;
    uint32_t blocks[BLOCK_COUNT];
    uint32_t chunk_index = 0U;
    uint64_t checksum = 0U;
    double elapsed = 0.0;
    while (chunk_index < CHUNK_COUNT)
    {
        const int32_t chunk_x = static_cast<int32_t>(chunk_index) + 1;
        const auto started = std::chrono::steady_clock::now();
        if (voxel_generate_chunk_in_region_with_context(region,
                chunk_x * GAME_VOXEL_CHUNK_WIDTH, CHUNK_Z * GAME_VOXEL_CHUNK_DEPTH,
                seed, context) != FT_ERR_SUCCESS)
            return (1);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - started).count();
        game_voxel_chunk *chunk;
        if (region.load_chunk(chunk_x, CHUNK_Z, &chunk) != FT_ERR_SUCCESS
            || chunk->copy_blocks(blocks, BLOCK_COUNT) != FT_ERR_SUCCESS)
            return (1);
        uint32_t block_index = 0U;
        while (block_index < BLOCK_COUNT)
        {
            checksum = checksum * 31U + blocks[block_index];
            block_index += 1U;
        }
        chunk_index += 1U;
    }
    std::printf("fresh region chunks: %.2f ns per build, checksum %" FT_PRIu64 "\n",
        elapsed / CHUNK_COUNT, checksum);
    return (0);
}

static int32_t measure_context_cache_hits(const voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 20000U;
    constexpr int32_t ORIGIN = GAME_VOXEL_CHUNK_WIDTH;
    constexpr uint32_t BLOCK_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT
        * GAME_VOXEL_CHUNK_DEPTH;
    constexpr const char *SEED = "context-benchmark";
    voxel_generation_context context;
    game_voxel_chunk chunk;
    game_voxel_region region;
    if (context.initialize(config) != FT_ERR_SUCCESS || chunk.initialize() != FT_ERR_SUCCESS
        || region.initialize(0, 0, ".") != FT_ERR_SUCCESS
        || voxel_generate_chunk_with_context(chunk, ORIGIN, ORIGIN, SEED, context)
            != FT_ERR_SUCCESS
        || voxel_generate_chunk_in_region_with_context(region, ORIGIN, ORIGIN, SEED, context)
            != FT_ERR_SUCCESS
        || measure_fresh_region_chunks(region, context, SEED) != 0)
        return (1);
    uint32_t index = 0U;
    const auto chunk_started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generate_chunk_with_context(chunk, ORIGIN, ORIGIN, SEED, context)
            != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double chunk_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - chunk_started).count();
    index = 0U;
    const auto region_started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generate_chunk_in_region_with_context(region, ORIGIN, ORIGIN, SEED, context)
            != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double region_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - region_started).count();
    game_voxel_chunk *region_chunk;
    uint32_t blocks[BLOCK_COUNT];
    if (region.load_chunk(1, 1, &region_chunk) != FT_ERR_SUCCESS
        || region_chunk->copy_blocks(blocks, BLOCK_COUNT) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    index = 0U;
    while (index < BLOCK_COUNT)
    {
        checksum = checksum * 31U + blocks[index];
        index += 1U;
    }
    std::printf("cached chunk context: %.2f ns/call, signature %u\n",
        chunk_elapsed / ITERATIONS, chunk.get_generation_metadata().configuration_signature);
    std::printf("cached region context: %.2f ns/call, signature %u, checksum %" FT_PRIu64 "\n",
        region_elapsed / ITERATIONS, region_chunk->get_generation_metadata().configuration_signature,
        checksum);
    return (0);
}

int32_t main()
{
    constexpr uint32_t CAVE_STAGES = VOXEL_STAGE_BASE_TERRAIN | VOXEL_STAGE_CAVES;
    constexpr uint32_t FLUID_STAGES = CAVE_STAGES | VOXEL_STAGE_FLUIDS;
    voxel_generation_config config;
    if (voxel_default_generation_config(config) != FT_ERR_SUCCESS
        || measure_generation("base terrain", config, VOXEL_STAGE_BASE_TERRAIN, 28U)
        || measure_generation("default caves", config, CAVE_STAGES, 20U)
        || measure_generation("default ores", config, CAVE_STAGES | VOXEL_STAGE_ORES, 20U)
        || measure_context_cache_hits(config)
        || config.underground_structures.set_chances(25U, 55U) != FT_ERR_SUCCESS
        || config.underground_structures.set_cavern_rooms(FT_TRUE, 40U, 15U) != FT_ERR_SUCCESS
        || measure_generation("dense caves", config, CAVE_STAGES, 20U))
        return (1);
    if (config.underground_structures.set_enabled(FT_FALSE, FT_TRUE) != FT_ERR_SUCCESS
        || config.underground_structures.set_chances(0U, 100U) != FT_ERR_SUCCESS
        || config.underground_structures.set_height_range(-16, 240) != FT_ERR_SUCCESS
        || config.underground_structures.set_cavern_rooms(FT_TRUE, 100U, 32U) != FT_ERR_SUCCESS
        || config.underground_structures.set_cave_entrances(100U, 8U) != FT_ERR_SUCCESS
        || measure_generation("wide caves and entrances", config, CAVE_STAGES, 24U))
        return (1);
    game_voxel_chunk chunk;
    biome_selector_trace trace = {};
    if (chunk.initialize() != FT_ERR_SUCCESS
        || config.set_biome_selector(trace_biome_selector, &trace) != FT_ERR_SUCCESS
        || voxel_generate_chunk_with_stage_mask(chunk, -16, 16, "terrain-selector",
            config, VOXEL_STAGE_BASE_TERRAIN) != FT_ERR_SUCCESS)
        return (1);
    std::printf("custom selector: %" FT_PRIu64 " calls, trace %" FT_PRIu64 "\n",
        trace.calls, trace.checksum);
    voxel_generation_config wet;
    if (voxel_default_generation_config(wet) != FT_ERR_SUCCESS
        || wet.set_biome_count(1U) != FT_ERR_SUCCESS
        || wet.set_sea_level(50) != FT_ERR_SUCCESS
        || wet.set_biome_height_profile(0U, 40, 0, 0) != FT_ERR_SUCCESS
        || wet.fluids.set_enabled(FT_TRUE, FT_FALSE) != FT_ERR_SUCCESS
        || wet.fluids.set_river_settings(96, 100) != FT_ERR_SUCCESS
        || wet.fluids.set_surface_water_depths(16U, 16U) != FT_ERR_SUCCESS
        || wet.fluids.set_underground_lakes_enabled(FT_FALSE) != FT_ERR_SUCCESS
        || wet.underground_structures.set_enabled(FT_FALSE, FT_FALSE) != FT_ERR_SUCCESS
        || wet.layers.set_enabled(FT_FALSE, FT_FALSE) != FT_ERR_SUCCESS
        || measure_generation("surface water", wet, CAVE_STAGES | VOXEL_STAGE_FLUIDS, 20U)
        || measure_underground_lakes("shallow underground lake", 1U)
        || measure_underground_lakes("deep underground lake", 2U))
        return (1);
    voxel_generation_config snowy;
    if (voxel_default_generation_config(snowy) != FT_ERR_SUCCESS
        || snowy.set_biome_count(1U) != FT_ERR_SUCCESS
        || snowy.set_biome_height_profile(0U, 95, 0, 0) != FT_ERR_SUCCESS
        || snowy.set_biome_snow_caps_enabled(0U, FT_TRUE) != FT_ERR_SUCCESS
        || snowy.fluids.set_enabled(FT_FALSE, FT_FALSE) != FT_ERR_SUCCESS
        || snowy.fluids.set_underground_lakes_enabled(FT_FALSE) != FT_ERR_SUCCESS
        || snowy.layers.set_enabled(FT_FALSE, FT_TRUE) != FT_ERR_SUCCESS
        || measure_generation("default depth snow", snowy, VOXEL_STAGE_DECORATION,
            32U, FLUID_STAGES)
        || snowy.layers.set_depths(0U, 0U, 24U) != FT_ERR_SUCCESS
        || measure_generation("deep snow", snowy, VOXEL_STAGE_DECORATION, 32U, FLUID_STAGES))
        return (1);
    if (snowy.set_feature_count(VOXEL_MAX_FEATURE_RULES) != FT_ERR_SUCCESS)
        return (1);
    uint32_t rule_index = 0U;
    while (rule_index < VOXEL_MAX_FEATURE_RULES)
    {
        if (snowy.features[rule_index].set_template(&voxel_small_cactus_tree_template())
                != FT_ERR_SUCCESS
            || snowy.features[rule_index].set_biome_range(-1, 0, GAME_VOXEL_CHUNK_HEIGHT)
                != FT_ERR_SUCCESS
            || snowy.features[rule_index].set_chance(0U) != FT_ERR_SUCCESS)
            return (1);
        rule_index += 1U;
    }
    voxel_feature_rule &oak_rule = snowy.features[VOXEL_MAX_FEATURE_RULES - 1U];
    if (oak_rule.set_template(&voxel_large_oak_tree_template()) != FT_ERR_SUCCESS
        || oak_rule.set_chance(100U) != FT_ERR_SUCCESS
        || measure_generation("configured structures", snowy, VOXEL_STAGE_STRUCTURES,
            32U, CAVE_STAGES))
        return (1);
    return (0);
}

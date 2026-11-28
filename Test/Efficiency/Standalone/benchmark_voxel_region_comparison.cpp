#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Game/game_voxel_region.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>

int32_t baseline_voxel_generate_chunk_in_region_with_context(game_voxel_region &region,
    int32_t x, int32_t z, const char *seed, const voxel_generation_context &context) noexcept;
int32_t context_voxel_generate_chunk_in_region_with_context(game_voxel_region &region,
    int32_t x, int32_t z, const char *seed, const voxel_generation_context &context) noexcept;

using generation_function = decltype(&voxel_generate_chunk_in_region_with_context);

int32_t context_voxel_generate_chunk_with_stage_mask(game_voxel_chunk &chunk,
    int32_t x, int32_t z, const char *seed, const voxel_generation_config &config,
    uint32_t stages) noexcept;

static int32_t measure_base_terrain(const voxel_generation_config &config)
{
    constexpr uint32_t ROUNDS = 17U;
    constexpr uint32_t CHUNKS = 24U;
    constexpr uint32_t BLOCK_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT
        * GAME_VOXEL_CHUNK_DEPTH;
    decltype(&voxel_generate_chunk_with_stage_mask) operations[] = {
        context_voxel_generate_chunk_with_stage_mask, voxel_generate_chunk_with_stage_mask
    };
    game_voxel_chunk chunks[2];
    uint32_t blocks[2][BLOCK_COUNT];
    double ratios[ROUNDS];
    if (chunks[0].initialize() != FT_ERR_SUCCESS || chunks[1].initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t round = 0U;
    while (round <= ROUNDS)
    {
        double total[2] = {};
        uint32_t chunk_index = 0U;
        while (chunk_index < CHUNKS)
        {
            uint32_t operation = 0U;
            while (operation < 2U)
            {
                const uint32_t index = (operation + round + chunk_index) % 2U;
                const auto started = std::chrono::steady_clock::now();
                if (operations[index](chunks[index],
                        (static_cast<int32_t>(chunk_index) - 12) * GAME_VOXEL_CHUNK_WIDTH,
                        -64, "terrain-benchmark", config, VOXEL_STAGE_BASE_TERRAIN) != FT_ERR_SUCCESS)
                    return (1);
                total[index] += std::chrono::duration<double, std::nano>(
                    std::chrono::steady_clock::now() - started).count();
                if (chunks[index].copy_blocks(blocks[index], BLOCK_COUNT) != FT_ERR_SUCCESS)
                    return (1);
                operation += 1U;
            }
            if (!std::equal(blocks[0], blocks[0] + BLOCK_COUNT, blocks[1]))
                return (1);
            chunk_index += 1U;
        }
        if (round != 0U)
            ratios[round - 1U] = total[1] / total[0];
        round += 1U;
    }
    std::sort(ratios, ratios + ROUNDS);
    std::printf("base-only paired ratio %.4f (%.4f-%.4f), all blocks matched\n",
        ratios[ROUNDS / 2U], ratios[0], ratios[ROUNDS - 1U]);
    return (0);
}

static int32_t run(generation_function generate, game_voxel_region &region,
    const voxel_generation_context &context, int32_t x, double &elapsed,
    uint64_t &checksum)
{
    constexpr int32_t CHUNK_Z = 3;
    constexpr uint32_t BLOCK_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT * GAME_VOXEL_CHUNK_DEPTH;
    const auto start = std::chrono::steady_clock::now();
    if (generate(region, x * GAME_VOXEL_CHUNK_WIDTH, CHUNK_Z * GAME_VOXEL_CHUNK_DEPTH,
            "context-benchmark", context) != FT_ERR_SUCCESS)
        return (1);
    elapsed += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now()
        - start).count();
    game_voxel_chunk *chunk;
    uint32_t blocks[BLOCK_COUNT];
    if (region.load_chunk(x, CHUNK_Z, &chunk) != FT_ERR_SUCCESS
        || chunk->copy_blocks(blocks, BLOCK_COUNT) != FT_ERR_SUCCESS)
        return (1);
    checksum = checksum * 31U + chunk->get_generation_metadata().configuration_signature;
    uint32_t index = 0U;
    while (index < BLOCK_COUNT)
    {
        checksum = checksum * 31U + blocks[index];
        index += 1U;
    }
    return (0);
}

int32_t main()
{
    constexpr uint32_t PAIRS = 17U;
    constexpr int32_t CHUNKS = 24;
    constexpr uint32_t IMPLEMENTATIONS = 3U;
    constexpr uint32_t CACHE_HITS = 10000U;
    voxel_generation_config config;
    voxel_generation_context context;
    if (voxel_default_generation_config(config) != FT_ERR_SUCCESS
        || context.initialize(config) != FT_ERR_SUCCESS)
        return (1);
    generation_function operations[IMPLEMENTATIONS] = {
        baseline_voxel_generate_chunk_in_region_with_context,
        context_voxel_generate_chunk_in_region_with_context,
        voxel_generate_chunk_in_region_with_context
    };
    double times[IMPLEMENTATIONS][PAIRS];
    double ratios[IMPLEMENTATIONS][PAIRS];
    double cache_times[IMPLEMENTATIONS][PAIRS];
    uint32_t pair = 0U;
    while (pair <= PAIRS)
    {
        game_voxel_region regions[IMPLEMENTATIONS];
        double elapsed[IMPLEMENTATIONS] = {};
        uint64_t checksums[IMPLEMENTATIONS] = {};
        uint32_t implementation = 0U;
        while (implementation < IMPLEMENTATIONS)
        {
            if (regions[implementation].initialize(0, 0, ".") != FT_ERR_SUCCESS)
                return (1);
            implementation += 1U;
        }
        int32_t x = 1;
        while (x <= CHUNKS)
        {
            implementation = 0U;
            while (implementation < IMPLEMENTATIONS)
            {
                const uint32_t index = (implementation + pair + static_cast<uint32_t>(x))
                    % IMPLEMENTATIONS;
                if (run(operations[index], regions[index], context, x, elapsed[index], checksums[index]))
                    return (1);
                implementation += 1U;
            }
            x += 1;
        }
        if (checksums[0] != checksums[1] || checksums[0] != checksums[2])
            return (1);
        if (pair > 0U)
        {
            implementation = 0U;
            while (implementation < IMPLEMENTATIONS)
            {
                times[implementation][pair - 1U] = elapsed[implementation] / CHUNKS / 1000000.0;
                ratios[implementation][pair - 1U] = elapsed[implementation] / elapsed[0];
                const uint32_t index = (implementation + pair) % IMPLEMENTATIONS;
                const auto start = std::chrono::steady_clock::now();
                uint32_t hit = 0U;
                while (hit < CACHE_HITS)
                {
                    if (operations[index](regions[index], GAME_VOXEL_CHUNK_WIDTH,
                            3 * GAME_VOXEL_CHUNK_DEPTH, "context-benchmark", context)
                        != FT_ERR_SUCCESS)
                        return (1);
                    hit += 1U;
                }
                cache_times[index][pair - 1U] = std::chrono::duration<double, std::nano>(
                    std::chrono::steady_clock::now() - start).count() / CACHE_HITS;
                implementation += 1U;
            }
        }
        pair += 1U;
    }
    const char *labels[IMPLEMENTATIONS] = {"baseline", "context reuse", "current"};
    uint32_t implementation = 0U;
    while (implementation < IMPLEMENTATIONS)
    {
        std::sort(times[implementation], times[implementation] + PAIRS);
        std::sort(ratios[implementation], ratios[implementation] + PAIRS);
        std::sort(cache_times[implementation], cache_times[implementation] + PAIRS);
        std::printf("%s fresh %.3f ms, paired ratio %.4f (%.4f-%.4f), cached %.2f ns\n",
            labels[implementation], times[implementation][PAIRS / 2U],
            ratios[implementation][PAIRS / 2U], ratios[implementation][0],
            ratios[implementation][PAIRS - 1U], cache_times[implementation][PAIRS / 2U]);
        implementation += 1U;
    }
    std::puts("All fresh chunk checksums and signatures matched.");
    return (measure_base_terrain(config));
}

#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_toggle()
{
    constexpr uint32_t CYCLES = 16384U;
    game_voxel_chunk_section section;
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (section.initialize() != FT_ERR_SUCCESS
        || cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t cycle = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (cycle < CYCLES)
    {
        const uint16_t index = static_cast<uint16_t>(cycle % GAME_VOXEL_SECTION_BLOCKS);
        if (section.set_block(index, 42U) != FT_ERR_SUCCESS)
            return (1);
        checksum += section.get_block(index);
        if (section.set_block(index, GAME_VOXEL_AIR_BLOCK) != FT_ERR_SUCCESS
            || !section.is_uniform())
            return (1);
        ++cycle;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != 42U * CYCLES || cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("toggle %.2f ns/cycle allocations=%" PRIu64 " checksum=%" PRIu64 "\n",
        elapsed / CYCLES, ft_platform_cast<uint64_t>(after - before), checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--toggle") == 0)
        return (measure_toggle());
    constexpr uint32_t passes = 32U;
    game_voxel_chunk_section section;
    if (section.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t pass = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (pass < passes)
    {
        const uint32_t value = 1U + (pass & 1U);
        uint16_t index = 0U;
        while (index < GAME_VOXEL_SECTION_BLOCKS)
        {
            if (section.set_block(index, value) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        if (section.is_uniform() != FT_TRUE || section.get_uniform_block() != value)
            return (1);
        checksum += section.get_uniform_block();
        ++pass;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("uniform section rewrite %.2f us/section checksum=%" PRIu64 "\n",
        elapsed / passes, checksum);
    return (0);
}

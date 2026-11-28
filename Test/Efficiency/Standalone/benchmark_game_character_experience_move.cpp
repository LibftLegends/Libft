#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int32_t main(int32_t argc, char **argv)
{
    constexpr int32_t ITERATIONS = 512;
    constexpr int32_t LARGE_LEVEL_COUNT = 1024;
    int32_t level_count = 2;
    if (argc > 1 && ft_strcmp(argv[1], "--large") == 0)
        level_count = LARGE_LEVEL_COUNT;
    int32_t levels[LARGE_LEVEL_COUNT];
    int32_t level = 0;
    while (level < level_count)
    {
        levels[level] = (level - 1) * 25;
        ++level;
    }
    double elapsed = 0.0;
    uint64_t allocations = 0;
    int64_t checksum = 0;
    int32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        game_character source;
        game_character destination;
        if (source.initialize() != FT_ERR_SUCCESS)
            return (1);
        game_experience_table &table = source.get_experience_table();
        if (table.set_levels(levels, level_count) != FT_ERR_SUCCESS)
            return (1);
        ft_size_t before;
        ft_size_t after;
        ft_size_t frees;
        if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = destination.move(source);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS)
        {
            std::fprintf(stderr, "character_move failed for %" PRId32 " levels: %" PRId32 "\n",
                level_count, result);
            return (1);
        }
        if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
            return (1);
        const game_experience_table &moved = destination.get_experience_table();
        if (moved.get_count() != level_count
            || moved.get_value(level_count - 1) != levels[level_count - 1])
            return (1);
        checksum += moved.get_level(27);
        if (moved.get_error() != FT_ERR_SUCCESS)
            return (1);
        allocations += after - before;
        ++iteration;
    }
    std::printf("character_move %.3f %" PRIu64 " %" PRId64 "\n",
        elapsed / ITERATIONS, allocations, checksum);
    return (0);
}

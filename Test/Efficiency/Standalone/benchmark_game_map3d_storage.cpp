#include "../../../Modules/Game/game_map3d.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t WIDTH = 48U;
    constexpr ft_size_t HEIGHT = 32U;
    constexpr ft_size_t DEPTH = 16U;
    constexpr uint32_t SAMPLES = 160U;
    ft_size_t allocations_before;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0U;
    const auto creation_start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        game_map3d grid;
        if (grid.initialize(WIDTH, HEIGHT, DEPTH, 37) != FT_ERR_SUCCESS
            || grid.get(WIDTH - 1U, HEIGHT - 1U, DEPTH - 1U) != 37)
            return (1);
        ++sample;
    }
    const double creation_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - creation_start).count() / SAMPLES;
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    const ft_size_t creation_allocations = (allocations_after - allocations_before) / SAMPLES;

    game_map3d grid;
    if (grid.initialize(WIDTH, HEIGHT, DEPTH, 37) != FT_ERR_SUCCESS
        || cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    const auto resize_start = std::chrono::steady_clock::now();
    sample = 0U;
    while (sample < SAMPLES)
    {
        grid.resize(WIDTH, HEIGHT, DEPTH, static_cast<int32_t>(sample & 1U));
        if (grid.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double resize_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - resize_start).count() / SAMPLES;
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    const ft_size_t resize_allocations = (allocations_after - allocations_before) / SAMPLES;
    grid.set(WIDTH - 1U, HEIGHT - 1U, DEPTH - 1U, 79);
    const auto read_start = std::chrono::steady_clock::now();
    uint64_t checksum = 0U;
    sample = 0U;
    constexpr uint32_t READS = 1200000U;
    while (sample < READS)
    {
        const ft_size_t index = sample % (WIDTH * HEIGHT * DEPTH);
        checksum += grid.get(index % WIDTH, index / WIDTH % HEIGHT, index / (WIDTH * HEIGHT));
        ++sample;
    }
    const double read_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - read_start).count() / READS;
    std::printf("create=%.3f resize=%.3f read=%.3f create_allocs=%" PRIu64
        " resize_allocs=%" PRIu64 " checksum=%" PRIu64 "\n", creation_us, resize_us,
        read_ns, static_cast<uint64_t>(creation_allocations),
        static_cast<uint64_t>(resize_allocations), checksum);
    return (0);
}

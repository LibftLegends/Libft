#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/DUMB/sound_device.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 10000;
    const ft_bool fresh = argument_count == 2 && ft_strcmp(arguments[1], "--fresh") == 0;
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0;
    uint32_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_sound_clip source;
        ft_sound_clip destination;
        if (source.initialize() != FT_ERR_SUCCESS
            || (!fresh && destination.initialize() != FT_ERR_SUCCESS)
            || destination.initialize(static_cast<ft_sound_clip &&>(source)) != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + static_cast<uint32_t>(destination.get_spec()->freq)
            + static_cast<uint32_t>(source.get_spec()->freq);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("clip %.3f us/move allocations=%" PRIu64 " checksum=%" PRIu64 "\n",
        elapsed / ITERATIONS, ft_platform_cast<uint64_t>(after - before), checksum);
    return (0);
}

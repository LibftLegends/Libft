#include "../../../Modules/Game/game_world_region.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_world_region(int32_t count)
{
    constexpr uint32_t ITERATIONS = 1000U;
    game_world_region source;
    game_world_region target;
    ft_vector<int32_t> alternate_ids;
    if (source.initialize() != FT_ERR_SUCCESS || target.initialize() != FT_ERR_SUCCESS
        || alternate_ids.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t identifier = 1;
    ft_vector<int32_t> &source_ids = source.get_region_ids();
    while (identifier <= count)
    {
        if (source_ids.push_back(identifier) != FT_ERR_SUCCESS
            || alternate_ids.push_back(identifier + 1) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto copy_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        game_world_region copy;
        if (copy.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        checksum += copy.get_region_ids()[count - 1];
        ++index;
    }
    const double copy_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - copy_start).count();

    target.set_region_ids(source_ids);
    if (target.get_error() != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto update_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        const ft_vector<int32_t> *ids = &source_ids;
        if ((index & 1U) != 0U)
            ids = &alternate_ids;
        target.set_region_ids(*ids);
        if (target.get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum += target.get_region_ids()[count - 1];
        ++index;
    }
    const double update_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - update_start).count();
    if (checksum != static_cast<uint64_t>(count) * ITERATIONS * 2U + ITERATIONS / 2U)
        return (1);
    std::printf("ids_%" PRId32 " %.3f %.3f %" PRIu64 "\n", count,
        copy_elapsed / ITERATIONS, update_elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_world_region(8) != 0)
        return (1);
    return (measure_world_region(256));
}

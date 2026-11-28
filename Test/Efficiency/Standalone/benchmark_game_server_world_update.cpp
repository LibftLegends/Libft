#include "../../../Modules/Game/game_server.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 200U;
    game_server server;
    ft_sharedptr<game_world> world(new game_world());
    if (!world || world->initialize() != FT_ERR_SUCCESS
        || server.initialize(world) != FT_ERR_SUCCESS)
        return (1);
    int32_t index = 0;
    while (index < 16)
    {
        ft_sharedptr<game_event> event(new game_event());
        if (!event || event->initialize() != FT_ERR_SUCCESS)
            return (1);
        event->set_id(index);
        event->set_duration(index + 1);
        world->schedule_event(event);
        if (world->get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    ft_size_t allocations_before;
    ft_size_t frees;
    ft_size_t live_before;
    ft_size_t peak;
    if (cma_get_extended_stats(&allocations_before, &frees, &live_before, &peak) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_string output;
        if (output.initialize() != FT_ERR_SUCCESS
            || server.serialize_world_locked(output) != FT_ERR_SUCCESS)
            return (1);
        checksum += output.size();
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t live_after;
    ft_size_t allocations_after;
    if (cma_get_extended_stats(&allocations_after, &frees, &live_after, &peak) != FT_ERR_SUCCESS
        || checksum == 0U)
        return (1);
    std::printf("world_update %.3f %" PRId64 " %" PRIu64 " %" PRIu64 "\n", elapsed / ITERATIONS,
        static_cast<int64_t>(live_after) - static_cast<int64_t>(live_before), checksum,
        allocations_after - allocations_before);
    return (0);
}

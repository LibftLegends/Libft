#include "../../../Modules/Game/game_server.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr int32_t CLIENT_COUNT = 64;
    constexpr uint32_t ITERATIONS = 1000U;
    game_server first;
    game_server second;
    ft_sharedptr<game_world> world;
    if (world.initialize() != FT_ERR_SUCCESS || first.initialize(world, "test-token") != FT_ERR_SUCCESS)
        return (1);
    int32_t client_identifier = 0;
    while (client_identifier < CLIENT_COUNT)
    {
        (void)first.join_client_locked(client_identifier, -1);
        if (first.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++client_identifier;
    }
    ft_size_t allocations_before;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    game_server *source = &first;
    game_server *destination = &second;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_server *previous = source;
        source = destination;
        destination = previous;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t allocations_after;
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || source->_clients.size() != static_cast<ft_size_t>(CLIENT_COUNT)
        || source->_auth_token != "test-token")
        return (1);
    std::printf("server_move %.3f %" PRIu64 "\n", elapsed / ITERATIONS,
        static_cast<uint64_t>(allocations_after - allocations_before));
    return (0);
}

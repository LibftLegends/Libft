#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    ft_socket_handle socket;
    if (socket.initialize() != FT_ERR_SUCCESS
        || !socket.reset(nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) || !socket.is_valid())
        return (1);
    event_loop loop;
    if (event_loop_initialize(&loop) != FT_ERR_SUCCESS)
        return (1);
    if (event_loop_add_interest(&loop, socket.get(), EVENT_LOOP_INTEREST_WRITE) != FT_ERR_SUCCESS)
    {
        event_loop_destroy(&loop);
        return (1);
    }
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    ft_size_t frees = 0;
    const int32_t before_result = cma_get_stats(&allocations_before, &frees);
    constexpr uint32_t ITERATIONS = 2000;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && event_loop_run(&loop, 0) == 1)
        ++iteration;
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t after_result = cma_get_stats(&allocations_after, &frees);
    event_loop_destroy(&loop);
    if (iteration != ITERATIONS || before_result != FT_ERR_SUCCESS || after_result != FT_ERR_SUCCESS)
        return (1);
    std::printf("%" FT_PRIu64 " allocations, %.2f us per wait (%u waits)\n",
        allocations_after - allocations_before, elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_count(uint32_t socket_count, uint32_t interest_mask)
{
    constexpr uint32_t SOCKET_CAPACITY = 64U;
    ft_socket_handle sockets[SOCKET_CAPACITY];
    event_loop loop;
    if (event_loop_initialize(&loop) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < socket_count)
    {
        ft_socket_handle &socket = sockets[index];
        if (socket.initialize() != FT_ERR_SUCCESS
            || !socket.reset(nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) || !socket.is_valid()
            || event_loop_add_interest(&loop, socket.get(), interest_mask) != FT_ERR_SUCCESS)
        {
            event_loop_destroy(&loop);
            return (1);
        }
        ++index;
    }
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    ft_size_t frees = 0;
    const int32_t before_result = cma_get_stats(&allocations_before, &frees);
    constexpr uint32_t ITERATIONS = 2000;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS
        && event_loop_run(&loop, 0) == static_cast<int32_t>(socket_count))
        ++iteration;
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t after_result = cma_get_stats(&allocations_after, &frees);
    event_loop_destroy(&loop);
    if (iteration != ITERATIONS || before_result != FT_ERR_SUCCESS || after_result != FT_ERR_SUCCESS)
        return (1);
    std::printf("%u ready sockets: %" FT_PRIu64 " allocations, %.2f us per wait (%u waits)\n",
        socket_count, allocations_after - allocations_before, elapsed_us / ITERATIONS,
        ITERATIONS);
    return (0);
}

int main()
{
    if (measure_count(1U, EVENT_LOOP_INTEREST_WRITE) != 0
        || measure_count(48U, EVENT_LOOP_INTEREST_WRITE) != 0
        || measure_count(64U, EVENT_LOOP_INTEREST_READ | EVENT_LOOP_INTEREST_WRITE) != 0)
        return (1);
    return (0);
}

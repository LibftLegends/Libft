#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t BASE_COUNT = 48;
    constexpr uint32_t ITERATIONS = 1000;
    constexpr uint32_t INTERESTS = EVENT_LOOP_INTEREST_READ | EVENT_LOOP_INTEREST_WRITE;
    event_loop loop;
    if (event_loop_initialize(&loop) != FT_ERR_SUCCESS)
        return (1);
    int32_t sockets[BASE_COUNT + 1];
    uint32_t opened = 0;
    while (opened < BASE_COUNT + 1)
    {
        const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor < 0)
            break ;
        sockets[opened++] = descriptor;
        if (opened <= BASE_COUNT
            && event_loop_add_interest(&loop, descriptor, INTERESTS) != FT_ERR_SUCCESS)
            break ;
    }
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    const int32_t before_error = cma_get_stats(&before, &frees);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    if (opened == BASE_COUNT + 1)
    {
        const int32_t descriptor = sockets[BASE_COUNT];
        while (iteration < ITERATIONS
            && event_loop_add_interest(&loop, descriptor, INTERESTS) == FT_ERR_SUCCESS
            && event_loop_remove_interest(&loop, descriptor, INTERESTS) == FT_ERR_SUCCESS)
            ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t after_error = cma_get_stats(&after, &frees);
    event_loop_destroy(&loop);
    while (opened > 0)
        nw_close(sockets[--opened]);
    if (iteration != ITERATIONS || before_error != FT_ERR_SUCCESS || after_error != FT_ERR_SUCCESS)
        return (1);
    std::printf("%" FT_PRIu64 " allocations, %.2f us per add/remove (%u iterations)\n",
        after - before, elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

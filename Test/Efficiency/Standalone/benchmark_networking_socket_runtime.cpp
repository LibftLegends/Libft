#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_socket_runtime(ft_bool keep_runtime_active)
{
    constexpr uint32_t ITERATIONS = 5000;
    if (keep_runtime_active && ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor < 0 || nw_close(descriptor) != 0)
            break ;
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (keep_runtime_active)
        ft_socket_runtime_release();
    if (iteration != ITERATIONS)
        return (1);
    const char *mode = "per-socket runtime";
    if (keep_runtime_active)
        mode = "shared runtime";
    std::printf("%s: %.2f us/open-close\n", mode, elapsed_us / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (benchmark_socket_runtime(FT_FALSE) != 0 || benchmark_socket_runtime(FT_TRUE) != 0)
        return (1);
    return (0);
}

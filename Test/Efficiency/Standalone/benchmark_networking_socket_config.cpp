#include "../../../Modules/Networking/networking.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_default()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    SocketConfig configuration;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (configuration.initialize() != FT_ERR_SUCCESS
            || configuration.destroy() != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%.2f ns per default socket configuration initialize/destroy\n",
        elapsed_ns / ITERATIONS);
    return (0);
}

static int32_t measure_copy()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    SocketConfig source;
    SocketConfig destination;
    if (source.initialize() != FT_ERR_SUCCESS
        || destination.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_strlcpy(source._multicast_group, "239.0.0.1", sizeof(source._multicast_group));
    ft_strlcpy(source._multicast_interface, "192.168.1.20",
        sizeof(source._multicast_interface));
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (destination.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (ft_strcmp(source._ip, destination._ip) != 0
        || ft_strcmp(source._multicast_group, destination._multicast_group) != 0
        || ft_strcmp(source._multicast_interface, destination._multicast_interface) != 0)
        return (1);
    std::printf("%.2f ns per populated socket configuration copy\n",
        elapsed_ns / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_default() != 0 || measure_copy() != 0)
        return (1);
    return (0);
}

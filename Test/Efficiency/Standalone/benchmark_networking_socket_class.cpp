#include "../../../Modules/Networking/socket_class.hpp"
#include <chrono>
#include <cstdio>
#ifndef _WIN32
# include <csignal>
#endif

static int32_t measure_failed_sends()
{
    SocketConfig configuration;
    ft_socket listener;
    if (configuration.initialize() != FT_ERR_SUCCESS)
        return (1);
    configuration._port = 0;
    if (listener.initialize(configuration) != FT_ERR_SUCCESS
        || listener.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    constexpr uint32_t ITERATIONS = 2000;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && listener.send_all("x", 1) == -1)
        ++iteration;
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (iteration != ITERATIONS)
        return (1);
    std::printf("%.3f us per failed send (%u calls)\n", elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

static int32_t measure_client_lookups()
{
    constexpr int32_t CLIENT_COUNT = 1024;
    constexpr uint32_t ITERATIONS = 20000;
    ft_socket socket;
    if (socket.initialize() != FT_ERR_SUCCESS || socket.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    // Lookup uses synthetic descriptors. Clear them before socket destruction.
    int32_t client = 0;
    while (client < CLIENT_COUNT)
    {
        if (socket._connected.push_back(client) != FT_ERR_SUCCESS)
        {
            socket._connected.clear();
            return (1);
        }
        ++client;
    }
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && !socket.is_client_connected(CLIENT_COUNT))
        ++iteration;
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    socket._connected.clear();
    if (iteration != ITERATIONS)
        return (1);
    std::printf("%.3f us per missing client (%u calls, %d clients)\n",
        elapsed_us / ITERATIONS, ITERATIONS, CLIENT_COUNT);
    return (0);
}

int32_t main()
{
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    if (measure_failed_sends() != 0 || measure_client_lookups() != 0)
        return (1);
    return (0);
}

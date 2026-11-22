#include "../../../Modules/Networking/udp_socket.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_udp_wait(udp_socket &socket, uint32_t unrelated_count)
{
    constexpr uint32_t MAX_UNRELATED = 48;
    constexpr uint32_t ITERATIONS = 10000;
    ft_socket_handle unrelated[MAX_UNRELATED];
    event_loop loop;
    if (event_loop_initialize(&loop) != FT_ERR_SUCCESS)
        return (1);
    uint32_t opened = 0;
    while (opened < unrelated_count)
    {
        ft_socket_handle &descriptor = unrelated[opened];
        if (descriptor.initialize() != FT_ERR_SUCCESS
            || !descriptor.reset(nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
            || !descriptor.is_valid()
            || event_loop_add_interest(&loop, descriptor.get(), EVENT_LOOP_INTEREST_READ)
                != FT_ERR_SUCCESS)
        {
            event_loop_destroy(&loop);
            return (1);
        }
        ++opened;
    }
    if (udp_event_loop_wait_write(&loop, socket, 0) != 1)
    {
        event_loop_destroy(&loop);
        return (1);
    }
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && udp_event_loop_wait_write(&loop, socket, 0) == 1)
        ++iteration;
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    event_loop_destroy(&loop);
    if (iteration != ITERATIONS)
        return (1);
    std::printf("%u unrelated sockets, %.3f us per wait (%u waits)\n",
        unrelated_count, elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

int32_t main()
{
    SocketConfig configuration;
    if (configuration.initialize() != FT_ERR_SUCCESS)
        return (1);
    configuration._type = SocketType::SERVER;
    configuration._protocol = IPPROTO_UDP;
    configuration._port = 0;
    udp_socket socket;
    if (socket.initialize() != FT_ERR_SUCCESS
        || socket.initialize(configuration) != FT_ERR_SUCCESS)
        return (1);
    if (measure_udp_wait(socket, 0) != 0 || measure_udp_wait(socket, 48) != 0)
        return (1);
    return (0);
}

#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Networking/socket_class.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include "../../../Modules/Networking/udp_socket.hpp"
#include <chrono>
#include <cstdio>

template<typename Socket>
static int32_t measure_numeric_address(Socket &socket, SocketConfig &configuration,
    const char *protocol_name)
{
    constexpr uint32_t ITERATIONS = 20000;
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    ft_size_t frees = 0;
    if (socket.configure_address(configuration) != FT_ERR_SUCCESS
        || cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        configuration._port = static_cast<uint16_t>(iteration);
        if (socket.configure_address(configuration) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.3f us per address, %" FT_PRIu64 " CMA allocations for %u addresses\n",
        protocol_name, elapsed_us / ITERATIONS, allocations_after - allocations_before, ITERATIONS);
    return (0);
}

int main()
{
    ft_socket_handle probe_socket;
    if (probe_socket.initialize() != FT_ERR_SUCCESS
        || !probe_socket.reset(nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
        || !probe_socket.is_valid())
        return (1);
    SocketConfig configuration;
    ft_socket stream_socket;
    udp_socket datagram_socket;
    if (configuration.initialize() != FT_ERR_SUCCESS
        || stream_socket.initialize() != FT_ERR_SUCCESS
        || datagram_socket.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (measure_numeric_address(stream_socket, configuration, "TCP") != 0)
        return (1);
    configuration._protocol = IPPROTO_UDP;
    return (measure_numeric_address(datagram_socket, configuration, "UDP"));
}

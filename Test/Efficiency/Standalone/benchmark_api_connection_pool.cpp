#include "../../../Modules/API/api_internal.hpp"
#include "../../../Modules/Networking/networking.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr char HOST[] = "127.0.0.1";
    constexpr uint32_t ITERATIONS = 8U;
    SocketConfig listener_config;
    ft_socket listener;
    if (listener_config.initialize() != FT_ERR_SUCCESS)
        return (1);
    listener_config._type = SocketType::SERVER;
    ft_strlcpy(listener_config._ip, HOST, sizeof(listener_config._ip));
    listener_config._port = 0;
    if (listener.initialize(listener_config) != FT_ERR_SUCCESS)
        return (1);
    sockaddr_in address{};
    socklen_t address_length = sizeof(address);
    if (getsockname(listener.get_file_descriptor(), reinterpret_cast<sockaddr *>(&address), &address_length) != 0)
        return (1);
    const uint16_t port = ntohs(address.sin_port);
    api_connection_pool_handle connection;
    if (connection.initialize() != FT_ERR_SUCCESS
        || api_connection_pool_acquire(connection, HOST, port, api_connection_security_mode::PLAIN, ft_nullptr))
        return (1);
    SocketConfig client_config;
    if (client_config.initialize() != FT_ERR_SUCCESS)
        return (1);
    client_config._type = SocketType::CLIENT;
    ft_strlcpy(client_config._ip, HOST, sizeof(client_config._ip));
    client_config._port = port;
    if (connection.socket.initialize(client_config) != FT_ERR_SUCCESS)
        return (1);
    connection.has_socket = FT_TRUE;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        api_connection_pool_mark_idle(connection);
        if (!api_connection_pool_acquire(connection, HOST, port, api_connection_security_mode::PLAIN, ft_nullptr))
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    api_connection_pool_evict(connection);
    api_connection_pool_flush();
    if (iteration != ITERATIONS)
        return (1);
    std::printf("api_connection_pool_round_trip us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

#include "../../../Modules/Networking/http_server.hpp"
#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Threading/thread.hpp"
#include <chrono>
#include <cstdio>

#ifndef LIBFT_TEST_BUILD
# error This benchmark requires LIBFT_TEST_BUILD to read the ephemeral server port.
#endif

static void run_server(ft_http_server *server, int32_t *result)
{
    *result = server->run_once();
    return ;
}

int32_t main()
{
    constexpr uint32_t REQUEST_COUNT = 100U;
    constexpr char REQUEST[] = "GET /bench HTTP/1.1\r\nHost: localhost\r\n\r\n";
    constexpr char KEEP_ALIVE_RESPONSE[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: keep-alive\r\n\r\nGET";
    constexpr char CLOSE_RESPONSE[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nGET";
    ft_http_server server;
    if (server.initialize() != FT_ERR_SUCCESS
        || server.start("127.0.0.1", 0U) != FT_ERR_SUCCESS)
        return (1);
    sockaddr_in address = {};
    socklen_t address_length = sizeof(address);
    if (getsockname(server._server_socket.get_file_descriptor(),
            reinterpret_cast<sockaddr *>(&address), &address_length) != 0)
        return (1);
    SocketConfig configuration;
    ft_socket client;
    if (configuration.initialize() != FT_ERR_SUCCESS)
        return (1);
    configuration._type = SocketType::CLIENT;
    ft_strlcpy(configuration._ip, "127.0.0.1", sizeof(configuration._ip));
    configuration._port = ntohs(address.sin_port);
    configuration._recv_timeout = 5000;
    if (client.initialize(configuration) != FT_ERR_SUCCESS)
        return (1);
    char requests[REQUEST_COUNT * (sizeof(REQUEST) - 1U)];
    uint32_t index = 0U;
    while (index < REQUEST_COUNT)
    {
        ft_memcpy_fixed<sizeof(REQUEST) - 1U>(
            requests + index * (sizeof(REQUEST) - 1U), REQUEST);
        index += 1U;
    }
    ft_size_t starting_allocations = 0U;
    if (cma_get_stats(&starting_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    int32_t server_result = FT_ERR_INVALID_STATE;
    const auto start = std::chrono::steady_clock::now();
    ft_thread server_thread(run_server, &server, &server_result);
    if (!server_thread.joinable())
        return (1);
    const ssize_t sent = client.send_all(requests, sizeof(requests), 0);
    ft_size_t response_length = 0U;
    char buffer[2048];
    if (sent == static_cast<ssize_t>(sizeof(requests)))
    {
        ssize_t received = nw_recv(client.get_file_descriptor(), buffer, sizeof(buffer), 0);
        while (received > 0)
        {
            response_length += static_cast<ft_size_t>(received);
            received = nw_recv(client.get_file_descriptor(), buffer, sizeof(buffer), 0);
        }
    }
    (void)client.close_socket();
    server_thread.join();
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t ending_allocations = 0U;
    if (cma_get_stats(&ending_allocations, ft_nullptr) != FT_ERR_SUCCESS
        || server_result != FT_ERR_SUCCESS
        || response_length != (REQUEST_COUNT - 1U) * (sizeof(KEEP_ALIVE_RESPONSE) - 1U)
            + (sizeof(CLOSE_RESPONSE) - 1U))
        return (1);
    std::printf("%u pipelined GET requests: %.2f us and %.2f CMA allocations per request\n",
        REQUEST_COUNT, elapsed_us / REQUEST_COUNT,
        static_cast<double>(ending_allocations - starting_allocations) / REQUEST_COUNT);
    return (0);
}

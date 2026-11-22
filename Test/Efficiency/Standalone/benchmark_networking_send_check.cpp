#include "../../../Modules/Networking/networking.hpp"
#include "../../../Modules/Networking/socket_handle.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    ft_socket_handle listener;
    ft_socket_handle client;
    ft_socket_handle peer;
    if (listener.initialize() != FT_ERR_SUCCESS || client.initialize() != FT_ERR_SUCCESS
        || peer.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (!listener.reset(nw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) || !listener.is_valid())
        return (1);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr *socket_address = reinterpret_cast<sockaddr *>(&address);
    socklen_t address_size = sizeof(address);
    if (nw_bind(listener.get(), socket_address, address_size) != 0
        || nw_listen(listener.get(), 1) != 0
        || getsockname(listener.get(), socket_address, &address_size) != 0)
        return (1);
    if (!client.reset(nw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) || !client.is_valid()
        || nw_connect(client.get(), socket_address, address_size) != 0)
        return (1);
    if (!peer.reset(nw_accept(listener.get(), ft_nullptr, ft_nullptr)) || !peer.is_valid())
        return (1);
    if (nw_send(client.get(), "x", 1, 0) != 1)
        return (1);

    constexpr uint32_t ITERATIONS = 30;
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0;
    while (index < ITERATIONS)
    {
        if (networking_check_socket_after_send(peer.get()) != 0)
            return (1);
        ++index;
    }
    const double ready_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count() / ITERATIONS;
    constexpr uint32_t POLL_ITERATIONS = 2000;
    const auto poll_start = std::chrono::steady_clock::now();
    index = 0;
    while (index < POLL_ITERATIONS)
    {
        int32_t readable = peer.get();
        if (nw_poll(&readable, 1, ft_nullptr, 0, 0) != 1 || readable != peer.get())
            return (1);
        ++index;
    }
    const double poll_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - poll_start).count() / POLL_ITERATIONS;
    char byte;
    if (nw_recv(peer.get(), &byte, 1, 0) != 1 || byte != 'x')
        return (1);
    const auto idle_start = std::chrono::steady_clock::now();
    if (networking_check_socket_after_send(peer.get()) != 0)
        return (1);
    const double idle_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - idle_start).count();
    if (!client.close() || networking_check_socket_after_send(peer.get()) != 1)
        return (1);
    std::printf("%.2f us per ready poll; %.2f us per ready check; %.2f ms idle check\n",
        poll_us, ready_us, idle_ms);
    return (0);
}

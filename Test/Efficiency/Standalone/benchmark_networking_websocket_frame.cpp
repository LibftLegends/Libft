#include "../../Test/websocket_test_utils.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_compressed_send(ft_websocket_server &server, int32_t server_descriptor,
    int32_t client_descriptor)
{
    constexpr uint32_t ITERATIONS = 1000U;
    char payload_bytes[8193];
    uint32_t random_state = 13U;
    ft_size_t index = 0U;
    while (index < sizeof(payload_bytes))
    {
        random_state = random_state * 1664525U + 1013904223U;
        payload_bytes[index++] = static_cast<char>(' ' + ((random_state >> 16U) % 95U));
    }
    ft_string payload;
    if (payload.initialize() != FT_ERR_SUCCESS
        || payload.assign(payload_bytes, sizeof(payload_bytes)) != FT_ERR_SUCCESS)
        return (1);
    server.store_connection_state_locked(server_descriptor, FT_TRUE);
    unsigned char compressed_payload[sizeof(payload_bytes) * 2U];
    uint32_t completed = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (completed < ITERATIONS)
    {
        unsigned char header[4];
        if (server.send_text(server_descriptor, payload) != FT_ERR_SUCCESS
            || nw_recv(client_descriptor, header, sizeof(header), MSG_WAITALL) != sizeof(header)
            || header[0] != 0xC1U || header[1] != 126U)
            return (1);
        const ft_size_t compressed_length = (static_cast<ft_size_t>(header[2]) << 8U) | header[3];
        if (compressed_length <= 4096U || compressed_length > sizeof(compressed_payload)
            || nw_recv(client_descriptor, compressed_payload, compressed_length, MSG_WAITALL)
                != static_cast<ssize_t>(compressed_length))
            return (1);
        ++completed;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%.3f us per compressed loopback send (%u sends, %" FT_PRIu64 " input bytes)\n",
        elapsed_us / ITERATIONS, ITERATIONS, static_cast<ft_size_t>(sizeof(payload_bytes)));
    return (0);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 2000U;
    constexpr ft_size_t PAYLOAD_LENGTH = 4097U;
    ft_websocket_server server;
    ft_websocket_client client;
    ft_socket_handle server_peer;
    ft_string payload;
    ft_string received;
    if (server.initialize() != FT_ERR_SUCCESS || client.initialize() != FT_ERR_SUCCESS
        || server_peer.initialize() != FT_ERR_SUCCESS || payload.initialize() != FT_ERR_SUCCESS
        || received.initialize() != FT_ERR_SUCCESS || payload.assign(PAYLOAD_LENGTH, 'x') != FT_ERR_SUCCESS
        || server.start("127.0.0.1", 0) != FT_ERR_SUCCESS)
        return (1);
    uint16_t port = 0U;
    if (server.get_port(port) != FT_ERR_SUCCESS)
        return (1);
    websocket_server_context context{};
    context.server = &server;
    context.client_fd = -1;
    ft_thread worker(websocket_server_worker, &context);
    if (!worker.joinable())
        return (1);
    if (client.connect("127.0.0.1", port, "/") != FT_ERR_SUCCESS
        || client.send_text(payload) != FT_ERR_SUCCESS)
    {
        client.close();
        worker.join();
        return (1);
    }
    worker.join();
    if (context.result != FT_ERR_SUCCESS || !server_peer.reset(context.client_fd))
        return (1);

    uint32_t completed = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (completed < ITERATIONS)
    {
        if (client.send_text(payload) != FT_ERR_SUCCESS
            || server.receive_frame_locked(server_peer.get(), received) != FT_ERR_SUCCESS
            || received.size() != PAYLOAD_LENGTH
            || server.send_text(server_peer.get(), received) != FT_ERR_SUCCESS
            || client.receive_text(received) != FT_ERR_SUCCESS
            || received.size() != PAYLOAD_LENGTH)
            return (1);
        ++completed;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (ft_memcmp(received.c_str(), payload.c_str(), PAYLOAD_LENGTH) != 0)
        return (1);
    std::printf("%.3f us per loopback echo (%u echoes, %" FT_PRIu64 " bytes per frame)\n",
        elapsed_us / ITERATIONS, ITERATIONS, PAYLOAD_LENGTH);
    return (measure_compressed_send(server, server_peer.get(), client._socket.get()));
}

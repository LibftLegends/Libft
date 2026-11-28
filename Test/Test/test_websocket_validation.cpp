#include "../test_internal.hpp"
#include "websocket_test_utils.hpp"
#include "networking_test_support.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int32_t open_websocket_batch_peer(const sockaddr_in &address, int32_t listener,
    int32_t &receiver, int32_t &sender)
{
    receiver = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (receiver < 0 || nw_connect(receiver, reinterpret_cast<const sockaddr *>(&address),
            sizeof(address)) != 0)
        return (FT_ERR_IO);
    sender = nw_accept(listener, ft_nullptr, ft_nullptr);
    if (sender < 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

static int32_t read_short_websocket_frame(int32_t receiver, uint8_t (&frame)[127])
{
    if (nw_recv(receiver, frame, 2U, MSG_WAITALL) != 2 || frame[1] > 125U)
        return (FT_ERR_IO);
    if (frame[1] != 0U && nw_recv(receiver, frame + 2U, frame[1], MSG_WAITALL) != frame[1])
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_websocket_server_batch_reuses_compression_for_mixed_clients)
{
    if (!networking_test_local_ipv4_available())
        return (1);
    ft_websocket_server server;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.enable_thread_safety());
    ft_string message;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.initialize("world update world update"));
    const int32_t listener = nw_socket(AF_INET, SOCK_STREAM, 0);
    FT_ASSERT(listener >= 0);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t address_length = sizeof(address);
    int32_t receivers[] = {-1, -1, -1};
    int32_t senders[] = {-1, -1, -1};
    uint8_t repeated[3][127]{};
    uint8_t batched[3][127]{};
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t frees = 0U;
    ft_size_t repeated_allocations = 0U;
    ft_size_t batched_allocations = 0U;
    int32_t result = FT_ERR_IO;
    if (nw_bind(listener, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0
        && nw_listen(listener, 3) == 0
        && getsockname(listener, reinterpret_cast<sockaddr *>(&address), &address_length) == 0)
    {
        result = FT_ERR_SUCCESS;
        ft_size_t index = 0U;
        while (index < 3U && result == FT_ERR_SUCCESS)
        {
            result = open_websocket_batch_peer(address, listener, receivers[index], senders[index]);
            if (result == FT_ERR_SUCCESS && index != 1U)
                server.store_connection_state_locked(senders[index], FT_TRUE);
            ++index;
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = cma_get_stats(&before, &frees);
    ft_size_t index = 0U;
    while (index < 3U && result == FT_ERR_SUCCESS)
    {
        result = server.send_text(senders[index], message);
        if (result == FT_ERR_SUCCESS)
            result = read_short_websocket_frame(receivers[index], repeated[index]);
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = cma_get_stats(&after, &frees);
    repeated_allocations = after - before;
    before = after;
    if (result == FT_ERR_SUCCESS)
        result = server.send_text_many(senders, 3U, message);
    index = 0U;
    while (index < 3U && result == FT_ERR_SUCCESS)
    {
        result = read_short_websocket_frame(receivers[index], batched[index]);
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = cma_get_stats(&after, &frees);
    batched_allocations = after - before;
    (void)nw_close(listener);
    index = 0U;
    while (index < 3U)
    {
        if (senders[index] >= 0)
            (void)nw_close(senders[index]);
        if (receivers[index] >= 0)
            (void)nw_close(receivers[index]);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(0xC1U, batched[0][0]);
    FT_ASSERT_EQ(0x81U, batched[1][0]);
    FT_ASSERT_EQ(0xC1U, batched[2][0]);
    FT_ASSERT_EQ(0, ft_memcmp(repeated, batched, sizeof(repeated)));
    FT_ASSERT_EQ(2U * batched_allocations, repeated_allocations);
    FT_ASSERT(batched_allocations > 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.send_text_many(ft_nullptr, 0U, message));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, server.send_text_many(ft_nullptr, 1U, message));
    return (1);
}

FT_TEST(test_websocket_server_rejects_unmasked_frame)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_websocket_server server;
    websocket_server_context context;
    int client_socket;
    ft_thread server_thread;
    unsigned short server_port;
    struct sockaddr_in server_address;
    ft_string handshake_request;
    const char *handshake_data;
    size_t handshake_length;
    size_t total_sent;
    ssize_t send_result;
    char response_buffer[512];
    ssize_t bytes_received;
    const char *terminator;
    unsigned char frame[16];
    size_t payload_length;

    if (server.initialize() != 0)
        return (0);
    if (server.start("127.0.0.1", 0) != 0)
        return (0);
    if (server.get_port(server_port) != 0)
        return (0);
    client_socket = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
        return (0);
    std::memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (nw_inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) != 1)
    {
        nw_close(client_socket);
        return (0);
    }
    if (nw_connect(client_socket, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) != 0)
    {
        nw_close(client_socket);
        return (0);
    }
    context.server = &server;
    context.result = -1;
    context.client_fd = -1;
    server_thread = ft_thread(websocket_server_worker, &context);
    if (!server_thread.joinable())
    {
        nw_close(client_socket);
        return (0);
    }
    if (handshake_request.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    handshake_request = "GET / HTTP/1.1\r\n";
    handshake_request.append("Host: 127.0.0.1\r\n");
    handshake_request.append("Upgrade: websocket\r\n");
    handshake_request.append("Connection: Upgrade\r\n");
    handshake_request.append("Sec-WebSocket-Version: 13\r\n");
    handshake_request.append("Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n\r\n");
    handshake_data = handshake_request.c_str();
    handshake_length = handshake_request.size();
    total_sent = 0;
    while (total_sent < handshake_length)
    {
        send_result = nw_send(client_socket, handshake_data + total_sent, handshake_length - total_sent, 0);
        if (send_result <= 0)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        total_sent += static_cast<size_t>(send_result);
    }
    handshake_request.clear();
    while (1)
    {
        bytes_received = nw_recv(client_socket, response_buffer, sizeof(response_buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        response_buffer[bytes_received] = '\0';
        handshake_request.append(response_buffer);
        terminator = ft_strstr(handshake_request.c_str(), "\r\n\r\n");
        if (terminator != ft_nullptr)
            break ;
    }
    frame[0] = 0x81;
    payload_length = 5;
    frame[1] = static_cast<unsigned char>(payload_length);
    frame[2] = 'h';
    frame[3] = 'e';
    frame[4] = 'l';
    frame[5] = 'l';
    frame[6] = 'o';
    total_sent = 0;
    while (total_sent < payload_length + 2)
    {
        send_result = nw_send(client_socket, frame + total_sent, (payload_length + 2) - total_sent, 0);
        if (send_result <= 0)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        total_sent += static_cast<size_t>(send_result);
    }
    server_thread.join();
    nw_close(client_socket);
    if (context.client_fd >= 0)
        nw_close(context.client_fd);
    if (context.result == 0)
        return (0);
    return (1);
}

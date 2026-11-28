#include "../test_internal.hpp"
#include "websocket_test_utils.hpp"
#include "../../Modules/Networking/networking_websocket_handshake_internal.hpp"
#include "networking_test_support.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static bool get_server_socket_port(int socket_fd, unsigned short *port_value)
{
    struct sockaddr_storage local_address;
    socklen_t address_length;

    if (port_value == ft_nullptr)
        return (false);
    if (socket_fd < 0)
        return (false);
    address_length = sizeof(local_address);
    if (getsockname(socket_fd, reinterpret_cast<struct sockaddr*>(&local_address), &address_length) != 0)
        return (false);
    if (local_address.ss_family == AF_INET)
    {
        const struct sockaddr_in *ipv4_address = reinterpret_cast<const struct sockaddr_in*>(&local_address);
        *port_value = ntohs(ipv4_address->sin_port);
    }
    else if (local_address.ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *ipv6_address = reinterpret_cast<const struct sockaddr_in6*>(&local_address);
        *port_value = ntohs(ipv6_address->sin6_port);
    }
    else
        return (false);
    return (true);
}

static void websocket_invalid_handshake_server(websocket_invalid_handshake_context *context)
{
    struct sockaddr_in client_address;
    socklen_t client_length;
    int client_socket;
    ft_string request;
    char buffer[512];
    ssize_t bytes_received;
    ft_string response;
    const char *response_data;
    size_t total_sent;
    ssize_t send_result;

    if (context == ft_nullptr)
        return ;
    if (context->server_socket < 0)
        return ;
    if (request.is_initialised() == FT_FALSE)
    {
        if (request.initialize() != FT_ERR_SUCCESS)
            return ;
    }
    if (response.is_initialised() == FT_FALSE)
    {
        if (response.initialize() != FT_ERR_SUCCESS)
            return ;
    }
    context->result = -1;
    client_length = sizeof(client_address);
    client_socket = nw_accept(context->server_socket, reinterpret_cast<struct sockaddr*>(&client_address), &client_length);
    if (client_socket < 0)
        return ;
    request.clear();
    while (1)
    {
        bytes_received = nw_recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            nw_close(client_socket);
            return ;
        }
        buffer[bytes_received] = '\0';
        request.append(buffer);
        if (ft_strstr(request.c_str(), "\r\n\r\n"))
            break ;
    }
    response.append("HTTP/1.1 101 Switching Protocols\r\n");
    response.append("Upgrade: websocket\r\n");
    response.append("Connection: Upgrade\r\n\r\n");
    response_data = response.c_str();
    total_sent = 0;
    while (total_sent < response.size())
    {
        send_result = nw_send(client_socket, response_data + total_sent, response.size() - total_sent, 0);
        if (send_result <= 0)
        {
            nw_close(client_socket);
            return ;
        }
        total_sent += static_cast<size_t>(send_result);
    }
    nw_close(client_socket);
    context->result = 0;
    return ;
}

FT_TEST(test_websocket_accept_key_matches_known_value)
{
    constexpr char KEY[] = "dGhlIHNhbXBsZSBub25jZQ==";
    constexpr char EXPECTED[] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    char accept[WEBSOCKET_ACCEPT_KEY_LENGTH + 1U];
    FT_ASSERT(websocket_compute_accept_key(KEY, sizeof(KEY) - 1U, accept));
    FT_ASSERT_EQ(0, ft_memcmp(accept, EXPECTED, sizeof(EXPECTED)));
    return (1);
}

static ft_bool websocket_check_additional_frames(ft_websocket_client &client,
    ft_websocket_server &server, int32_t client_descriptor, ft_string &message, ft_string &received)
{
    message.clear();
    if (client.send_text(message) != FT_ERR_SUCCESS
        || server.receive_frame_locked(client_descriptor, received) != FT_ERR_SUCCESS
        || !received.empty() || server.send_text(client_descriptor, message) != FT_ERR_SUCCESS
        || client.receive_text(received) != FT_ERR_SUCCESS || !received.empty())
        return (FT_FALSE);

    char payload[4097];
    ft_memset(payload, 'm', sizeof(payload));
    payload[17] = '\0';
    payload[sizeof(payload) - 1U] = 'z';
    if (message.assign(payload, sizeof(payload)) != FT_ERR_SUCCESS
        || client.send_text(message) != FT_ERR_SUCCESS
        || server.receive_frame_locked(client_descriptor, received) != FT_ERR_SUCCESS
        || received.size() != sizeof(payload)
        || ft_memcmp(received.c_str(), payload, sizeof(payload)) != 0
        || server.send_text(client_descriptor, message) != FT_ERR_SUCCESS
        || client.receive_text(received) != FT_ERR_SUCCESS
        || received.size() != sizeof(payload)
        || ft_memcmp(received.c_str(), payload, sizeof(payload)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_websocket_handshake_and_echo)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_websocket_server server;
    websocket_server_context context;
    ft_thread server_thread;
    ft_websocket_client client;
    ft_string reply;
    ft_string received;
    unsigned short server_port;

    if (server.initialize() != 0)
        return (0);
    if (server.start("127.0.0.1", 0) != 0)
        return (0);
    if (server.get_port(server_port) != 0)
        return (0);
    context.server = &server;
    context.result = -1;
    context.client_fd = -1;
    server_thread = ft_thread(websocket_server_worker, &context);
    if (!server_thread.joinable())
        return (0);
    if (client.initialize() != 0)
        return (0);
    if (client.connect("127.0.0.1", server_port, "/") != 0)
    {
        server_thread.join();
        return (0);
    }
    ft_string message;

    if (message.initialize() != FT_ERR_SUCCESS)
    {
        server_thread.join();
        client.close();
        return (0);
    }
    if (reply.initialize() != FT_ERR_SUCCESS)
    {
        server_thread.join();
        client.close();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        return (0);
    }
    if (received.initialize() != FT_ERR_SUCCESS)
    {
        server_thread.join();
        client.close();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        return (0);
    }
    message = "ping";
    reply = "pong";
    if (client.send_text(message) != 0)
    {
        server_thread.join();
        client.close();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (0);
    }
    server_thread.join();
    if (context.result != 0)
    {
        client.close();
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (0);
    }
    if (!(context.message == message))
    {
        client.close();
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (0);
    }
    if (server.send_text(context.client_fd, reply) != 0)
    {
        client.close();
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (0);
    }
    if (client.receive_text(received) != 0)
    {
        client.close();
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (0);
    }
    const ft_bool frames_ok = received == reply
        && websocket_check_additional_frames(client, server, context.client_fd, message, received);
    client.close();
    if (context.client_fd >= 0)
        nw_close(context.client_fd);
    if (frames_ok)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reply.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, received.destroy());
    return (0);
}

FT_TEST(test_websocket_server_handles_fragmented_handshake)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_websocket_server server;
    websocket_server_context context;
    int client_socket;
    struct sockaddr_in server_address;
    ft_thread server_thread;
    ft_string handshake_request;
    const char *handshake_data;
    size_t handshake_length;
    size_t total_sent;
    size_t send_length;
    ssize_t send_result;
    char response_buffer[512];
    ft_string handshake_response;
    ssize_t bytes_received;
    const char *terminator;
    ft_string expected_message;
    unsigned char frame[64];
    size_t payload_length;
    size_t frame_length;
    size_t index_value;
    size_t mask_index;
    unsigned short server_port;

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
    handshake_request = "GET /chat HTTP/1.1\r\n";
    handshake_request.append("Host: 127.0.0.1\r\n");
    handshake_request.append("Upgrade: websocket\r\n");
    handshake_request.append("Connection: Upgrade\r\n");
    handshake_request.append("Sec-WebSocket-Version: 13\r\n");
    handshake_request.append("Sec-WebSocket-Key: fragmented==\r\n\r\n");
    handshake_data = handshake_request.c_str();
    handshake_length = handshake_request.size();
    total_sent = 0;
    while (total_sent < handshake_length)
    {
        send_length = 3;
        if (handshake_length - total_sent < send_length)
            send_length = handshake_length - total_sent;
        send_result = nw_send(client_socket, handshake_data + total_sent, send_length, 0);
        if (send_result <= 0)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        total_sent += static_cast<size_t>(send_result);
        if (total_sent < handshake_length)
            usleep(50000);
    }
    if (handshake_response.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_request.destroy());
        return (0);
    }
    handshake_response.clear();
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
        handshake_response.append(response_buffer);
        terminator = ft_strstr(handshake_response.c_str(), "\r\n\r\n");
        if (terminator != ft_nullptr)
            break;
    }
    if (expected_message.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_request.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_response.destroy());
        return (0);
    }
    expected_message = "split";
    payload_length = expected_message.size();
    frame[0] = 0x81;
    frame[1] = static_cast<unsigned char>(0x80 | payload_length);
    frame[2] = 0x12;
    frame[3] = 0x34;
    frame[4] = 0x56;
    frame[5] = 0x78;
    index_value = 0;
    while (index_value < payload_length)
    {
        mask_index = index_value % 4;
        frame[6 + index_value] = static_cast<unsigned char>(expected_message.c_str()[index_value] ^ frame[2 + mask_index]);
        index_value++;
    }
    frame_length = 6 + payload_length;
    total_sent = 0;
    while (total_sent < frame_length)
    {
        send_result = nw_send(client_socket, frame + total_sent, frame_length - total_sent, 0);
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
    if (context.result == 0 && context.message == expected_message)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_request.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_response.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_message.destroy());
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_request.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handshake_response.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_message.destroy());
    return (0);
}

FT_TEST(test_websocket_client_rejects_invalid_handshake)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    int server_socket;
    struct sockaddr_in server_address;
    websocket_invalid_handshake_context context;
    ft_thread server_thread;
    ft_websocket_client client;
    int connect_result;
    unsigned short server_port;

    server_socket = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
        return (0);
    std::memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = htons(0);
    if (nw_bind(server_socket, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) != 0)
    {
        nw_close(server_socket);
        return (0);
    }
    if (get_server_socket_port(server_socket, &server_port) == false)
    {
        nw_close(server_socket);
        return (0);
    }
    if (nw_listen(server_socket, 1) != 0)
    {
        nw_close(server_socket);
        return (0);
    }
    context.server_socket = server_socket;
    context.result = -1;
    server_thread = ft_thread(websocket_invalid_handshake_server, &context);
    if (!server_thread.joinable())
    {
        nw_close(server_socket);
        return (0);
    }
    if (client.initialize() != 0)
    {
        server_thread.join();
        nw_close(server_socket);
        return (0);
    }
    connect_result = client.connect("127.0.0.1", server_port, "/");
    server_thread.join();
    nw_close(server_socket);
    if (context.result != 0)
    {
        client.close();
        return (0);
    }
    if (connect_result == 0)
    {
        client.close();
        return (0);
    }
    client.close();
    return (1);
}

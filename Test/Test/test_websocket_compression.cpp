#include "../test_internal.hpp"
#include "websocket_compression_test_utils.hpp"
#include "networking_test_support.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#include "websocket_test_utils.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_websocket_server_negotiates_permessage_deflate)
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
    ft_string handshake_response;
    const char *handshake_data;
    size_t handshake_length;
    size_t total_sent;
    ssize_t send_result;
    char response_buffer[512];
    ssize_t bytes_received;
    const char *terminator;
    ft_string expected_message;
    ft_vector<unsigned char> compressed_payload;
    ft_vector<unsigned char> frame_buffer;
    unsigned char mask_key[4];
    size_t payload_length;
    size_t index_value;
    ft_string reply;
    unsigned char header_bytes[2];
    ssize_t header_received;
    unsigned char payload_buffer[16384];
    size_t payload_received;
    ft_string decompressed_reply;

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
    handshake_request.append("Sec-WebSocket-Extensions: permessage-deflate\r\n");
    handshake_request.append("Sec-WebSocket-Key: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n");
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
    if (handshake_response.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
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
            break ;
    }
    if (ft_strstr(handshake_response.c_str(), "Sec-WebSocket-Extensions: permessage-deflate") == ft_nullptr)
    {
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    char message_data[8193];
    uint32_t random_state = 13U;
    ft_size_t message_index = 0U;
    while (message_index < sizeof(message_data))
    {
        random_state = random_state * 1664525U + 1013904223U;
        message_data[message_index++] = static_cast<char>(' ' + ((random_state >> 16U) % 95U));
    }
    message_data[15] = '\0';
    if (expected_message.initialize() != FT_ERR_SUCCESS
        || expected_message.assign(message_data, sizeof(message_data)) != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    if (compressed_payload.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    if (frame_buffer.initialize() != FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    if (websocket_permessage_deflate_compress(expected_message, compressed_payload) == false)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    payload_length = compressed_payload.size();
    frame_buffer.clear();
    frame_buffer.push_back(0xC1);
    if (frame_buffer.get_error() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        server_thread.join();
        return (0);
    }
    if (payload_length <= 125)
    {
        frame_buffer.push_back(static_cast<unsigned char>(0x80 | payload_length));
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
    }
    else if (payload_length <= 65535)
    {
        frame_buffer.push_back(0xFE);
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        frame_buffer.push_back(static_cast<unsigned char>((payload_length >> 8) & 0xFF));
        frame_buffer.push_back(static_cast<unsigned char>(payload_length & 0xFF));
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
    }
    else
    {
        size_t shift_index;

        frame_buffer.push_back(0xFF);
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        shift_index = 0;
        while (shift_index < 8)
        {
            frame_buffer.push_back(static_cast<unsigned char>((payload_length >> ((7 - shift_index) * 8)) & 0xFF));
            if (frame_buffer.get_error() != FT_ERR_SUCCESS)
            {
                nw_close(client_socket);
                server_thread.join();
                return (0);
            }
            shift_index++;
        }
    }
    mask_key[0] = 0x12;
    mask_key[1] = 0x34;
    mask_key[2] = 0x56;
    mask_key[3] = 0x78;
    index_value = 0;
    while (index_value < 4)
    {
        frame_buffer.push_back(mask_key[index_value]);
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        index_value++;
    }
    index_value = 0;
    while (index_value < payload_length)
    {
        unsigned char masked_byte;

        masked_byte = static_cast<unsigned char>(compressed_payload[index_value] ^ mask_key[index_value % 4]);
        frame_buffer.push_back(masked_byte);
        if (frame_buffer.get_error() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        index_value++;
    }
    total_sent = 0;
    while (total_sent < frame_buffer.size())
    {
        send_result = nw_send(client_socket, frame_buffer.begin() + total_sent, frame_buffer.size() - total_sent, 0);
        if (send_result <= 0)
        {
            nw_close(client_socket);
            server_thread.join();
            return (0);
        }
        total_sent += static_cast<size_t>(send_result);
    }
    server_thread.join();
    if (context.result != 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    if (context.message.size() != sizeof(message_data)
        || ft_memcmp(context.message.c_str(), message_data, sizeof(message_data)) != 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    if (reply.initialize() != FT_ERR_SUCCESS
        || reply.assign(message_data, sizeof(message_data)) != FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    if (server.send_text(context.client_fd, reply) != 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    header_received = nw_recv(client_socket, header_bytes, 2, MSG_WAITALL);
    if (header_received != 2)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    if (header_bytes[0] != 0xC1)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    if ((header_bytes[1] & 0x80) != 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    unsigned char length_code;

    length_code = static_cast<unsigned char>(header_bytes[1] & 0x7F);
    if (length_code == 126)
    {
        unsigned char extended[2];
        ssize_t extended_received;

        extended_received = nw_recv(client_socket, extended, 2, MSG_WAITALL);
        if (extended_received != 2)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
            nw_close(client_socket);
            if (context.client_fd >= 0)
                nw_close(context.client_fd);
            return (0);
        }
        payload_length = static_cast<size_t>((extended[0] << 8) | extended[1]);
    }
    else if (length_code == 127)
    {
        unsigned char extended[8];
        ssize_t extended_received;
        size_t shift_index;

        extended_received = nw_recv(client_socket, extended, 8, MSG_WAITALL);
        if (extended_received != 8)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
            nw_close(client_socket);
            if (context.client_fd >= 0)
                nw_close(context.client_fd);
            return (0);
        }
        payload_length = 0;
        shift_index = 0;
        while (shift_index < 8)
        {
            payload_length = (payload_length << 8) | extended[shift_index];
            shift_index++;
        }
    }
    else
        payload_length = length_code;
    if (payload_length > sizeof(payload_buffer))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    payload_received = 0;
    while (payload_received < payload_length)
    {
        ssize_t chunk_received;

        chunk_received = nw_recv(client_socket, payload_buffer + payload_received, payload_length - payload_received, 0);
        if (chunk_received <= 0)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
            nw_close(client_socket);
            if (context.client_fd >= 0)
                nw_close(context.client_fd);
            return (0);
        }
        payload_received += static_cast<size_t>(chunk_received);
    }
    if (websocket_permessage_deflate_decompress(payload_buffer, payload_length, decompressed_reply) == false)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
        nw_close(client_socket);
        if (context.client_fd >= 0)
            nw_close(context.client_fd);
        return (0);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed_payload.destroy());
    nw_close(client_socket);
    if (context.client_fd >= 0)
        nw_close(context.client_fd);
    return (decompressed_reply.size() == sizeof(message_data)
        && ft_memcmp(decompressed_reply.c_str(), message_data, sizeof(message_data)) == 0);
}

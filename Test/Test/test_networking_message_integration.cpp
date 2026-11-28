#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Networking/udp_socket.hpp"
#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <cstdlib>
#include <cstring>

FT_TEST(test_networking_ipv6_udp_loopback_integration)
{
    networking_udp_datagram_io server_io;
    networking_udp_datagram_io client_io;
    SocketConfig server_configuration;
    SocketConfig client_configuration;
    networking_message_endpoint destination;
    networking_message_endpoint source;
    sockaddr_storage server_address;
    socklen_t address_length;
    uint16_t server_port;
    const uint8_t payload[8] = {'i', 'p', 'v', '6', '-', 'p', 'i', 'n'};
    uint8_t received_payload[sizeof(payload)] = {0U};
    ft_size_t received_size;
    uint32_t wait_attempts;
    int32_t wait_result;
    int32_t receive_result;

    ft_memset(&server_address, 0, sizeof(server_address));
    if (server_configuration.initialize() != FT_ERR_SUCCESS)
        return (1);
    server_configuration._type = SocketType::SERVER;
    server_configuration._address_family = AF_INET6;
    server_configuration._protocol = IPPROTO_UDP;
    server_configuration._port = 0U;
    std::strncpy(server_configuration._ip, "::1",
        sizeof(server_configuration._ip) - 1U);
    server_configuration._ip[sizeof(server_configuration._ip) - 1U] = '\0';
    if (server_io.initialize(server_configuration) != FT_ERR_SUCCESS)
    {
        (void)server_configuration.destroy();
        return (1);
    }
    address_length = sizeof(server_address);
    if (getsockname(server_io.get_file_descriptor(),
            reinterpret_cast<sockaddr *>(&server_address), &address_length)
        != 0)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_INVALID_OPERATION);
    }
    if (reinterpret_cast<sockaddr_in6 *>(&server_address)->sin6_family
        != AF_INET6)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_INVALID_STATE);
    }
    server_port = ntohs(reinterpret_cast<sockaddr_in6 *>(
        &server_address)->sin6_port);
    if (client_configuration.initialize() != FT_ERR_SUCCESS)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    client_configuration._type = SocketType::CLIENT;
    client_configuration._address_family = AF_INET6;
    client_configuration._protocol = IPPROTO_UDP;
    client_configuration._port = server_port;
    std::strncpy(client_configuration._ip, "::1",
        sizeof(client_configuration._ip) - 1U);
    client_configuration._ip[sizeof(client_configuration._ip) - 1U] = '\0';
    if (client_io.initialize(client_configuration) != FT_ERR_SUCCESS)
    {
        (void)client_configuration.destroy();
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (1);
    }
    ft_memset(&destination, 0, sizeof(destination));
    destination.address = server_address;
    destination.length = address_length;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.send_datagram(destination, payload,
        sizeof(payload)));
    wait_attempts = 0U;
    wait_result = FT_ERR_TIMEOUT;
    while (wait_result == FT_ERR_TIMEOUT && wait_attempts < 20U)
    {
        wait_result = server_io.wait_readable(100U);
        FT_ASSERT(wait_result == FT_ERR_SUCCESS
            || wait_result == FT_ERR_TIMEOUT);
        wait_attempts += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wait_result);
    received_size = 0U;
    receive_result = server_io.receive_datagram(source, received_payload,
        sizeof(received_payload), &received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
    FT_ASSERT_EQ(sizeof(payload), received_size);
    FT_ASSERT_EQ(AF_INET6, source.address.ss_family);
    FT_ASSERT_EQ(0, ft_memcmp(payload, received_payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_configuration.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_configuration.destroy());
    return (1);
}

FT_TEST(test_networking_ipv4_udp_loopback_integration)
{
    networking_udp_datagram_io server_io;
    networking_udp_datagram_io client_io;
    SocketConfig server_configuration;
    SocketConfig client_configuration;
    networking_message_endpoint destination;
    networking_message_endpoint source;
    sockaddr_storage server_address;
    socklen_t address_length;
    uint16_t server_port;
    const uint8_t payload[8] = {'i', 'p', 'v', '4', '-', 'p', 'i', 'n'};
    uint8_t received_payload[sizeof(payload)] = {0U};
    ft_size_t received_size;
    uint32_t wait_attempts;
    int32_t wait_result;
    int32_t receive_result;

    ft_memset(&server_address, 0, sizeof(server_address));
    if (server_configuration.initialize() != FT_ERR_SUCCESS)
        return (1);
    server_configuration._type = SocketType::SERVER;
    server_configuration._address_family = AF_INET;
    server_configuration._protocol = IPPROTO_UDP;
    server_configuration._port = 0U;
    std::strncpy(server_configuration._ip, "127.0.0.1",
        sizeof(server_configuration._ip) - 1U);
    server_configuration._ip[sizeof(server_configuration._ip) - 1U] = '\0';
    if (server_io.initialize(server_configuration) != FT_ERR_SUCCESS)
    {
        (void)server_configuration.destroy();
        return (1);
    }
    address_length = sizeof(server_address);
    if (getsockname(server_io.get_file_descriptor(),
            reinterpret_cast<sockaddr *>(&server_address), &address_length)
        != 0)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_INVALID_OPERATION);
    }
    if (reinterpret_cast<sockaddr_in *>(&server_address)->sin_family != AF_INET)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_INVALID_STATE);
    }
    server_port = ntohs(reinterpret_cast<sockaddr_in *>(
        &server_address)->sin_port);
    if (client_configuration.initialize() != FT_ERR_SUCCESS)
    {
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    client_configuration._type = SocketType::CLIENT;
    client_configuration._address_family = AF_INET;
    client_configuration._protocol = IPPROTO_UDP;
    client_configuration._port = server_port;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1U);
    client_configuration._ip[sizeof(client_configuration._ip) - 1U] = '\0';
    if (client_io.initialize(client_configuration) != FT_ERR_SUCCESS)
    {
        (void)client_configuration.destroy();
        (void)server_io.destroy();
        (void)server_configuration.destroy();
        return (1);
    }
    ft_memset(&destination, 0, sizeof(destination));
    destination.address = server_address;
    destination.length = address_length;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.send_datagram(destination, payload,
        sizeof(payload)));
    wait_attempts = 0U;
    wait_result = FT_ERR_TIMEOUT;
    while (wait_result == FT_ERR_TIMEOUT && wait_attempts < 20U)
    {
        wait_result = server_io.wait_readable(100U);
        FT_ASSERT(wait_result == FT_ERR_SUCCESS
            || wait_result == FT_ERR_TIMEOUT);
        wait_attempts += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wait_result);
    received_size = 0U;
    receive_result = server_io.receive_datagram(source, received_payload,
        sizeof(received_payload), &received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
    FT_ASSERT_EQ(sizeof(payload), received_size);
    FT_ASSERT_EQ(AF_INET, source.address.ss_family);
    FT_ASSERT_EQ(0, ft_memcmp(payload, received_payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_configuration.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_configuration.destroy());
    return (1);
}

static int32_t networking_test_message_transport_udp_reliable_roundtrip(
    int32_t address_family, const char *loopback_ip)
{
    networking_udp_datagram_io server_io;
    networking_udp_datagram_io client_io;
    SocketConfig server_socket_configuration;
    SocketConfig client_socket_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport server;
    networking_message_transport client;
    networking_message_endpoint server_endpoint;
    networking_message_endpoint client_endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options send_options;
    networking_received_message received_message;
    sockaddr_storage server_address;
    sockaddr_storage client_address;
    socklen_t server_address_length;
    socklen_t client_address_length;
    uint8_t payload[4096];
    ft_size_t received_count;
    uint32_t index;
    uint32_t poll_index;
    int32_t result;

    ft_memset(&server_address, 0, sizeof(server_address));
    ft_memset(&client_address, 0, sizeof(client_address));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_socket_configuration.initialize());
    server_socket_configuration._type = SocketType::SERVER;
    server_socket_configuration._address_family = address_family;
    server_socket_configuration._protocol = IPPROTO_UDP;
    server_socket_configuration._port = 0U;
    std::strncpy(server_socket_configuration._ip, loopback_ip,
        sizeof(server_socket_configuration._ip) - 1U);
    result = server_io.initialize(server_socket_configuration);
    if (result != FT_ERR_SUCCESS)
    {
        (void)server_socket_configuration.destroy();
        if (address_family == AF_INET6)
            return (1);
        return (0);
    }
    server_address_length = sizeof(server_address);
    FT_ASSERT_EQ(0, getsockname(server_io.get_file_descriptor(),
        reinterpret_cast<sockaddr *>(&server_address),
        &server_address_length));
    FT_ASSERT_EQ(address_family, server_address.ss_family);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket_configuration.initialize());
    client_socket_configuration._type = SocketType::CLIENT;
    client_socket_configuration._address_family = address_family;
    client_socket_configuration._protocol = IPPROTO_UDP;
    if (address_family == AF_INET6)
        client_socket_configuration._port = ntohs(reinterpret_cast<sockaddr_in6 *>(
            &server_address)->sin6_port);
    else
        client_socket_configuration._port = ntohs(reinterpret_cast<sockaddr_in *>(
            &server_address)->sin_port);
    std::strncpy(client_socket_configuration._ip, loopback_ip,
        sizeof(client_socket_configuration._ip) - 1U);
    result = client_io.initialize(client_socket_configuration);
    if (result != FT_ERR_SUCCESS)
    {
        (void)client_socket_configuration.destroy();
        (void)server_io.destroy();
        (void)server_socket_configuration.destroy();
        if (address_family == AF_INET6)
            return (1);
        return (0);
    }
    client_address_length = sizeof(client_address);
    FT_ASSERT_EQ(0, getsockname(client_io.get_file_descriptor(),
        reinterpret_cast<sockaddr *>(&client_address),
        &client_address_length));
    server_endpoint.address = server_address;
    server_endpoint.length = server_address_length;
    client_endpoint.address = client_address;
    client_endpoint.length = client_address_length;
    transport_configuration.enable_authenticated_handshake = FT_FALSE;
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.maximum_datagram_size = 256U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(transport_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(transport_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(server_endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(server_endpoint,
        client_connection));
    index = 0U;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<uint8_t>((index * 37U) & 0xffU);
        index += 1U;
    }
    send_options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    send_options.channel = 9U;
    send_options.lane = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload), send_options));
    poll_index = 0U;
    while (poll_index < 48U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        result = server.poll(25);
        if (result != FT_ERR_SUCCESS)
            FT_ASSERT_EQ(FT_ERR_TIMEOUT, result);
        result = client.poll(25);
        if (result != FT_ERR_SUCCESS)
            FT_ASSERT_EQ(FT_ERR_TIMEOUT, result);
        poll_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(&received_message,
        1U, received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(sizeof(payload), received_message.payload.size());
    FT_ASSERT_EQ(9U, received_message.channel);
    FT_ASSERT_EQ(static_cast<uint8_t>(2U), received_message.lane);
    index = 0U;
    while (index < sizeof(payload))
    {
        FT_ASSERT_EQ(payload[index], received_message.payload[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_connection(
        received_message.connection_id, server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_connection.send_message(payload,
        32U, send_options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    result = client.poll(250);
    FT_ASSERT(result == FT_ERR_SUCCESS || result == FT_ERR_TIMEOUT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_messages(&received_message,
        1U, received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(32U, received_message.payload.size());
    index = 0U;
    while (index < 32U)
    {
        FT_ASSERT_EQ(payload[index], received_message.payload[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket_configuration.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_socket_configuration.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_ipv4_udp_reliable_large_roundtrip)
{
    return (networking_test_message_transport_udp_reliable_roundtrip(AF_INET,
        "127.0.0.1"));
}

FT_TEST(test_networking_message_transport_ipv6_udp_reliable_large_roundtrip)
{
    return (networking_test_message_transport_udp_reliable_roundtrip(AF_INET6,
        "::1"));
}

#if defined(__linux__)
static ft_bool networking_test_namespace_enabled(void) noexcept
{
    const char *enabled = std::getenv("LIBFT_NETWORK_NAMESPACE_TESTS");

    if (enabled != ft_nullptr && std::strcmp(enabled, "1") == 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool networking_test_namespace_udp_blocked(void) noexcept
{
    const char *blocked = std::getenv("LIBFT_NETWORK_NAMESPACE_BLOCK_UDP");

    if (blocked != ft_nullptr && std::strcmp(blocked, "1") == 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static void networking_test_namespace_peer_endpoint(
    networking_message_endpoint &endpoint, const char *ip_address,
    uint16_t port) noexcept
{
    sockaddr_in address;

    ft_memset(&endpoint, 0, sizeof(endpoint));
    ft_memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    (void)inet_pton(AF_INET, ip_address, &address.sin_addr);
    ft_memcpy(&endpoint.address, &address, sizeof(address));
    endpoint.length = sizeof(address);
    return ;
}

static const char *networking_test_namespace_address(
    const char *variable_name, const char *default_address) noexcept
{
    const char *configured_address;

    configured_address = std::getenv(variable_name);
    if (configured_address == ft_nullptr || configured_address[0] == '\0')
        return (default_address);
    return (configured_address);
}

FT_TEST(test_networking_message_namespace_server_process)
{
    const uint16_t port = 49321U;
    const char *server_address;
    networking_udp_datagram_io io;
    SocketConfig socket_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport transport;
    networking_message_endpoint local_endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    networking_received_message received;
    const char response[] = "namespace-reply";
    ft_size_t attempt;
    int32_t receive_result;

    if (networking_test_namespace_enabled() == FT_FALSE)
        return (1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.initialize());
    socket_configuration._type = SocketType::SERVER;
    socket_configuration._address_family = AF_INET;
    socket_configuration._protocol = IPPROTO_UDP;
    socket_configuration._port = port;
    server_address = networking_test_namespace_address(
        "LIBFT_NETWORK_NAMESPACE_SERVER_IP", "10.89.1.1");
    std::strncpy(socket_configuration._ip, server_address,
        sizeof(socket_configuration._ip) - 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, io.initialize(socket_configuration));
    networking_test_namespace_peer_endpoint(local_endpoint, server_address,
        port);
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(transport_configuration,
        io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(local_endpoint));
    attempt = 0U;
    receive_result = FT_ERR_EMPTY;
    while (attempt < 40U && receive_result != FT_ERR_SUCCESS)
    {
        int32_t poll_result = transport.poll(250);

        FT_ASSERT(poll_result == FT_ERR_SUCCESS || poll_result == FT_ERR_TIMEOUT);
        receive_result = transport.receive_message(received);
        if (receive_result != FT_ERR_SUCCESS)
        {
            FT_ASSERT_EQ(FT_ERR_EMPTY, receive_result);
            attempt += 1U;
        }
    }
    if (networking_test_namespace_udp_blocked() != FT_FALSE)
    {
        FT_ASSERT_EQ(FT_ERR_EMPTY, receive_result);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, io.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.destroy());
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
    FT_ASSERT_EQ(1024U, received.payload.size());
    attempt = 0U;
    while (attempt < 1024U)
    {
        FT_ASSERT_EQ(static_cast<uint8_t>((attempt * 31U + 11U) & 0xffU),
            received.payload[attempt]);
        attempt += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(
        received.connection_id, connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(response,
        sizeof(response) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.destroy());
    return (1);
}

FT_TEST(test_networking_message_namespace_client_process)
{
    const uint16_t port = 49321U;
    const char *server_address;
    networking_udp_datagram_io io;
    SocketConfig socket_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport transport;
    networking_message_endpoint server_endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    networking_received_message received;
    uint8_t payload[1024];
    ft_size_t attempt;
    uint32_t index;
    int32_t receive_result;

    if (networking_test_namespace_enabled() == FT_FALSE)
        return (1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.initialize());
    socket_configuration._type = SocketType::CLIENT;
    socket_configuration._address_family = AF_INET;
    socket_configuration._protocol = IPPROTO_UDP;
    socket_configuration._port = port;
    server_address = networking_test_namespace_address(
        "LIBFT_NETWORK_NAMESPACE_SERVER_IP", "10.89.1.1");
    std::strncpy(socket_configuration._ip, server_address,
        sizeof(socket_configuration._ip) - 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, io.initialize(socket_configuration));
    networking_test_namespace_peer_endpoint(server_endpoint, server_address,
        port);
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(transport_configuration,
        io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(server_endpoint,
        connection));
    index = 0U;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<uint8_t>((index * 31U + 11U) & 0xffU);
        index += 1U;
    }
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 12U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(payload,
        sizeof(payload), options));
    attempt = 0U;
    receive_result = FT_ERR_EMPTY;
    while (attempt < 40U && receive_result != FT_ERR_SUCCESS)
    {
        int32_t poll_result = transport.poll(250);

        FT_ASSERT(poll_result == FT_ERR_SUCCESS || poll_result == FT_ERR_TIMEOUT);
        receive_result = transport.receive_message(received);
        if (receive_result != FT_ERR_SUCCESS)
        {
            FT_ASSERT_EQ(FT_ERR_EMPTY, receive_result);
            attempt += 1U;
        }
    }
    if (networking_test_namespace_udp_blocked() != FT_FALSE)
    {
        FT_ASSERT_EQ(FT_ERR_EMPTY, receive_result);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, io.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.destroy());
        return (1);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
    FT_ASSERT_EQ(sizeof("namespace-reply") - 1U, received.payload.size());
    FT_ASSERT_EQ(0, ft_memcmp(&received.payload[0U], "namespace-reply",
        sizeof("namespace-reply") - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, socket_configuration.destroy());
    return (1);
}
#endif

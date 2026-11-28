#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"

static void networking_flow_test_write_u64(uint8_t *data, ft_size_t offset,
    uint64_t value) noexcept
{
    uint32_t index = 0U;

    while (index < 8U)
    {
        data[offset + 7U - index] = static_cast<uint8_t>(value & 0xffU);
        value >>= 8U;
        index += 1U;
    }
    return ;
}

FT_TEST(test_networking_message_flow_control_enforces_advertised_credit)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config client_configuration;
    networking_message_transport_config server_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    uint8_t payload[64] = {0U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    client_configuration.enable_encryption = FT_FALSE;
    server_configuration.enable_encryption = FT_FALSE;
    server_configuration.maximum_reassembly_bytes = 32U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(client_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(server_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 1U;
    FT_ASSERT_EQ(FT_ERR_FULL, client_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_sender_queue_limit_backpressure_and_recovery)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_message_statistics statistics;
    networking_received_message received[2];
    ft_size_t received_count;
    const uint8_t first_payload[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    const uint8_t second_payload[1] = {9U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.set_queue_limits(sizeof(first_payload)));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(first_payload,
        sizeof(first_payload), options));
    FT_ASSERT_EQ(FT_ERR_FULL, client_connection.set_queue_limits(4U));
    FT_ASSERT_EQ(FT_ERR_FULL, client_connection.send_message(second_payload,
        sizeof(second_payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_statistics(statistics));
    FT_ASSERT_EQ(sizeof(first_payload), statistics.queue_bytes);
    FT_ASSERT_EQ(1U, statistics.messages_sent);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_statistics(statistics));
    FT_ASSERT_EQ(sizeof(first_payload), statistics.queue_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.queue_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(second_payload,
        sizeof(second_payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_statistics(statistics));
    FT_ASSERT_EQ(1U, statistics.queue_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(received, 2U,
        received_count));
    FT_ASSERT_EQ(2U, received_count);
    FT_ASSERT_EQ(sizeof(first_payload), received[0].payload.size());
    FT_ASSERT_EQ(0, ft_memcmp(&received[0].payload[0], first_payload,
        sizeof(first_payload)));
    FT_ASSERT_EQ(1U, received[1].payload.size());
    FT_ASSERT_EQ(second_payload[0], received[1].payload[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_receive_credit_grows_after_consumption)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config client_configuration;
    networking_message_transport_config server_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    const uint8_t payload[8] = {0x51U, 0x52U, 0x53U, 0x54U,
        0x55U, 0x56U, 0x57U, 0x58U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    client_configuration.enable_encryption = FT_FALSE;
    client_configuration.enable_authenticated_handshake = FT_FALSE;
    server_configuration.enable_encryption = FT_FALSE;
    server_configuration.enable_authenticated_handshake = FT_FALSE;
    server_configuration.maximum_reassembly_bytes = sizeof(payload);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(client_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(server_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_FULL, client_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_flow_credit_cannot_shrink)
{
    networking_test_memory_datagram_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    uint8_t flow_control_packet[36U];
    uint8_t payload[48U] = {0U};

    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.maximum_reassembly_bytes = 128U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    ft_memset(flow_control_packet, 0, sizeof(flow_control_packet));
    flow_control_packet[0] = 0x4cU;
    flow_control_packet[1] = 1U;
    flow_control_packet[2] = 9U;
    networking_flow_test_write_u64(flow_control_packet, 4U,
        connection.get_id());
    networking_flow_test_write_u64(flow_control_packet, 12U, 1U);
    networking_flow_test_write_u64(flow_control_packet, 20U, 0U);
    networking_flow_test_write_u64(flow_control_packet, 28U, 64U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(endpoint,
        flow_control_packet, sizeof(flow_control_packet)));
    networking_flow_test_write_u64(flow_control_packet, 12U, 2U);
    networking_flow_test_write_u64(flow_control_packet, 28U, 32U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(endpoint,
        flow_control_packet, sizeof(flow_control_packet)));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 5U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

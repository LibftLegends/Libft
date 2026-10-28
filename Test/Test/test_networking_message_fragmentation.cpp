#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_message_fragmentation_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"

FT_TEST(test_networking_fragmented_message_reassembles_with_exact_bytes)
{
    networking_test_memory_datagram_io sender_io;
    networking_test_memory_datagram_io receiver_io;
    networking_message_transport_config configuration;
    networking_message_transport sender;
    networking_message_transport receiver;
    networking_message_endpoint endpoint;
    networking_message_connection sender_connection;
    networking_message_connection receiver_connection;
    networking_message_send_options options;
    networking_received_message received;
    uint8_t payload[512];
    ft_size_t received_count;
    uint32_t index;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.maximum_datagram_size = 128U;
    configuration.maximum_message_size =
        static_cast<uint32_t>(sizeof(payload));
    configuration.maximum_reassembly_bytes = 1024U;
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    index = 0U;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<uint8_t>(index * 31U + 7U);
        index += 1U;
    }
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(payload,
        sizeof(payload), options));
    index = 0U;
    while (index < 32U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(&received, 1U,
        received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());
    index = 0U;
    while (index < sizeof(payload))
    {
        FT_ASSERT_EQ(payload[index], received.payload[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_message_sizes_around_datagram_limit)
{
    networking_test_memory_datagram_io sender_io;
    networking_test_memory_datagram_io receiver_io;
    networking_message_transport_config configuration;
    networking_message_transport sender;
    networking_message_transport receiver;
    networking_message_endpoint endpoint;
    networking_message_connection sender_connection;
    networking_message_connection receiver_connection;
    networking_message_send_options options;
    /* Datagram 128 minus its 64-byte packet header and 21-byte frame header. */
    networking_received_message received[37];
    const uint32_t message_lengths[37] = {1U, 42U, 43U, 44U, 85U, 86U,
        87U, 127U, 128U, 129U, 130U, 171U, 172U, 173U, 214U, 215U,
        216U, 257U, 258U, 259U, 300U, 301U, 302U, 343U, 344U, 345U,
        386U, 387U, 388U, 429U, 430U, 431U, 472U, 473U, 474U, 511U,
        512U};
    uint8_t payload[512];
    ft_size_t received_count;
    uint32_t message_index;
    uint32_t byte_index;
    uint32_t poll_index;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.maximum_datagram_size = 128U;
    configuration.maximum_message_size = sizeof(payload);
    configuration.maximum_reassembly_bytes = 65536U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration,
        receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    message_index = 0U;
    while (message_index < 37U)
    {
        byte_index = 0U;
        while (byte_index < message_lengths[message_index])
        {
            payload[byte_index] = static_cast<uint8_t>(
                byte_index * 17U + message_index * 53U);
            byte_index += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(payload,
            message_lengths[message_index], options));
        message_index += 1U;
    }
    poll_index = 0U;
    while (poll_index < 256U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        sender_io.advance(1U);
        receiver_io.advance(1U);
        poll_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 37U,
        received_count));
    FT_ASSERT_EQ(37U, received_count);
    message_index = 0U;
    while (message_index < received_count)
    {
        FT_ASSERT_EQ(message_lengths[message_index],
            received[message_index].payload.size());
        byte_index = 0U;
        while (byte_index < message_lengths[message_index])
        {
            FT_ASSERT_EQ(static_cast<uint8_t>(
                    byte_index * 17U + message_index * 53U),
                received[message_index].payload[byte_index]);
            byte_index += 1U;
        }
        message_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_reassembly_order_duplicates_and_conflicts)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_received_message received;
    networking_message_statistics statistics_before;
    networking_message_statistics statistics_after;
    uint8_t packet[128];
    const uint8_t payload_a[4] = {'A', 'B', 'C', 'D'};
    const uint8_t payload_b[4] = {'E', 'F', 'G', 'H'};
    const uint8_t overlap_prefix[5] = {'a', 'b', 'c', 'd', 'e'};
    const uint8_t conflicting_overlap[4] = {'X', 'f', 'g', 'h'};
    const uint8_t overlap_suffix[3] = {'f', 'g', 'h'};
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 32U;
    configuration.maximum_reassembly_bytes = 1024U;
    configuration.maximum_reassembly_messages = 8U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));

    packet_size = networking_fragmentation_make_packet(packet, 501U, 1U,
        8U, 4U, 2U, payload_b,
        static_cast<uint16_t>(sizeof(payload_b)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 501U, 2U,
        8U, 4U, 2U, payload_b,
        static_cast<uint16_t>(sizeof(payload_b)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    packet_size = networking_fragmentation_make_packet(packet, 501U, 3U,
        8U, 0U, 2U, payload_a,
        static_cast<uint16_t>(sizeof(payload_a)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(8U, received.payload.size());
    FT_ASSERT_EQ(0, ft_memcmp(&received.payload[0U], "ABCDEFGH", 8U));

    packet_size = networking_fragmentation_make_packet(packet, 502U, 4U,
        8U, 0U, 2U, payload_a,
        static_cast<uint16_t>(sizeof(payload_a)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    packet_size = networking_fragmentation_make_packet(packet, 502U, 5U,
        9U, 4U, 2U, payload_b,
        static_cast<uint16_t>(sizeof(payload_b)));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    packet_size = networking_fragmentation_make_packet(packet, 502U, 6U,
        8U, 4U, 2U, payload_b,
        static_cast<uint16_t>(sizeof(payload_b)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(0, ft_memcmp(&received.payload[0U], "ABCDEFGH", 8U));

    packet_size = networking_fragmentation_make_packet(packet, 503U, 7U,
        8U, 0U, 2U, overlap_prefix,
        static_cast<uint16_t>(sizeof(overlap_prefix)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics_before));
    packet_size = networking_fragmentation_make_packet(packet, 503U, 8U,
        8U, 4U, 2U, conflicting_overlap,
        static_cast<uint16_t>(sizeof(conflicting_overlap)));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics_after));
    FT_ASSERT_EQ(statistics_before.packets_received,
        statistics_after.packets_received);
    FT_ASSERT_EQ(statistics_before.bytes_received,
        statistics_after.bytes_received);
    packet_size = networking_fragmentation_make_packet(packet, 503U, 9U,
        8U, 5U, 2U, overlap_suffix,
        static_cast<uint16_t>(sizeof(overlap_suffix)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(0, ft_memcmp(&received.payload[0U], "abcdefgh", 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_reassembly_bounds_and_failure_ordinals)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_statistics statistics;
    const uint8_t payload[4] = {1U, 2U, 3U, 4U};
    uint8_t packet[128];
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 16U;
    configuration.maximum_reassembly_bytes = 1024U;
    configuration.maximum_reassembly_messages = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_REASSEMBLY_ALLOCATE, 1U));
    packet_size = networking_fragmentation_make_packet(packet, 601U, 1U,
        8U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 602U, 2U,
        8U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(2U, networking_test_failure_attempt_count(
        NETWORKING_TEST_REASSEMBLY_ALLOCATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_REASSEMBLY_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics));
    FT_ASSERT_EQ(1U, statistics.reassembly_messages);
    FT_ASSERT_EQ(8U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset(
        NETWORKING_TEST_REASSEMBLY_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 603U, 3U,
        8U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_FULL, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics));
    FT_ASSERT_EQ(2U, statistics.reassembly_messages);
    FT_ASSERT_EQ(16U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_reassembly_timeout_releases_memory)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_statistics statistics;
    networking_received_message received;
    const uint8_t partial_payload[4] = {1U, 2U, 3U, 4U};
    const uint8_t complete_payload[3] = {5U, 6U, 7U};
    uint8_t packet[128];
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 50U;
    configuration.maximum_message_size = 16U;
    configuration.maximum_reassembly_bytes = 1024U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 701U, 1U,
        8U, 0U, 2U, partial_payload,
        static_cast<uint16_t>(sizeof(partial_payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    io.advance(25U);
    packet_size = networking_fragmentation_make_packet(packet, 702U, 2U,
        3U, 0U, 1U, complete_payload,
        static_cast<uint16_t>(sizeof(complete_payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(3U, received.payload.size());
    io.advance(25U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics));
    FT_ASSERT_EQ(0U, statistics.reassembly_messages);
    FT_ASSERT_EQ(0U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(1U, statistics.fragments_dropped);
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_reassembly_abort_discards_partial_message)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_connection connection;
    networking_message_statistics statistics;
    networking_received_message received;
    const uint8_t payload[4] = {0x31U, 0x42U, 0x53U, 0x64U};
    uint8_t packet[128];
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 16U;
    configuration.maximum_reassembly_bytes = 512U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 1001U, 1U,
        8U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(77U, connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(1U, statistics.reassembly_messages);
    FT_ASSERT_EQ(8U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.abort(
        networking_message_close_reason::APPLICATION));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.reassembly_messages);
    FT_ASSERT_EQ(0U, statistics.reassembly_bytes);
    packet_size = networking_fragmentation_make_packet(packet, 1001U, 2U,
        8U, 4U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_reassembly_graceful_close_releases_memory)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_connection connection;
    networking_message_statistics statistics;
    networking_received_message received;
    const uint8_t payload[4] = {0x71U, 0x82U, 0x93U, 0xa4U};
    uint8_t packet[128];
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 16U;
    configuration.maximum_reassembly_bytes = 512U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 1101U, 1U,
        8U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(77U, connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(1U, statistics.reassembly_messages);
    FT_ASSERT_EQ(8U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.close());
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.reassembly_messages);
    FT_ASSERT_EQ(0U, statistics.reassembly_bytes);
    packet_size = networking_fragmentation_make_packet(packet, 1101U, 2U,
        8U, 4U, 2U, payload, static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, transport.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_hostile_partial_flood_stays_bounded)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_connection connection;
    networking_message_statistics statistics;
    const uint8_t payload[4] = {0xa1U, 0xb2U, 0xc3U, 0xd4U};
    uint8_t packet[128];
    ft_size_t packet_size;
    uint32_t index;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 16U;
    configuration.maximum_reassembly_bytes = 4096U;
    configuration.maximum_reassembly_messages = 16U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    index = 0U;
    while (index < 16U)
    {
        packet_size = networking_fragmentation_make_packet(packet,
            2000U + index, 1U + index, 8U, 0U, 2U, payload,
            static_cast<uint16_t>(sizeof(payload)));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source,
            packet, packet_size));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics));
    FT_ASSERT_EQ(16U, statistics.reassembly_messages);
    FT_ASSERT_EQ(128U, statistics.reassembly_bytes);
    index = 0U;
    while (index < 256U)
    {
        packet_size = networking_fragmentation_make_packet(packet,
            3000U + index, 17U + index, 8U, 0U, 2U, payload,
            static_cast<uint16_t>(sizeof(payload)));
        FT_ASSERT_EQ(FT_ERR_FULL, transport.process_datagram(source, packet,
            packet_size));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U,
        statistics));
    FT_ASSERT_EQ(16U, statistics.reassembly_messages);
    FT_ASSERT_EQ(128U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(77U, connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.abort(
        networking_message_close_reason::APPLICATION));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.reassembly_messages);
    FT_ASSERT_EQ(0U, statistics.reassembly_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_fragment_global_reassembly_budget_spans_connections)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_message_statistics statistics;
    networking_message_statistics second_statistics;
    const uint8_t payload[16] = {0U};
    uint8_t packet[128];
    ft_size_t packet_size;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.idle_timeout_milliseconds = 0U;
    configuration.maximum_message_size = 4096U;
    configuration.maximum_reassembly_bytes = 4096U;
    configuration.maximum_total_reassembly_bytes = 3000U;
    configuration.maximum_reassembly_messages = 16U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 4001U, 1U,
        1024U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)),
        77U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 4002U, 1U,
        1024U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)),
        78U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U, statistics));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(78U,
        second_statistics));
    FT_ASSERT_EQ(2048U, statistics.reassembly_bytes
        + second_statistics.reassembly_bytes);
    FT_ASSERT_EQ(2U, statistics.reassembly_messages
        + second_statistics.reassembly_messages);

    packet_size = networking_fragmentation_make_packet(packet, 4003U, 1U,
        1024U, 0U, 2U, payload, static_cast<uint16_t>(sizeof(payload)),
        79U);
    FT_ASSERT_EQ(FT_ERR_FULL, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(77U, statistics));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_statistics(78U,
        second_statistics));
    FT_ASSERT_EQ(2048U, statistics.reassembly_bytes
        + second_statistics.reassembly_bytes);
    FT_ASSERT_EQ(2U, statistics.reassembly_messages
        + second_statistics.reassembly_messages);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_message_fragmentation_test_support.hpp"
#include "test_cma_failure_injection.hpp"

class networking_codec_test_io : public networking_datagram_io
{
    public:
        networking_codec_test_io() noexcept
        {
            return ;
        }

        ~networking_codec_test_io() noexcept override
        {
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)destination;
            (void)data;
            (void)size;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            (void)source;
            (void)data;
            (void)capacity;
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (0U);
        }
};

FT_TEST(test_networking_message_encoder_preserves_frame_wire_bytes)
{
    networking_test_memory_datagram_io sender_io;
    networking_test_memory_datagram_io receiver_io;
    sender_io.connect(receiver_io);
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    networking_message_transport sender;
    networking_message_connection connection;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint, connection));
    uint8_t actual[128];
    ft_size_t actual_size = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver_io.receive_datagram(endpoint, actual, sizeof(actual),
            &actual_size));
    FT_ASSERT(actual_size >= 3U);
    FT_ASSERT_EQ(9U, actual[2]);

    networking_message_send_options options;
    options.delivery = networking_message_delivery::UNRELIABLE;
    options.lane = 0U;
    const uint8_t payload[] = {0x00U, 0x23U, 0x80U, 0xFEU, 0x55U};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(payload, sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());

    uint8_t expected[128];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver_io.receive_datagram(endpoint, actual, sizeof(actual),
            &actual_size));
    const ft_size_t expected_size = networking_fragmentation_make_packet(expected, 1U, 2U,
        sizeof(payload), 0U, 1U, payload, sizeof(payload), connection.get_id());
    FT_ASSERT_EQ(expected_size, actual_size);
    FT_ASSERT_EQ(0, ft_memcmp(expected, actual, expected_size));
    return (1);
}

struct networking_codec_cma_scope
{
    test_cma_failure_controller controller{};

    ~networking_codec_cma_scope()
    {
        (void)test_cma_failure_controller_destroy(this->controller);
        return ;
    }
};

FT_TEST(test_networking_ordered_delivery_retries_after_partial_queue_append)
{
    networking_codec_cma_scope failure_scope;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(failure_scope.controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(failure_scope.controller));
    networking_codec_test_io io;
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    networking_message_transport receiver;
    networking_message_endpoint source;
    networking_test_prepare_endpoint(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.listen(source));

    constexpr ft_size_t SEQUENCE_OFFSET = 75U;
    const uint8_t bytes[] = {'A', 'B', 'C'};
    uint8_t packet[128];
    uint64_t sequence = 2U;
    while (sequence <= sizeof(bytes))
    {
        const ft_size_t size = networking_fragmentation_make_packet(packet, sequence, sequence,
            1U, 0U, 1U, bytes + sequence - 1U, 1U);
        packet[3] = static_cast<uint8_t>(networking_message_delivery::RELIABLE_ORDERED);
        networking_fragmentation_write_u64(packet + SEQUENCE_OFFSET, sequence);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.process_datagram(source, packet, size));
        ++sequence;
    }
    networking_received_message received;
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received));
    const ft_size_t size = networking_fragmentation_make_packet(packet, 1U, 1U,
        1U, 0U, 1U, bytes, 1U);
    packet[3] = static_cast<uint8_t>(networking_message_delivery::RELIABLE_ORDERED);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_fail_after(failure_scope.controller,
            TEST_CMA_FAILURE_ALLOCATE, 1U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, receiver.process_datagram(source, packet, size));
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.process_datagram(source, packet, size));
    ft_size_t index = 0U;
    while (index < sizeof(bytes))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_message(received));
        FT_ASSERT_EQ(index + 1U, received.sequence);
        FT_ASSERT_EQ(1U, received.payload.size());
        FT_ASSERT_EQ(bytes[index], received.payload[0]);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received));
    return (1);
}

FT_TEST(test_networking_connection_lookup_keeps_ordered_and_legacy_ids_across_move)
{
    networking_codec_test_io io;
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    networking_message_transport transport;
    networking_message_transport moved;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(endpoint));
    networking_message_connection handle;
    uint64_t expected_id = 1U;
    while (expected_id <= 12U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint, handle));
        FT_ASSERT_EQ(expected_id, handle.get_id());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.destroy());
        ++expected_id;
    }

    uint8_t packet[128];
    const uint8_t payload = 0x5AU;
    ft_size_t size = networking_fragmentation_make_packet(packet, 1U, 1U,
        1U, 0U, 1U, &payload, 1U, 100U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(endpoint, packet, size));
    size = networking_fragmentation_make_packet(packet, 1U, 1U,
        1U, 0U, 1U, &payload, 1U, 200U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    const int32_t scheduled = networking_test_failure_fail_next(NETWORKING_TEST_EVENT_ENQUEUE);
    const int32_t failed_open = transport.process_datagram(endpoint, packet, size);
    const int32_t ended = networking_test_failure_end();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduled);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_open);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ended);

    const uint64_t incoming_ids[] = {150U, 45U, 120U};
    ft_size_t index = 0U;
    while (index < sizeof(incoming_ids) / sizeof(incoming_ids[0]))
    {
        size = networking_fragmentation_make_packet(packet, 1U, 1U,
            1U, 0U, 1U, &payload, 1U, incoming_ids[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(endpoint, packet, size));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(transport));
    networking_message_statistics statistics;
    const uint64_t existing_ids[] = {1U, 12U, 100U, 150U, 45U, 120U};
    index = 0U;
    while (index < sizeof(existing_ids) / sizeof(existing_ids[0]))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_statistics(existing_ids[index], statistics));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, moved.get_statistics(200U, statistics));
    return (1);
}

FT_TEST(test_networking_batch_receive_preserves_remaining_queue_and_unused_slots)
{
    networking_codec_test_io io;
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    networking_message_transport receiver;
    networking_message_endpoint source;
    networking_test_prepare_endpoint(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.listen(source));
    const uint8_t bytes[] = {'A', 'B', 'C'};
    uint8_t packet[128];
    ft_size_t index = 0U;
    while (index < sizeof(bytes))
    {
        const ft_size_t size = networking_fragmentation_make_packet(packet, index + 1U,
            index + 1U, 1U, 0U, 1U, bytes + index, 1U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.process_datagram(source, packet, size));
        ++index;
    }
    networking_received_message received[3];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, received[2].payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, received[2].payload.push_back('Z'));
    ft_size_t count = 99U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, receiver.receive_messages(ft_nullptr, 2U, count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 2U, count));
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ('A', received[0].payload[0]);
    FT_ASSERT_EQ('B', received[1].payload[0]);
    FT_ASSERT_EQ('Z', received[2].payload[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 3U, count));
    FT_ASSERT_EQ(1U, count);
    FT_ASSERT_EQ('C', received[0].payload[0]);
    FT_ASSERT_EQ('B', received[1].payload[0]);
    FT_ASSERT_EQ('Z', received[2].payload[0]);
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_messages(received, 3U, count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received[0]));
    FT_ASSERT_EQ('C', received[0].payload[0]);
    return (1);
}

FT_TEST(test_networking_sparse_channels_keep_independent_reliable_sequences)
{
    networking_test_memory_datagram_io sender_io;
    networking_test_memory_datagram_io receiver_io;
    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    networking_message_transport sender;
    networking_message_transport receiver;
    networking_message_connection sender_connection;
    networking_message_connection receiver_connection;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint, sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint, receiver_connection));
    constexpr uint32_t CHANNEL_COUNT = 12U;
    uint32_t round = 0U;
    while (round < 2U)
    {
        uint32_t index = 0U;
        while (index < CHANNEL_COUNT)
        {
            networking_message_send_options options;
            options.channel = (CHANNEL_COUNT - index) * 131072U;
            if (index == 0U)
                options.channel = UINT32_MAX;
            const uint8_t payload = static_cast<uint8_t>(round * CHANNEL_COUNT + index);
            FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(&payload, 1U, options));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
            networking_received_message message;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_message(message));
            FT_ASSERT_EQ(options.channel, message.channel);
            FT_ASSERT_EQ(round + 1U, message.sequence);
            FT_ASSERT_EQ(1U, message.payload.size());
            FT_ASSERT_EQ(payload, message.payload[0]);
            sender_io.advance(2U);
            receiver_io.advance(2U);
            ++index;
        }
        ++round;
    }
    return (1);
}

FT_TEST(test_networking_message_codec_rejects_header_mutations_and_truncation)
{
    networking_codec_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    const uint8_t payload[1] = {0x5aU};
    uint8_t packet[128];
    uint32_t size;
    ft_size_t packet_size;

    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = static_cast<socklen_t>(sizeof(sockaddr_in));
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    size = 0U;
    while (size < sizeof(packet))
    {
        ft_memset(packet, 0, sizeof(packet));
        if (size >= 1U)
            packet[0] = 0x4cU;
        if (size >= 2U)
            packet[1] = 1U;
        if (size >= 3U)
            packet[2] = 0U;
        if (size >= 4U)
            packet[3] = 0U;
        FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
            source, packet, size));
        size += 1U;
    }
    packet[0] = 0U;
    packet[1] = 1U;
    packet[2] = 0U;
    packet[3] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    packet_size = networking_fragmentation_make_packet(packet, 1U, 1U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, transport.process_datagram(
        source, packet, packet_size));
    packet[0] = 0x4cU;
    packet[1] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    packet[1] = 1U;
    packet[2] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_codec_valid_frame_truncation_and_field_matrix)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_connection unexpected_connection;
    networking_message_endpoint source;
    networking_received_message received;
    const uint8_t payload[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint8_t packet[128];
    ft_size_t packet_size;
    ft_size_t truncated_size;
    int32_t parse_result;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 801U, 1U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());

    truncated_size = 0U;
    while (truncated_size < packet_size)
    {
        packet_size = networking_fragmentation_make_packet(packet,
            802U + truncated_size, 2U + truncated_size,
            sizeof(payload), 0U, 1U, payload,
            static_cast<uint16_t>(sizeof(payload)));
        parse_result = transport.process_datagram(source, packet,
            truncated_size);
        if (truncated_size < packet_size - sizeof(payload))
            FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, parse_result);
        else
            FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, parse_result);
        truncated_size += 1U;
    }

    packet_size = networking_fragmentation_make_packet(packet, 900U, 100U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[0] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[0] = 0x4cU;
    packet[1] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[1] = 1U;
    packet[2] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 901U, 101U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[3] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[2] = 2U;
    packet[3] = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 902U, 102U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[11] = 78U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(78U,
        unexpected_connection));
    packet_size = networking_fragmentation_make_packet(packet, 909U, 109U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[11] = 79U;
    packet[3] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        transport.get_connection(79U, unexpected_connection));
    packet_size = networking_fragmentation_make_packet(packet, 903U, 103U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[44] = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 904U, 104U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[44] = 5U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 904U, 104U,
        sizeof(payload), UINT32_MAX, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 906U, 106U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[84] = 5U;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 907U, 107U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[84] = 3U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 908U, 108U,
        sizeof(payload), 0U, 0U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 905U, 105U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[packet_size] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size + 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

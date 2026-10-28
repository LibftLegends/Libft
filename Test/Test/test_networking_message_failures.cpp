#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "networking_test_support.hpp"

FT_TEST(test_networking_message_fragment_allocation_failure_is_transactional)
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
    networking_message_statistics statistics;
    networking_received_message received;
    uint8_t payload[129];
    uint32_t byte_index;
    uint32_t poll_index;

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
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    byte_index = 0U;
    while (byte_index < sizeof(payload))
    {
        payload[byte_index] = static_cast<uint8_t>(byte_index * 37U + 11U);
        byte_index += 1U;
    }
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE, 1U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, sender_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(2U, networking_test_failure_attempt_count(
        NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        sender_connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.messages_sent);
    FT_ASSERT_EQ(0U, statistics.queue_depth);
    FT_ASSERT_EQ(0U, statistics.queue_bytes);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(payload,
        sizeof(payload), options));
    poll_index = 0U;
    while (poll_index < 32U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        sender_io.advance(1U);
        receiver_io.advance(1U);
        poll_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_message(received));
    FT_ASSERT_EQ(1U, received.sequence);
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());
    byte_index = 0U;
    while (byte_index < sizeof(payload))
    {
        FT_ASSERT_EQ(payload[byte_index], received.payload[byte_index]);
        byte_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver_connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

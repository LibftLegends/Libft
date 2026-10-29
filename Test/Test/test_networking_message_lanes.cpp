#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"

class networking_test_limited_datagram_io : public networking_datagram_io
{
    private:
        networking_test_memory_datagram_io _inner;
        uint32_t _successful_send_limit;
        uint32_t _successful_sends;

    public:
        networking_test_limited_datagram_io() noexcept
            : _inner(), _successful_send_limit(UINT32_MAX), _successful_sends(0U)
        {
            return ;
        }

        ~networking_test_limited_datagram_io() noexcept override
        {
            return ;
        }

        void connect(networking_test_limited_datagram_io &peer) noexcept
        {
            this->_inner.connect(peer._inner);
            return ;
        }

        void set_successful_send_limit(uint32_t limit) noexcept
        {
            this->_successful_send_limit = limit;
            this->_successful_sends = 0U;
            return ;
        }

        uint32_t successful_sends() const noexcept
        {
            return (this->_successful_sends);
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            if (this->_successful_sends >= this->_successful_send_limit)
                return (FT_ERR_SOCKET_SEND_FAILED);
            this->_successful_sends += 1U;
            return (this->_inner.send_datagram(destination, data, size));
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            return (this->_inner.receive_datagram(source, data, capacity,
                received_size));
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (this->_inner.now_milliseconds());
        }
};

FT_TEST(test_networking_message_lanes_account_messages_independently)
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
    networking_received_message received[4];
    networking_message_statistics statistics;
    ft_size_t received_count;
    uint32_t lane;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    lane = 0U;
    while (lane < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(
            static_cast<uint8_t>(lane),
            lane + 1U, 0U));
        options.delivery = networking_message_delivery::UNRELIABLE;
        options.lane = static_cast<uint8_t>(lane);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
            &lane, sizeof(lane), options));
        lane += 1U;
    }
    lane = 0U;
    while (lane < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        lane += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 4U,
        received_count));
    FT_ASSERT_EQ(4U, received_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.get_statistics(
        receiver_connection.get_id(), statistics));
    lane = 0U;
    while (lane < 4U)
    {
        FT_ASSERT_EQ(1U, statistics.lane_messages_received[lane]);
        FT_ASSERT_EQ(sizeof(lane), statistics.lane_bytes_received[lane]);
        lane += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_message_statistics_match_generated_lane_trace)
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
    networking_message_statistics sender_statistics;
    networking_message_statistics receiver_statistics;
    uint64_t expected_messages[4] = {0U, 0U, 0U, 0U};
    uint64_t expected_bytes[4] = {0U, 0U, 0U, 0U};
    uint8_t payload[64];
    uint32_t state = 0x7f4a7c15U;
    uint32_t message_index = 0U;
    uint64_t expected_total_bytes = 0U;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration,
        receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    options.delivery = networking_message_delivery::UNRELIABLE;
    options.channel = 0U;
    options.expiry_milliseconds = 0U;
    while (message_index < 64U)
    {
        uint8_t lane;
        ft_size_t payload_size;
        uint32_t byte_index = 0U;

        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        lane = static_cast<uint8_t>(state % 4U);
        payload_size = 1U + (state >> 8U) % 63U;
        while (byte_index < payload_size)
        {
            payload[byte_index] = static_cast<uint8_t>(state + byte_index
                + message_index);
            byte_index += 1U;
        }
        options.lane = lane;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(payload,
            payload_size, options));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_message(received));
        FT_ASSERT_EQ(lane, received.lane);
        FT_ASSERT_EQ(payload_size, received.payload.size());
        byte_index = 0U;
        while (byte_index < payload_size)
        {
            FT_ASSERT_EQ(payload[byte_index], received.payload[byte_index]);
            byte_index += 1U;
        }
        expected_messages[lane] += 1U;
        expected_bytes[lane] += payload_size;
        expected_total_bytes += payload_size;
        message_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.get_statistics(
        sender_statistics));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver_connection.get_statistics(
        receiver_statistics));
    FT_ASSERT_EQ(64U, sender_statistics.messages_sent);
    FT_ASSERT_EQ(64U, receiver_statistics.messages_received);
    /* The transport accounts for its initial control packet separately. */
    FT_ASSERT(sender_statistics.packets_sent >= 64U);
    FT_ASSERT(sender_statistics.packets_sent <= 65U);
    FT_ASSERT(receiver_statistics.packets_received >= 64U);
    FT_ASSERT(receiver_statistics.packets_received <= 65U);
    FT_ASSERT_EQ(0U, sender_statistics.queue_depth);
    FT_ASSERT_EQ(0U, sender_statistics.queue_bytes);
    message_index = 0U;
    while (message_index < 4U)
    {
        FT_ASSERT_EQ(expected_messages[message_index],
            sender_statistics.lane_messages_sent[message_index]);
        FT_ASSERT_EQ(expected_messages[message_index],
            receiver_statistics.lane_messages_received[message_index]);
        FT_ASSERT_EQ(expected_bytes[message_index],
            sender_statistics.lane_bytes_sent[message_index]);
        FT_ASSERT_EQ(expected_bytes[message_index],
            receiver_statistics.lane_bytes_received[message_index]);
        message_index += 1U;
    }
    FT_ASSERT_EQ(expected_total_bytes, sender_statistics.lane_bytes_sent[0U]
        + sender_statistics.lane_bytes_sent[1U]
        + sender_statistics.lane_bytes_sent[2U]
        + sender_statistics.lane_bytes_sent[3U]);
    FT_ASSERT_EQ(expected_total_bytes,
        receiver_statistics.lane_bytes_received[0U]
        + receiver_statistics.lane_bytes_received[1U]
        + receiver_statistics.lane_bytes_received[2U]
        + receiver_statistics.lane_bytes_received[3U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_message_lanes_weighted_service_prevents_starvation)
{
    networking_test_limited_datagram_io sender_io;
    networking_test_limited_datagram_io receiver_io;
    networking_message_transport_config configuration;
    networking_message_transport sender;
    networking_message_transport receiver;
    networking_message_endpoint endpoint;
    networking_message_connection sender_connection;
    networking_message_connection receiver_connection;
    networking_message_send_options options;
    networking_message_statistics sender_statistics;
    networking_received_message received[210];
    uint32_t lane_counts[4] = {0U, 0U, 0U, 0U};
    ft_size_t received_count;
    uint32_t lane;
    uint32_t message_index;
    uint32_t received_index;
    uint32_t lane_message_limit;
    uint32_t payload_index;
    int32_t poll_result;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(1U, 8U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(2U, 4U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(3U, 2U, 0U));
    options.delivery = networking_message_delivery::UNRELIABLE;
    lane = 1U;
    while (lane < 4U)
    {
        lane_message_limit = 30U;
        if (lane == 1U)
            lane_message_limit = 120U;
        else if (lane == 2U)
            lane_message_limit = 60U;
        message_index = 0U;
        while (message_index < lane_message_limit)
        {
            options.lane = static_cast<uint8_t>(lane);
            FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(&lane,
                sizeof(lane), options));
            message_index += 1U;
        }
        lane += 1U;
    }
    sender_io.set_successful_send_limit(65U);
    poll_result = FT_ERR_SUCCESS;
    while (sender_io.successful_sends() < 65U
        && poll_result == FT_ERR_SUCCESS)
    {
        poll_result = sender.poll();
        if (poll_result != FT_ERR_SUCCESS)
            break ;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
    }
    if (poll_result == FT_ERR_SUCCESS)
        poll_result = sender.poll();
    FT_ASSERT_EQ(65U, sender_io.successful_sends());
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, poll_result);
    message_index = 0U;
    while (message_index < 128U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        message_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 64U,
        received_count));
    FT_ASSERT_EQ(64U, received_count);
    received_index = 0U;
    while (received_index < received_count)
    {
        FT_ASSERT(received[received_index].lane >= 1U
            && received[received_index].lane <= 3U);
        lane_counts[received[received_index].lane] += 1U;
        received_index += 1U;
    }
    /* 8:4:2 weights over 64 datagrams allow a two-datagram tolerance. */
    FT_ASSERT(lane_counts[1] >= 35U && lane_counts[1] <= 39U);
    FT_ASSERT(lane_counts[2] >= 16U && lane_counts[2] <= 20U);
    FT_ASSERT(lane_counts[3] >= 7U && lane_counts[3] <= 11U);
    sender_io.set_successful_send_limit(UINT32_MAX);
    message_index = 0U;
    while (message_index < 256U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        message_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 210U,
        received_count));
    FT_ASSERT_EQ(146U, received_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(1U, 3U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(2U, 1U, 0U));
    payload_index = 0U;
    while (payload_index < 30U)
    {
        options.lane = 1U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
            &payload_index, sizeof(payload_index), options));
        payload_index += 1U;
    }
    payload_index = 0U;
    while (payload_index < 10U)
    {
        options.lane = 2U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
            &payload_index, sizeof(payload_index), options));
        payload_index += 1U;
    }
    sender_io.set_successful_send_limit(21U);
    poll_result = FT_ERR_SUCCESS;
    while (sender_io.successful_sends() < 21U
        && poll_result == FT_ERR_SUCCESS)
    {
        poll_result = sender.poll();
        if (poll_result != FT_ERR_SUCCESS)
            break ;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
    }
    if (poll_result == FT_ERR_SUCCESS)
        poll_result = sender.poll();
    FT_ASSERT_EQ(21U, sender_io.successful_sends());
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, poll_result);
    message_index = 0U;
    while (message_index < 40U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        message_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 20U,
        received_count));
    FT_ASSERT_EQ(20U, received_count);
    ft_memset(lane_counts, 0, sizeof(lane_counts));
    received_index = 0U;
    while (received_index < received_count)
    {
        lane_counts[received[received_index].lane] += 1U;
        received_index += 1U;
    }
    FT_ASSERT(lane_counts[1] >= 13U && lane_counts[1] <= 17U);
    FT_ASSERT(lane_counts[2] >= 3U && lane_counts[2] <= 7U);
    FT_ASSERT_EQ(0U, lane_counts[3]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.get_statistics(
        sender_statistics));
    FT_ASSERT_EQ(3U, sender_statistics.lane_priority_weight[1U]);
    FT_ASSERT_EQ(1U, sender_statistics.lane_priority_weight[2U]);
    FT_ASSERT_EQ(2U, sender_statistics.lane_priority_weight[3U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_message_lane_reservation_precedes_unreserved_lane)
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
    networking_received_message received[8];
    ft_size_t received_count;
    uint32_t message_index;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration, receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(1U, 1U,
        10000U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.configure_lane(2U, 1000U,
        0U));
    options.delivery = networking_message_delivery::UNRELIABLE;
    message_index = 0U;
    while (message_index < 4U)
    {
        options.lane = 1U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
            &message_index, sizeof(message_index), options));
        options.lane = 2U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
            &message_index, sizeof(message_index), options));
        message_index += 1U;
    }
    message_index = 0U;
    while (message_index < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        sender_io.advance(1U);
        receiver_io.advance(1U);
        message_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.receive_messages(received, 8U,
        received_count));
    FT_ASSERT_EQ(8U, received_count);
    message_index = 0U;
    while (message_index < 4U)
    {
        FT_ASSERT_EQ(1U, received[message_index].lane);
        message_index += 1U;
    }
    while (message_index < 8U)
    {
        FT_ASSERT_EQ(2U, received[message_index].lane);
        message_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_message_pacing_uses_manual_clock)
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
    const uint8_t warmup_payload = 0x5aU;
    const uint8_t paced_payload = 0xa5U;
    uint64_t bytes_after_warmup;
    uint64_t bytes_after_first_send;

    sender_io.connect(receiver_io);
    receiver_io.connect(sender_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.acknowledgement_delay_milliseconds = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(configuration, sender_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(configuration,
        receiver_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.open_connection(endpoint,
        sender_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.open_connection(endpoint,
        receiver_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
        &warmup_payload, sizeof(warmup_payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
    sender_io.advance(1U);
    receiver_io.advance(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        sender_connection.get_statistics(statistics));
    FT_ASSERT(statistics.smoothed_rtt_milliseconds > 0U);
    bytes_after_warmup = sender_io.bytes_sent();

    options.delivery = networking_message_delivery::UNRELIABLE;
    options.lane = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
        &paced_payload, sizeof(paced_payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(
        &paced_payload, sizeof(paced_payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    bytes_after_first_send = sender_io.bytes_sent();
    FT_ASSERT(bytes_after_first_send > bytes_after_warmup);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    FT_ASSERT_EQ(bytes_after_first_send, sender_io.bytes_sent());
    sender_io.advance(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    FT_ASSERT(sender_io.bytes_sent() > bytes_after_first_send);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

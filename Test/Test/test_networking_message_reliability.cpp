#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static uint32_t networking_reliability_soak_seed(void) noexcept
{
    const char *seed_text;
    uint32_t seed_value;
    ft_size_t index;

    seed_text = std::getenv("LIBFT_NETWORKING_SOAK_SEED");
    if (seed_text == ft_nullptr || seed_text[0] == '\0')
        return (0x31a7d25bU);
    seed_value = 0U;
    index = 0U;
    while (seed_text[index] != '\0')
    {
        uint32_t digit;

        if (seed_text[index] < '0' || seed_text[index] > '9')
            return (0x31a7d25bU);
        digit = static_cast<uint32_t>(seed_text[index] - '0');
        if (seed_value > (UINT32_MAX - digit) / 10U)
            return (0x31a7d25bU);
        seed_value = seed_value * 10U + digit;
        index += 1U;
    }
    return (seed_value);
}
#include "../../Modules/Networking/networking_simulator.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_networking_unreliable_datagram_is_not_retransmitted)
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
    const uint8_t payload[5] = {3U, 1U, 4U, 1U, 5U};
    uint64_t packets_sent_before;
    uint32_t poll_index;

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
    poll_index = 0U;
    while (poll_index < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        poll_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.get_statistics(statistics));
    packets_sent_before = statistics.packets_sent;
    sender_io.drop_next_datagram();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
    poll_index = 0U;
    while (poll_index < 8U)
    {
        sender_io.advance(configuration.retransmission_timeout_milliseconds);
        receiver_io.advance(configuration.retransmission_timeout_milliseconds);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.poll());
        poll_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, receiver.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        sender_connection.get_statistics(statistics));
    FT_ASSERT(statistics.packets_sent >= packets_sent_before);
    FT_ASSERT_EQ(1U, statistics.packets_sent - packets_sent_before);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_reliable_messages_retransmit_in_channel_order)
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
    networking_received_message received[4];
    ft_size_t received_count;
    uint8_t payloads[4] = {11U, 22U, 33U, 44U};
    uint32_t index;

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
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    index = 0U;
    while (index < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(
            &payloads[index], 1U, options));
        index += 1U;
    }
    client_io.drop_next_datagram();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    client_io.advance(1000U);
    server_io.advance(1000U);
    index = 0U;
    while (index < 32U)
    {
        client_io.advance(configuration.retransmission_timeout_milliseconds);
        server_io.advance(configuration.retransmission_timeout_milliseconds);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(received, 4U,
        received_count));
    FT_ASSERT_EQ(4U, received_count);
    index = 0U;
    while (index < received_count)
    {
        FT_ASSERT_EQ(1U, received[index].payload.size());
        FT_ASSERT_EQ(payloads[index], received[index].payload[0]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

static int32_t networking_reliability_run_seeded_impairment_trace(
    uint32_t impairment_seed)
{
    networking_test_memory_datagram_io client_underlying;
    networking_test_memory_datagram_io server_underlying;
    networking_simulator_config client_simulator_configuration;
    networking_simulator_config server_simulator_configuration;
    networking_simulated_datagram_io client_simulator;
    networking_simulated_datagram_io server_simulator;
    networking_message_transport_config transport_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_message_statistics statistics;
    networking_received_message received[32];
    ft_size_t received_count;
    uint32_t payloads[32];
    uint32_t payload_index;
    uint32_t poll_index;
    uint32_t impairment_rate_index;
    uint32_t loss_parts_per_million;
    int32_t poll_result;

    client_underlying.connect(server_underlying);
    server_underlying.connect(client_underlying);
    impairment_rate_index = impairment_seed % 4U;
    loss_parts_per_million = impairment_rate_index * 100000U;
    client_simulator_configuration.loss_parts_per_million =
        loss_parts_per_million;
    client_simulator_configuration.duplicate_parts_per_million = 50000U;
    /* The plaintext trace cannot authenticate arbitrary bit flips. */
    client_simulator_configuration.corruption_parts_per_million = 0U;
    client_simulator_configuration.reorder_parts_per_million = 100000U;
    client_simulator_configuration.random_seed = impairment_seed;
    server_simulator_configuration.loss_parts_per_million =
        loss_parts_per_million;
    server_simulator_configuration.duplicate_parts_per_million = 50000U;
    server_simulator_configuration.corruption_parts_per_million = 0U;
    server_simulator_configuration.reorder_parts_per_million = 100000U;
    server_simulator_configuration.random_seed = impairment_seed
        ^ 0x8c4e1063U;
    if (server_simulator_configuration.random_seed == 0U)
        server_simulator_configuration.random_seed = 1U;
    if (std::getenv("LIBFT_NETWORKING_SOAK_SEED") != ft_nullptr)
        std::fprintf(stderr,
            "networking impairment seed=%u server_seed=%" PRIu64 " loss=%u/1000000 duplicate=50000/1000000 corruption=0/1000000 reorder=100000/1000000 trace=send_ordered(channel=2,count=32,payload_u32=i*7919+37);poll_and_advance(1200,10ms)\n",
            impairment_seed,
            server_simulator_configuration.random_seed,
            loss_parts_per_million);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_simulator.initialize(
        client_underlying, client_simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_simulator.initialize(
        server_underlying, server_simulator_configuration));
    networking_test_prepare_endpoint(endpoint);
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(transport_configuration,
        client_simulator));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(transport_configuration,
        server_simulator));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 2U;
    payload_index = 0U;
    while (payload_index < 32U)
    {
        payloads[payload_index] = payload_index * 7919U + 37U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(
            &payloads[payload_index], sizeof(payloads[payload_index]), options));
        payload_index += 1U;
    }
    poll_index = 0U;
    while (poll_index < 1200U)
    {
        /* Corrupt datagrams may be rejected while delivery recovers. */
        poll_result = client.poll();
        FT_ASSERT(poll_result == FT_ERR_SUCCESS
            || poll_result == FT_ERR_PERMISSION_DENIED
            || poll_result == FT_ERR_INVALID_ARGUMENT);
        poll_result = server.poll();
        FT_ASSERT(poll_result == FT_ERR_SUCCESS
            || poll_result == FT_ERR_PERMISSION_DENIED
            || poll_result == FT_ERR_INVALID_ARGUMENT);
        client_simulator.advance(10U);
        server_simulator.advance(10U);
        poll_index += 1U;
    }
    received_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(received, 32U,
        received_count));
    FT_ASSERT_EQ(32U, received_count);
    payload_index = 0U;
    while (payload_index < received_count)
    {
        uint32_t actual_payload;

        FT_ASSERT_EQ(sizeof(actual_payload), received[payload_index].payload.size());
        ft_memcpy(&actual_payload, &received[payload_index].payload[0],
            sizeof(actual_payload));
        FT_ASSERT_EQ(payloads[payload_index], actual_payload);
        payload_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_statistics(statistics));
    FT_ASSERT_EQ(0U, statistics.queue_bytes);
    FT_ASSERT_EQ(0U, statistics.queue_depth);
    FT_ASSERT(statistics.bytes_in_flight
        <= statistics.congestion_window_bytes);
    FT_ASSERT(statistics.packets_acknowledged > 0U);
    FT_ASSERT(statistics.smoothed_rtt_milliseconds > 0U);
    FT_ASSERT(statistics.latest_rtt_milliseconds > 0U);
    FT_ASSERT(statistics.minimum_rtt_milliseconds
        <= statistics.smoothed_rtt_milliseconds);
    FT_ASSERT(statistics.pacing_rate_bytes_per_second > 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_simulator.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_simulator.destroy());
    return (1);
}

FT_TEST(test_networking_reliable_delivery_seeded_impairment_trace)
{
    return (networking_reliability_run_seeded_impairment_trace(
        networking_reliability_soak_seed()));
}

FT_TEST(test_networking_reliable_delivery_seeded_impairment_trace_matrix)
{
    const char *trace_count_text;
    uint32_t trace_count;
    uint32_t trace_index;
    uint32_t trace_seed;

    trace_count = 1U;
    trace_count_text = std::getenv("LIBFT_NETWORKING_TRACE_COUNT");
    if (trace_count_text != ft_nullptr && trace_count_text[0] != '\0')
    {
        ft_size_t digit_index;

        digit_index = 0U;
        trace_count = 0U;
        while (trace_count_text[digit_index] != '\0')
        {
            uint32_t digit;

            if (trace_count_text[digit_index] < '0'
                || trace_count_text[digit_index] > '9')
                return (0);
            digit = static_cast<uint32_t>(
                trace_count_text[digit_index] - '0');
            if (trace_count > (10000U - digit) / 10U)
                return (0);
            trace_count = trace_count * 10U + digit;
            digit_index += 1U;
        }
        if (trace_count == 0U)
            return (0);
    }
    trace_seed = networking_reliability_soak_seed();
    trace_index = 0U;
    while (trace_index < trace_count)
    {
        if (networking_reliability_run_seeded_impairment_trace(trace_seed)
            == 0)
            return (0);
        trace_seed = trace_seed * 1664525U + 1013904223U;
        if (trace_seed == 0U)
            trace_seed = 1U;
        trace_index += 1U;
    }
    return (1);
}

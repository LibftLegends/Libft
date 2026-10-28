#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

FT_TEST(test_networking_message_performance_baseline)
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
    networking_received_message received;
    uint8_t payload[128];
    uint64_t latency_microseconds[128];
    uint64_t elapsed_microseconds;
    uint64_t throughput_messages_per_second;
    uint32_t message_index;
    uint32_t poll_attempt;
    uint32_t sort_index;
    uint32_t insert_index;
    int32_t receive_result;
    std::chrono::steady_clock::time_point total_start;
    std::chrono::steady_clock::time_point total_end;
    std::chrono::steady_clock::time_point message_start;
    std::chrono::steady_clock::time_point message_end;

    if (std::getenv("LIBFT_NETWORKING_PERF_BASELINE") == ft_nullptr
        || std::strcmp(std::getenv("LIBFT_NETWORKING_PERF_BASELINE"), "1")
            != 0)
        return (1);
    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    ft_memset(payload, 0x5a, sizeof(payload));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 1U;
    total_start = std::chrono::steady_clock::now();
    message_index = 0U;
    while (message_index < 128U)
    {
        message_start = std::chrono::steady_clock::now();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
            sizeof(payload), options));
        receive_result = FT_ERR_EMPTY;
        poll_attempt = 0U;
        while (poll_attempt < 64U && receive_result == FT_ERR_EMPTY)
        {
            int32_t client_poll_result = client.poll();
            int32_t server_poll_result = server.poll();

            FT_ASSERT(client_poll_result == FT_ERR_SUCCESS
                || client_poll_result == FT_ERR_EMPTY);
            FT_ASSERT(server_poll_result == FT_ERR_SUCCESS
                || server_poll_result == FT_ERR_EMPTY);
            receive_result = server.receive_message(received);
            if (receive_result == FT_ERR_EMPTY)
                poll_attempt += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
        FT_ASSERT_EQ(sizeof(payload), received.payload.size());
        FT_ASSERT_EQ(0, ft_memcmp(&received.payload[0U], payload,
            sizeof(payload)));
        message_end = std::chrono::steady_clock::now();
        latency_microseconds[message_index] = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                message_end - message_start).count());
        message_index += 1U;
    }
    total_end = std::chrono::steady_clock::now();
    elapsed_microseconds = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            total_end - total_start).count());
    if (elapsed_microseconds == 0U)
        elapsed_microseconds = 1U;
    throughput_messages_per_second =
        (128000000U / elapsed_microseconds);
    sort_index = 1U;
    while (sort_index < 128U)
    {
        uint64_t value = latency_microseconds[sort_index];

        insert_index = sort_index;
        while (insert_index > 0U
            && latency_microseconds[insert_index - 1U] > value)
        {
            latency_microseconds[insert_index]
                = latency_microseconds[insert_index - 1U];
            insert_index -= 1U;
        }
        latency_microseconds[insert_index] = value;
        sort_index += 1U;
    }
    std::printf("networking_performance messages=128 payload_bytes=128 elapsed_us=%" PRIu64
        " throughput_messages_per_second=%" PRIu64
        " latency_p50_us=%" PRIu64 " latency_p95_us=%" PRIu64 "\n",
        elapsed_microseconds, throughput_messages_per_second,
        latency_microseconds[63U], latency_microseconds[121U]);
    FT_ASSERT(throughput_messages_per_second >= 1000U);
    FT_ASSERT(latency_microseconds[121U] <= 50000U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

#include "../../Test/networking_message_fragmentation_test_support.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr ft_size_t BATCH_SIZE = 128U;
    constexpr uint32_t BATCH_COUNT = 100U;
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    configuration.enable_thread_safety = FT_TRUE;
    configuration.enable_encryption = FT_FALSE;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    if (transport.initialize(configuration, io) != FT_ERR_SUCCESS
        || transport.listen(endpoint) != FT_ERR_SUCCESS)
        return (1);
    networking_received_message messages[BATCH_SIZE];
    uint64_t packet_number = 1U;
    uint32_t batch = 0U;
    double elapsed_us = 0.0;
    while (batch < BATCH_COUNT)
    {
        ft_size_t index = 0U;
        while (index < BATCH_SIZE)
        {
            const uint8_t payload = static_cast<uint8_t>(index);
            uint8_t packet[128];
            const ft_size_t size = networking_fragmentation_make_packet(packet, packet_number,
                packet_number, 1U, 0U, 1U, &payload, 1U);
            if (transport.process_datagram(endpoint, packet, size) != FT_ERR_SUCCESS)
                return (1);
            ++index;
            ++packet_number;
        }
        ft_size_t received_count = 0U;
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = transport.receive_messages(messages, BATCH_SIZE, received_count);
        elapsed_us += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS || received_count != BATCH_SIZE)
            return (1);
        index = 0U;
        while (index < BATCH_SIZE)
        {
            if (messages[index].payload.size() != 1U || messages[index].payload[0] != index)
                return (1);
            ++index;
        }
        ++batch;
    }
    std::printf("%.3f us per batch, %.3f us per message (%u batches of %" FT_PRIu64 " messages)\n",
        elapsed_us / BATCH_COUNT, elapsed_us / (BATCH_COUNT * BATCH_SIZE), BATCH_COUNT, BATCH_SIZE);
    return (0);
}

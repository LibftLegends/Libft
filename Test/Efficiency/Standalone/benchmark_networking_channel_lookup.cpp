#include "../../Test/networking_message_fragmentation_test_support.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_channels(uint32_t channel_count)
{
    constexpr uint32_t ITERATIONS = 20000U;
    constexpr ft_size_t CHANNEL_OFFSET = 61U;
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    if (transport.initialize(configuration, io) != FT_ERR_SUCCESS
        || transport.listen(endpoint) != FT_ERR_SUCCESS)
        return (1);
    uint64_t packet_number = 1U;
    uint8_t payload = 42U;
    uint8_t packet[128];
    networking_received_message message;
    while (packet_number <= channel_count)
    {
        const uint32_t channel = (channel_count + 1U - static_cast<uint32_t>(packet_number)) * 4096U;
        const ft_size_t size = networking_fragmentation_make_packet(packet, packet_number,
            packet_number, 1U, 0U, 1U, &payload, 1U);
        networking_fragmentation_write_u32(packet + CHANNEL_OFFSET, channel);
        if (transport.process_datagram(endpoint, packet, size) != FT_ERR_SUCCESS
            || transport.receive_message(message) != FT_ERR_SUCCESS || message.channel != channel)
            return (1);
        ++packet_number;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const uint32_t channel = (iteration % channel_count + 1U) * 4096U;
        const ft_size_t size = networking_fragmentation_make_packet(packet, packet_number,
            packet_number, 1U, 0U, 1U, &payload, 1U);
        networking_fragmentation_write_u32(packet + CHANNEL_OFFSET, channel);
        if (transport.process_datagram(endpoint, packet, size) != FT_ERR_SUCCESS
            || transport.receive_message(message) != FT_ERR_SUCCESS || message.channel != channel)
            return (1);
        ++packet_number;
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%u channels: %.3f us per datagram delivery (%u datagrams)\n",
        channel_count, elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

int main()
{
    if (measure_channels(4U) != 0 || measure_channels(256U) != 0)
        return (1);
    return (0);
}

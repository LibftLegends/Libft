#include "../../../Modules/Networking/networking_replication_protocol.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_decode(ft_size_t payload_size)
{
    constexpr uint32_t ITERATIONS = 10000U;
    networking_received_message message;
    networking_replication_envelope envelope;
    networking_replication_envelope decoded;
    ft_byte_buffer header;
    ft_byte_buffer payload;
    envelope.message_type = 3U;
    envelope.payload_size = static_cast<uint32_t>(payload_size);
    envelope.server_instance_id = 10U;
    envelope.session_id = 11U;
    envelope.message_sequence = 12U;
    if (header.initialize() != FT_ERR_SUCCESS || payload.initialize() != FT_ERR_SUCCESS
        || message.payload.initialize() != FT_ERR_SUCCESS
        || networking_replication_envelope_serialize(envelope, header) != FT_ERR_SUCCESS)
        return (1);
    message.payload.resize(header.size() + payload_size);
    if (message.payload.get_error() != FT_ERR_SUCCESS)
        return (1);
    ft_memcpy(message.payload.begin(), header.data(), header.size());
    ft_memset(message.payload.begin() + header.size(), 0xA5, payload_size);
    if (networking_replication_decode_message(message, decoded, payload) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t frees = 0U;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (networking_replication_decode_message(message, decoded, payload) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS || payload.size() != payload_size
        || decoded.message_sequence != envelope.message_sequence
        || ft_memcmp(payload.data(), message.payload.begin() + header.size(), payload_size) != 0)
        return (1);
    std::printf("%" FT_PRIu64 " bytes: %.3f us per decode, %" FT_PRIu64 " allocations for %u messages\n",
        payload_size, elapsed_us / ITERATIONS, after - before, ITERATIONS);
    return (0);
}

int main()
{
    if (measure_decode(128U) != 0 || measure_decode(65536U) != 0)
        return (1);
    return (0);
}

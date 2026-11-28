#include "../../../Modules/Game/game_world_delta.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

template <typename Message,
    int32_t (*Encode)(const Message &, ft_byte_buffer &) noexcept,
    int32_t (*Decode)(Message &, ft_byte_buffer &) noexcept>
static int32_t measure_wire(const char *label, Message &message)
{
    constexpr uint32_t ITERATIONS = 131072U;
    ft_byte_buffer wire;
    if (wire.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        Message decoded;
        message.request_id = iteration + 1U;
        if (wire.clear() != FT_ERR_SUCCESS || Encode(message, wire) != FT_ERR_SUCCESS
            || Decode(decoded, wire) != FT_ERR_SUCCESS || decoded.request_id != message.request_id
            || wire.remaining() != 0U)
            return (1);
        checksum += decoded.request_id + decoded.world_id;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const ft_size_t size = wire.size();
    const uint8_t *bytes = wire.data();
    ft_size_t offset = 0U;
    while (offset < size)
        checksum = checksum * 31U + bytes[offset++];
    std::printf("%s %.2f ns/roundtrip bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        label, elapsed / ITERATIONS, static_cast<uint64_t>(size), checksum);
    return (0);
}

int main()
{
    game_block_change_request request = {GAME_WORLD_DELTA_PROTOCOL_VERSION,
        10U, 1U, 2U, -9, 12, 500U, 77U, 78U, 1U, 255U, 15U};
    game_block_delta delta = {GAME_WORLD_DELTA_PROTOCOL_VERSION,
        10U, 1U, 2U, -9, 12, 500U, 501U, 78U, FT_TRUE, 10000U, 1U, 255U, 15U};
    if (measure_wire<game_block_change_request, game_block_change_request_serialize,
            game_block_change_request_deserialize>("request", request) != 0
        || measure_wire<game_block_delta, game_block_delta_serialize,
            game_block_delta_deserialize>("delta", delta) != 0)
        return (1);
    return (0);
}

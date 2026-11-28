#include "../../../Modules/Game/game_world_delta.hpp"
#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_snapshot(uint32_t edits)
{
    constexpr uint32_t PASSES = 256U;
    constexpr uint32_t CHUNK_BLOCKS = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_HEIGHT
        * GAME_VOXEL_CHUNK_DEPTH;
    game_voxel_chunk chunk;
    ft_byte_buffer buffer;
    if (chunk.initialize() != FT_ERR_SUCCESS || buffer.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t edit = 0U;
    while (edit < edits)
    {
        const uint32_t coordinate = (edit * 1549U) % CHUNK_BLOCKS;
        if (chunk.write_block(coordinate & 15U, coordinate >> 8U,
                (coordinate >> 4U) & 15U, 2U + edit % 5U) != FT_ERR_SUCCESS)
            return (1);
        ++edit;
    }
    uint32_t pass = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (pass < PASSES)
    {
        if (buffer.clear() != FT_ERR_SUCCESS
            || game_world_delta_snapshot_serialize(chunk, buffer) != FT_ERR_SUCCESS)
            return (1);
        ++pass;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    game_voxel_chunk restored;
    if (restored.initialize() != FT_ERR_SUCCESS)
        return (1);
    pass = 0U;
    const auto decode_start = std::chrono::steady_clock::now();
    while (pass < PASSES)
    {
        if (buffer.reset_read_position() != FT_ERR_SUCCESS
            || game_world_delta_snapshot_deserialize(restored, buffer) != FT_ERR_SUCCESS)
            return (1);
        ++pass;
    }
    const double decode_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - decode_start).count();
    const uint8_t *wire = buffer.data();
    const ft_size_t size = buffer.size();
    if (buffer.read_position() != size || restored.get_revision() != chunk.get_revision()
        || restored.get_player_override_count() != edits)
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < size)
    {
        checksum = checksum * 31U + wire[index];
        ++index;
    }
    std::printf("edits %" PRIu32 " encode %.2f us/frame decode %.2f us/frame bytes=%" PRIu64
        " checksum=%" PRIu64 "\n",
        edits, elapsed / PASSES, decode_elapsed / PASSES, static_cast<uint64_t>(size), checksum);
    return (0);
}

int main()
{
    if (measure_snapshot(2U) != 0 || measure_snapshot(4096U) != 0)
        return (1);
    return (0);
}

#include "../../../Modules/Game/game_block_edit_op.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 8192U;
    ft_byte_buffer buffer;
    if (buffer.initialize(24U, FT_TRUE) != FT_ERR_SUCCESS
        || buffer.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    game_block_edit_op source{-17, 64, -312, 4U, 0U};
    game_block_edit_op restored{};
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        source.tick = iteration;
        if (buffer.clear() != FT_ERR_SUCCESS
            || game_block_edit_op_serialize(source, buffer) != FT_ERR_SUCCESS
            || game_block_edit_op_deserialize(restored, buffer) != FT_ERR_SUCCESS
            || restored.world_x != source.world_x || restored.world_y != source.world_y
            || restored.world_z != source.world_z || restored.block_type != source.block_type)
            return (1);
        checksum += restored.tick;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("block edit round trip %.2f ns/op, checksum %" PRIu64 "\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}

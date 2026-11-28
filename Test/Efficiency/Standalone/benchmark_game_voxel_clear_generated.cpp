#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_clear(int32_t generated_height)
{
    constexpr uint32_t PASSES = 12U;
    game_voxel_chunk chunk;
    if (chunk.initialize() != FT_ERR_SUCCESS
        || chunk.write_block(0, 0, 0, 2U) != FT_ERR_SUCCESS)
        return (1);
    double elapsed = 0.0;
    uint64_t checksum = 0U;
    uint32_t pass = 0U;
    while (pass < PASSES)
    {
        int32_t local_y = 0;
        while (local_y < generated_height)
        {
            int32_t column = 0;
            while (column < GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH)
            {
                if (chunk.write_generated_block(column % GAME_VOXEL_CHUNK_WIDTH, local_y,
                        column / GAME_VOXEL_CHUNK_WIDTH, 1U) != FT_ERR_SUCCESS)
                    return (1);
                ++column;
            }
            ++local_y;
        }
        const auto start = std::chrono::steady_clock::now();
        const int32_t status = chunk.clear_generated_blocks();
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        uint32_t generated = 0U;
        uint32_t retained = 0U;
        uint32_t cleared = 0U;
        if (status != FT_ERR_SUCCESS || chunk.get_player_override_count() != 1U
            || chunk.get_generated_block(0, 0, 0, &generated) != FT_ERR_SUCCESS
            || chunk.read_block(0, 0, 0, &retained) != FT_ERR_SUCCESS
            || chunk.read_block(15, generated_height - 1, 15, &cleared) != FT_ERR_SUCCESS
            || generated != GAME_VOXEL_AIR_BLOCK || retained != 2U
            || cleared != GAME_VOXEL_AIR_BLOCK)
            return (1);
        checksum += retained;
        ++pass;
    }
    std::printf("height %" PRId32 " clear %.2f us/chunk checksum=%" PRIu64 "\n",
        generated_height, elapsed / PASSES, checksum);
    return (0);
}

int main()
{
    if (measure_clear(16) != 0 || measure_clear(80) != 0
        || measure_clear(GAME_VOXEL_CHUNK_HEIGHT) != 0)
        return (1);
    return (0);
}

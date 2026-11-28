#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

static int32_t measure_snapshots(ft_bool dense)
{
    constexpr uint32_t iterations = 256U;
    game_voxel_chunk chunk;
    if (chunk.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (dense == FT_TRUE)
    {
        int32_t y = 0;
        while (y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            if (chunk.write_generated_block(0, y, 0, 1U) != FT_ERR_SUCCESS)
                return (1);
            ++y;
        }
    }
    constexpr uint32_t plane = GAME_VOXEL_CHUNK_HEIGHT * GAME_VOXEL_CHUNK_WIDTH;
    std::vector<uint32_t> blocks(plane * GAME_VOXEL_CHUNK_DEPTH);
    std::vector<uint32_t> x_border(plane);
    std::vector<uint32_t> z_border(plane);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (chunk.copy_blocks(blocks.data(), plane * GAME_VOXEL_CHUNK_DEPTH) != FT_ERR_SUCCESS
            || chunk.copy_x_border(x_border.data(), plane, 0U) != FT_ERR_SUCCESS
            || chunk.copy_z_border(z_border.data(), plane, 0U) != FT_ERR_SUCCESS)
            return (1);
        checksum += blocks[index * GAME_VOXEL_CHUNK_WIDTH]
            + x_border[index * GAME_VOXEL_CHUNK_DEPTH] + z_border[index * GAME_VOXEL_CHUNK_WIDTH];
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("materialized=%u %.2f us/snapshot set checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(dense), elapsed / iterations, checksum);
    return (0);
}

int32_t main()
{
    if (measure_snapshots(FT_FALSE) != 0)
        return (1);
    return (measure_snapshots(FT_TRUE));
}

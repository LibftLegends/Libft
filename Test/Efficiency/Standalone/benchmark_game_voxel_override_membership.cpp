#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_compaction(uint32_t retained)
{
    constexpr uint32_t EDITS = 4096U;
    constexpr uint32_t PASSES = 32U;
    const uint32_t prefix = EDITS - retained;
    uint32_t pass = 0U;
    double elapsed = 0.0;
    while (pass < PASSES)
    {
        game_voxel_chunk chunk;
        if (chunk.initialize() != FT_ERR_SUCCESS)
            return (1);
        uint32_t index = 0U;
        while (index < EDITS)
        {
            if (chunk.write_block(index % 16U, index / 256U, (index / 16U) % 16U, 42U)
                != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        index = 0U;
        while (index < prefix)
        {
            if (chunk.write_block(index % 16U, index / 256U, (index / 16U) % 16U,
                    GAME_VOXEL_AIR_BLOCK) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = chunk.write_block(0, 0, 0, 87U);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        game_voxel_block_override first;
        game_voxel_block_override last;
        if (result != FT_ERR_SUCCESS || chunk.get_player_override_count() != retained + 1U
            || chunk.get_player_override(0U, &first) != FT_ERR_SUCCESS || first.local_index != prefix
            || chunk.get_player_override(retained, &last) != FT_ERR_SUCCESS
            || last.local_index != 0U || last.current_block_id != 87U)
            return (1);
        ++pass;
    }
    std::printf("override compaction %.2f ns/edit\n", elapsed / PASSES);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--compact") == 0)
        return (measure_compaction(3072U));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--compact-sparse") == 0)
        return (measure_compaction(128U));
    constexpr uint32_t edits = 4096U;
    constexpr uint32_t probes = 32768U;
    game_voxel_chunk chunk;
    if (chunk.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < edits)
    {
        if (chunk.write_block(index % 16U, index / 256U, (index / 16U) % 16U, 42U)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto written = std::chrono::steady_clock::now();
    index = 0U;
    uint32_t misses = 0U;
    while (index < probes)
    {
        if (chunk.is_block_player_modified(index % 16U, 16U + index / 256U,
                (index / 16U) % 16U) == FT_FALSE)
            ++misses;
        ++index;
    }
    const auto absent_checked = std::chrono::steady_clock::now();
    index = 0U;
    uint32_t hits = 0U;
    while (index < probes)
    {
        const uint32_t position = index % edits;
        hits += chunk.is_block_player_modified(position % 16U, position / 256U,
            (position / 16U) % 16U);
        ++index;
    }
    const auto present_checked = std::chrono::steady_clock::now();
    if (hits != probes || misses != probes || chunk.get_player_override_count() != edits)
        return (1);
    constexpr uint32_t removals = 128U;
    index = 0U;
    while (index < removals)
    {
        if (chunk.write_block(index % 16U, index / 256U, (index / 16U) % 16U,
                GAME_VOXEL_AIR_BLOCK) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto removed = std::chrono::steady_clock::now();
    if (chunk.get_player_override_count() != edits - removals)
        return (1);
    std::printf("insert %.2f ns/edit absent %.2f ns/probe present %.2f ns/probe "
        "front removal %.2f ns/edit hits=%u misses=%u\n",
        std::chrono::duration<double, std::nano>(written - start).count() / edits,
        std::chrono::duration<double, std::nano>(absent_checked - written).count() / probes,
        std::chrono::duration<double, std::nano>(present_checked - absent_checked).count() / probes,
        std::chrono::duration<double, std::nano>(removed - present_checked).count() / removals,
        hits, misses);
    return (0);
}

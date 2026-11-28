#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/Game/game_world_delta.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t EDIT_COUNT = GAME_VOXEL_SECTION_BLOCKS;
    constexpr uint32_t PASSES = 24U;
    game_voxel_chunk server;
    game_voxel_chunk client;
    game_block_delta deltas[EDIT_COUNT];
    if (server.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < EDIT_COUNT)
    {
        if (server.write_block(index & 15U, index >> 8U, (index >> 4U) & 15U, 2U)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (client.initialize(server) != FT_ERR_SUCCESS)
        return (1);

    game_block_change_request request = {};
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.world_id = 1U;
    request.session_id = 2U;
    request.expected_revision = server.get_revision();
    double server_elapsed = 0.0;
    double client_elapsed = 0.0;
    uint32_t pass = 0U;
    while (pass < PASSES)
    {
        request.expected_block_id = 2U + (pass & 1U);
        request.requested_block_id = 2U + ((pass + 1U) & 1U);
        index = 0U;
        const auto server_start = std::chrono::steady_clock::now();
        while (index < EDIT_COUNT)
        {
            const uint32_t coordinate = (index * 1549U) % EDIT_COUNT;
            request.local_x = coordinate & 15U;
            request.local_y = coordinate >> 8U;
            request.local_z = (coordinate >> 4U) & 15U;
            request.request_id += 1U;
            if (server.apply_authoritative_block_change(request, &deltas[index]) != FT_ERR_SUCCESS
                || deltas[index].player_modified != FT_TRUE)
                return (1);
            request.expected_revision += 1U;
            ++index;
        }
        server_elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - server_start).count();

        index = 0U;
        const auto client_start = std::chrono::steady_clock::now();
        while (index < EDIT_COUNT)
        {
            if (client.apply_authoritative_block_delta(deltas[index]) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        client_elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - client_start).count();
        ++pass;
    }

    const uint64_t revision = client.get_revision();
    if (revision != request.expected_revision || server.get_revision() != revision
        || client.get_player_override_count() != EDIT_COUNT || server.get_player_override_count() != EDIT_COUNT)
        return (1);
    uint64_t checksum = 0U;
    index = 0U;
    while (index < EDIT_COUNT)
    {
        game_voxel_block_override record;
        if (client.get_player_override(index, &record) != FT_ERR_SUCCESS
            || record.local_index != index || record.current_block_id != request.requested_block_id)
            return (1);
        checksum = checksum * 31U + record.current_block_id;
        ++index;
    }
    constexpr uint32_t TOTAL_EDITS = EDIT_COUNT * PASSES;
    std::printf("server %.2f ns/edit client %.2f ns/delta revision=%" PRIu64 " checksum=%" PRIu64 "\n",
        server_elapsed / TOTAL_EDITS, client_elapsed / TOTAL_EDITS, revision, checksum);
    return (0);
}

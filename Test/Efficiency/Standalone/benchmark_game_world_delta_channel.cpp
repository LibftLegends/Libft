#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/Game/game_world_delta.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t REQUEST_COUNT = 131072U;
    game_voxel_chunk chunk;
    game_world_delta_channel channel;
    ft_vector<game_block_delta> recovered;
    if (chunk.initialize() != FT_ERR_SUCCESS
        || chunk.write_generated_block(1, 17, 3, 1U) != FT_ERR_SUCCESS
        || channel.initialize(chunk, 1U, 0, 0) != FT_ERR_SUCCESS
        || recovered.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_block_change_request request = {};
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.world_id = 1U;
    request.session_id = 2U;
    request.local_x = 1U;
    request.local_y = 17U;
    request.local_z = 3U;
    request.expected_block_id = 1U;
    game_block_delta delta;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (request.request_id < REQUEST_COUNT)
    {
        request.requested_block_id = 2U + (request.request_id & 1U);
        request.request_id += 1U;
        if (channel.apply_request(request, delta) != FT_ERR_SUCCESS)
            return (1);
        checksum += delta.revision + delta.server_tick;
        request.expected_revision = delta.revision;
        request.expected_block_id = delta.current_block_id;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (channel.get_revision() != REQUEST_COUNT
        || channel.recover_from(REQUEST_COUNT - 1U, recovered) != FT_ERR_SUCCESS
        || recovered.size() != 1U || recovered[0U].server_tick != REQUEST_COUNT
        || recovered[0U].current_block_id != request.requested_block_id)
        return (1);
    std::printf("channel %.2f ns/request checksum=%" PRIu64 "\n",
        elapsed / REQUEST_COUNT, checksum);
    return (0);
}

#include "../../Modules/Game/game_world_delta.hpp"
#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_world_delta_request_and_delta_round_trip)
{
    game_block_change_request request;
    game_block_change_request restored_request;
    game_block_delta delta;
    game_block_delta restored_delta;
    ft_byte_buffer buffer;

    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 11U;
    request.request_id = 12U;
    request.world_id = 13U;
    request.chunk_x = -14;
    request.chunk_z = 15;
    request.expected_revision = 16U;
    request.expected_block_id = 17U;
    request.requested_block_id = 18U;
    request.local_x = 1U;
    request.local_y = 2U;
    request.local_z = 3U;
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    delta.session_id = 21U;
    delta.request_id = 22U;
    delta.world_id = 23U;
    delta.chunk_x = -24;
    delta.chunk_z = 25;
    delta.previous_revision = 26U;
    delta.revision = 27U;
    delta.current_block_id = 28U;
    delta.player_modified = FT_TRUE;
    delta.server_tick = 29U;
    delta.local_x = 4U;
    delta.local_y = 5U;
    delta.local_z = 6U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_block_change_request_serialize(request, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_block_change_request_deserialize(restored_request, buffer));
    FT_ASSERT_EQ(request.request_id, restored_request.request_id);
    FT_ASSERT_EQ(request.chunk_x, restored_request.chunk_x);
    FT_ASSERT_EQ(request.chunk_z, restored_request.chunk_z);
    FT_ASSERT_EQ(request.requested_block_id,
        restored_request.requested_block_id);
    FT_ASSERT_EQ(request.local_y, restored_request.local_y);
    buffer.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_delta_serialize(delta, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_delta_deserialize(restored_delta,
        buffer));
    FT_ASSERT_EQ(delta.revision, restored_delta.revision);
    FT_ASSERT_EQ(delta.current_block_id, restored_delta.current_block_id);
    FT_ASSERT_EQ(delta.player_modified, restored_delta.player_modified);
    FT_ASSERT_EQ(delta.server_tick, restored_delta.server_tick);
    return (1);
}

FT_TEST(test_game_world_delta_deserialization_is_transactional)
{
    game_block_delta delta;
    game_block_delta original_delta;
    ft_byte_buffer buffer;

    original_delta.protocol_version = 9U;
    original_delta.session_id = 8U;
    original_delta.request_id = 7U;
    original_delta.world_id = 6U;
    original_delta.chunk_x = -5;
    original_delta.chunk_z = 4;
    original_delta.previous_revision = 3U;
    original_delta.revision = 4U;
    original_delta.current_block_id = 5U;
    original_delta.player_modified = FT_TRUE;
    original_delta.server_tick = 10U;
    original_delta.local_x = 4U;
    original_delta.local_y = 3U;
    original_delta.local_z = 2U;
    delta = original_delta;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(1U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        game_block_delta_deserialize(delta, buffer));
    FT_ASSERT_EQ(original_delta.request_id, delta.request_id);
    FT_ASSERT_EQ(original_delta.current_block_id, delta.current_block_id);
    return (1);
}

FT_TEST(test_game_voxel_chunk_authoritative_change_checks_revision)
{
    game_voxel_chunk chunk;
    game_block_change_request request;
    game_block_delta delta;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_generated_block(1, 2, 3, 5U));
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 100U;
    request.request_id = 200U;
    request.world_id = 300U;
    request.chunk_x = -2;
    request.chunk_z = 3;
    request.expected_revision = 0U;
    request.expected_block_id = 5U;
    request.requested_block_id = 8U;
    request.local_x = 1U;
    request.local_y = 2U;
    request.local_z = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        chunk.apply_authoritative_block_change(request, &delta));
    FT_ASSERT_EQ(0U, delta.previous_revision);
    FT_ASSERT_EQ(1U, delta.revision);
    FT_ASSERT_EQ(FT_TRUE, delta.player_modified);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(8U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        chunk.apply_authoritative_block_change(request, &delta));
    FT_ASSERT_EQ(1U, delta.revision);
    request.requested_block_id = 10U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        chunk.apply_authoritative_block_change(request, &delta));
    request.requested_block_id = 8U;
    request.request_id += 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        chunk.apply_authoritative_block_change(request, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(8U, block_id);
    return (1);
}

FT_TEST(test_game_voxel_chunk_applies_authoritative_delta_once)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    game_block_change_request request;
    game_block_delta delta;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_chunk.write_generated_block(2, 3, 4, 6U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.write_generated_block(2, 3, 4, 6U));
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 1U;
    request.request_id = 2U;
    request.world_id = 3U;
    request.chunk_x = 4;
    request.chunk_z = -5;
    request.expected_revision = 0U;
    request.expected_block_id = 6U;
    request.requested_block_id = 9U;
    request.local_x = 2U;
    request.local_y = 3U;
    request.local_z = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_chunk.apply_authoritative_block_change(request, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.apply_authoritative_block_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.read_block(2, 3, 4, &block_id));
    FT_ASSERT_EQ(9U, block_id);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        destination_chunk.apply_authoritative_block_delta(delta));
    return (1);
}

FT_TEST(test_game_world_delta_history_recovers_or_requests_snapshot)
{
    game_world_delta_history history;
    ft_vector<game_block_delta> recovered;
    game_block_delta first = {};
    game_block_delta second = {};
    game_block_delta third = {};

    first.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    first.previous_revision = 0U;
    first.revision = 1U;
    second = first;
    second.previous_revision = 1U;
    second.revision = 2U;
    third = second;
    third.previous_revision = 2U;
    third.revision = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.initialize(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(third));
    FT_ASSERT_EQ(2U, history.size());
    FT_ASSERT_EQ(2U, history.get_oldest_revision());
    FT_ASSERT_EQ(3U, history.get_latest_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.get_since(1U, recovered));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2U), recovered.size());
    FT_ASSERT_EQ(2U, recovered[0U].revision);
    FT_ASSERT_EQ(3U, recovered[1U].revision);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, history.get_since(0U, recovered));
    return (1);
}

FT_TEST(test_game_world_delta_interest_snapshot_handoff)
{
    game_world_delta_interest_set interests;
    ft_vector<uint64_t> live_clients;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live_clients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(10U, 20U, -1, 2, 7U));
    FT_ASSERT(interests.is_snapshot_pending(10U, 20U, -1, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(20U, -1,
        2, live_clients));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0U), live_clients.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(10U, 20U,
        -1, 2, 7U));
    FT_ASSERT(!interests.is_snapshot_pending(10U, 20U, -1, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(20U, -1,
        2, live_clients));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), live_clients.size());
    FT_ASSERT_EQ(10U, live_clients[0U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_revision(10U, 20U,
        -1, 2, 8U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, interests.acknowledge_revision(10U,
        20U, -1, 2, 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.unsubscribe(10U, 20U, -1, 2));
    FT_ASSERT_EQ(0U, interests.size());
    return (1);
}

FT_TEST(test_game_world_delta_channel_authority_and_recovery)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    game_world_delta_channel channel;
    ft_vector<uint64_t> live_clients;
    ft_vector<game_block_delta> recovered;
    ft_byte_buffer snapshot;
    game_block_change_request request = {};
    game_block_delta delta = {};
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_generated_block(
        1, 2, 3, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.write_generated_block(
        1, 2, 3, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.initialize(source_chunk, 99U,
        -4, 8, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live_clients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.subscribe(10U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.subscribe(11U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.acknowledge_snapshot(10U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.serialize_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_world_delta_snapshot_deserialize(destination_chunk, snapshot));
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 1U;
    request.request_id = 2U;
    request.world_id = 99U;
    request.chunk_x = -4;
    request.chunk_z = 8;
    request.expected_block_id = 4U;
    request.requested_block_id = 12U;
    request.local_x = 1U;
    request.local_y = 2U;
    request.local_z = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.apply_request(request, delta));
    FT_ASSERT_EQ(1U, channel.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.collect_live_clients(live_clients));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), live_clients.size());
    FT_ASSERT_EQ(10U, live_clients[0U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.recover_from(0U, recovered));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), recovered.size());
    FT_ASSERT_EQ(1U, recovered[0U].revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.apply_authoritative_block_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 2, 3,
        &block_id));
    FT_ASSERT_EQ(12U, block_id);
    return (1);
}

FT_TEST(test_game_world_delta_snapshot_checksum_rejects_corruption)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer snapshot;
    ft_byte_buffer corrupted_snapshot;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_generated_block(
        5, 6, 7, 41U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(5, 6, 7, 42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.write_generated_block(
        5, 6, 7, 41U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_serialize(
        source_chunk, snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_world_delta_snapshot_deserialize(destination_chunk, snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(5, 6, 7,
        &block_id));
    FT_ASSERT_EQ(42U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, corrupted_snapshot.initialize(snapshot));
    corrupted_snapshot._data[corrupted_snapshot.size() - 1U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, corrupted_snapshot.reset_read_position());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        game_world_delta_snapshot_deserialize(destination_chunk,
            corrupted_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(5, 6, 7,
        &block_id));
    FT_ASSERT_EQ(42U, block_id);
    return (1);
}

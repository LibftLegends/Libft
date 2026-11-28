#include "../../Modules/Game/game_world_delta.hpp"
#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/CMA/CMA.hpp"
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
    constexpr uint8_t REQUEST_WIRE[] = {
        0x01, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xf2, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x12, 0x00,
        0x00, 0x00, 0x01, 0x02, 0x00, 0x03
    };
    FT_ASSERT_EQ(sizeof(REQUEST_WIRE), buffer.size());
    FT_ASSERT_EQ(0, ft_memcmp(REQUEST_WIRE, buffer.data(), sizeof(REQUEST_WIRE)));
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
    constexpr uint8_t DELTA_WIRE[] = {
        0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xe8, 0xff, 0xff, 0xff, 0x19, 0x00, 0x00, 0x00, 0x1a, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x06
    };
    FT_ASSERT_EQ(sizeof(DELTA_WIRE), buffer.size());
    FT_ASSERT_EQ(0, ft_memcmp(DELTA_WIRE, buffer.data(), sizeof(DELTA_WIRE)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_delta_deserialize(restored_delta,
        buffer));
    FT_ASSERT_EQ(delta.revision, restored_delta.revision);
    FT_ASSERT_EQ(delta.current_block_id, restored_delta.current_block_id);
    FT_ASSERT_EQ(delta.player_modified, restored_delta.player_modified);
    FT_ASSERT_EQ(delta.server_tick, restored_delta.server_tick);

    ft_byte_buffer limited;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, limited.initialize(15U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, limited.append_u8(0xABU));
    FT_ASSERT_EQ(FT_ERR_FULL, game_block_change_request_serialize(request, limited));
    FT_ASSERT_EQ(11U, limited.size());
    FT_ASSERT_EQ(0xABU, limited.data()[0U]);
    FT_ASSERT_EQ(0, ft_memcmp(REQUEST_WIRE, limited.data() + 1U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, limited.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, limited.append_u8(0xABU));
    FT_ASSERT_EQ(FT_ERR_FULL, game_block_delta_serialize(delta, limited));
    FT_ASSERT_EQ(11U, limited.size());
    FT_ASSERT_EQ(0xABU, limited.data()[0U]);
    FT_ASSERT_EQ(0, ft_memcmp(DELTA_WIRE, limited.data() + 1U, 10U));
    return (1);
}

FT_TEST(test_game_world_delta_deserialization_is_transactional)
{
    game_block_delta delta;
    game_block_delta original_delta;
    ft_byte_buffer buffer;
    ft_size_t initial_read_position;

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
    initial_read_position = buffer.read_position();
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        game_block_delta_deserialize(delta, buffer));
    FT_ASSERT_EQ(initial_read_position, buffer.read_position());
    FT_ASSERT_EQ(original_delta.request_id, delta.request_id);
    FT_ASSERT_EQ(original_delta.current_block_id, delta.current_block_id);
    return (1);
}

FT_TEST(test_game_world_delta_request_deserialization_preserves_cursor)
{
    game_block_change_request request;
    game_block_change_request original_request;
    ft_byte_buffer buffer;
    ft_size_t initial_read_position;

    original_request.protocol_version = 99U;
    original_request.request_id = 88U;
    original_request.requested_block_id = 77U;
    request = original_request;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(
        GAME_WORLD_DELTA_PROTOCOL_VERSION));
    initial_read_position = buffer.read_position();
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        game_block_change_request_deserialize(request, buffer));
    FT_ASSERT_EQ(initial_read_position, buffer.read_position());
    FT_ASSERT_EQ(original_request.request_id, request.request_id);
    FT_ASSERT_EQ(original_request.requested_block_id,
        request.requested_block_id);
    return (1);
}

FT_TEST(test_game_world_delta_decoders_reject_every_truncation)
{
    game_block_change_request request;
    game_block_change_request original_request = {};
    game_block_delta delta;
    game_block_delta original_delta = {};
    ft_byte_buffer encoded_request;
    ft_byte_buffer encoded_delta;
    ft_byte_buffer truncated;
    ft_size_t truncated_size;

    original_request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    original_request.session_id = 11U;
    original_request.request_id = 12U;
    original_request.world_id = 13U;
    original_request.chunk_x = -14;
    original_request.chunk_z = 15;
    original_request.expected_revision = 16U;
    original_request.expected_block_id = 17U;
    original_request.requested_block_id = 18U;
    original_request.local_x = 1U;
    original_request.local_y = 2U;
    original_request.local_z = 3U;
    original_delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    original_delta.session_id = 21U;
    original_delta.request_id = 22U;
    original_delta.world_id = 23U;
    original_delta.chunk_x = -24;
    original_delta.chunk_z = 25;
    original_delta.previous_revision = 26U;
    original_delta.revision = 27U;
    original_delta.current_block_id = 28U;
    original_delta.player_modified = FT_TRUE;
    original_delta.server_tick = 29U;
    original_delta.local_x = 4U;
    original_delta.local_y = 5U;
    original_delta.local_z = 6U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_request.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_delta.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_change_request_serialize(
        original_request, encoded_request));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_delta_serialize(original_delta,
        encoded_delta));
    truncated_size = 0U;
    while (truncated_size < encoded_request.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.append(encoded_request.data(),
            truncated_size));
        request = original_request;
        FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
            game_block_change_request_deserialize(request, truncated));
        FT_ASSERT_EQ(static_cast<ft_size_t>(0U), truncated.read_position());
        FT_ASSERT_EQ(original_request.request_id, request.request_id);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.destroy());
        truncated_size += 1U;
    }
    truncated_size = 0U;
    while (truncated_size < encoded_delta.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.append(encoded_delta.data(),
            truncated_size));
        delta = original_delta;
        FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
            game_block_delta_deserialize(delta, truncated));
        FT_ASSERT_EQ(static_cast<ft_size_t>(0U), truncated.read_position());
        FT_ASSERT_EQ(original_delta.request_id, delta.request_id);
        FT_ASSERT_EQ(original_delta.revision, delta.revision);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.destroy());
        truncated_size += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_request.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_delta.destroy());
    return (1);
}

FT_TEST(test_game_world_delta_rejects_zero_identity_fields)
{
    game_block_change_request request = {};
    game_block_delta delta = {};
    ft_byte_buffer request_buffer;
    ft_byte_buffer delta_buffer;

    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 0U;
    request.request_id = 2U;
    request.world_id = 3U;
    request.local_x = 1U;
    request.local_y = 2U;
    request.local_z = 3U;
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    delta.session_id = 1U;
    delta.request_id = 0U;
    delta.world_id = 3U;
    delta.previous_revision = 0U;
    delta.revision = 1U;
    delta.local_x = 1U;
    delta.local_y = 2U;
    delta.local_z = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, delta_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_change_request_serialize(
        request, request_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_delta_serialize(delta,
        delta_buffer));
    request = {};
    delta = {};
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        game_block_change_request_deserialize(request, request_buffer));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        game_block_delta_deserialize(delta, delta_buffer));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0U), request_buffer.read_position());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0U), delta_buffer.read_position());
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

    request.request_id += 1U;
    request.expected_revision = delta.revision;
    request.expected_block_id = 9U;
    request.requested_block_id = 6U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_chunk.apply_authoritative_block_change(request, &delta));
    FT_ASSERT_EQ(FT_FALSE, delta.player_modified);
    FT_ASSERT_EQ(0U, source_chunk.get_player_override_count());
    delta.player_modified = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        destination_chunk.apply_authoritative_block_delta(delta));
    FT_ASSERT_EQ(1U, destination_chunk.get_revision());
    FT_ASSERT_EQ(1U, destination_chunk.get_player_override_count());
    delta.player_modified = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.apply_authoritative_block_delta(delta));
    FT_ASSERT_EQ(2U, destination_chunk.get_revision());
    FT_ASSERT_EQ(0U, destination_chunk.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.read_block(2, 3, 4, &block_id));
    FT_ASSERT_EQ(6U, block_id);
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
    FT_ASSERT_EQ(static_cast<ft_size_t>(2U), recovered.size());
    return (1);
}

FT_TEST(test_game_world_delta_history_wraps_and_recovers_suffix)
{
    game_world_delta_history history;
    ft_vector<game_block_delta> recovered;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.initialize(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.initialize());
    game_block_delta delta = {};
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    uint64_t revision = 10U;
    while (revision <= 16U)
    {
        delta.previous_revision = revision - 1U;
        delta.revision = revision;
        delta.current_block_id = static_cast<uint32_t>(revision);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(delta));
        ++revision;
    }
    delta.previous_revision = 17U;
    delta.revision = 18U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, history.append(delta));
    FT_ASSERT_EQ(3U, history.size());
    FT_ASSERT_EQ(14U, history.get_oldest_revision());
    FT_ASSERT_EQ(16U, history.get_latest_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.get_since(14U, recovered));
    FT_ASSERT_EQ(2U, recovered.size());
    FT_ASSERT_EQ(15U, recovered[0U].revision);
    FT_ASSERT_EQ(16U, recovered[1U].revision);
    FT_ASSERT_EQ(16U, recovered[1U].current_block_id);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, history.get_since(12U, recovered));
    FT_ASSERT_EQ(2U, recovered.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.get_since(UINT64_MAX, recovered));
    FT_ASSERT_EQ(0U, recovered.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.initialize(1U));
    delta.previous_revision = UINT64_MAX - 2U;
    delta.revision = UINT64_MAX - 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(delta));
    delta.previous_revision = UINT64_MAX - 1U;
    delta.revision = UINT64_MAX;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(delta));
    FT_ASSERT_EQ(UINT64_MAX, history.get_oldest_revision());
    FT_ASSERT_EQ(UINT64_MAX, history.get_latest_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.get_since(UINT64_MAX - 1U, recovered));
    FT_ASSERT_EQ(1U, recovered.size());
    FT_ASSERT_EQ(UINT64_MAX, recovered[0U].revision);
    return (1);
}

FT_TEST(test_game_world_delta_history_keeps_recovered_output_on_allocation_failure)
{
    game_world_delta_history history;
    ft_vector<game_block_delta> recovered;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.initialize(128U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.initialize());
    game_block_delta delta = {};
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    while (delta.revision < 129U)
    {
        delta.previous_revision = delta.revision;
        ++delta.revision;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, history.append(delta));
    }
    delta.revision = 999U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.push_back(delta));
    FT_ASSERT(recovered.capacity() < 128U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = history.get_since(1U, recovered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(1U, recovered.size());
    FT_ASSERT_EQ(999U, recovered[0U].revision);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, history.get_since(1U, recovered));
    FT_ASSERT_EQ(128U, recovered.size());
    ft_size_t index = 0U;
    while (index < recovered.size())
    {
        FT_ASSERT_EQ(index + 2U, recovered[index].revision);
        ++index;
    }
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

FT_TEST(test_game_world_delta_interest_groups_preserve_recipient_order)
{
    game_world_delta_interest_set interests;
    ft_vector<uint64_t> live_clients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live_clients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(31U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(31U, 3U, 1, -4, 9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(51U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(61U, 4U, 1, -5, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(71U, 4U, -1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(21U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(51U, 4U, 1, -4, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(21U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, interests.acknowledge_snapshot(51U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(51U, 4U, 1, -4, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(31U, 4U, 1, -4, 8U));
    FT_ASSERT_EQ(FT_TRUE, interests.is_snapshot_pending(31U, 3U, 1, -4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(4U, 1, -4, live_clients));
    FT_ASSERT_EQ(3U, live_clients.size());
    FT_ASSERT_EQ(31U, live_clients[0U]);
    FT_ASSERT_EQ(51U, live_clients[1U]);
    FT_ASSERT_EQ(21U, live_clients[2U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.unsubscribe(31U, 4U, 1, -4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(31U, 4U, 1, -4, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(31U, 4U, 1, -4, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(4U, 1, -4, live_clients));
    FT_ASSERT_EQ(3U, live_clients.size());
    FT_ASSERT_EQ(51U, live_clients[0U]);
    FT_ASSERT_EQ(21U, live_clients[1U]);
    FT_ASSERT_EQ(31U, live_clients[2U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(4U, 1, -3, live_clients));
    FT_ASSERT_EQ(0U, live_clients.size());
    return (1);
}

FT_TEST(test_game_world_delta_interest_collects_batches_in_subscription_order)
{
    game_world_delta_interest_set interests;
    ft_vector<uint64_t> live_clients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, live_clients.initialize());
    uint64_t id = 110U;
    while (id != 0U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(id, 4U, 1, -4, 8U));
        if (id % 5U != 0U)
            FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(id, 4U, 1, -4, 8U));
        --id;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.subscribe(111U, 4U, 1, -3, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.acknowledge_snapshot(111U, 4U, 1, -3, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interests.collect_live_clients(4U, 1, -4, live_clients));
    FT_ASSERT_EQ(88U, live_clients.size());
    id = 110U;
    ft_size_t list_index = 0U;
    while (id != 0U)
    {
        if (id % 5U != 0U)
        {
            FT_ASSERT_EQ(id, live_clients[list_index]);
            ++list_index;
        }
        --id;
    }
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
    FT_ASSERT_EQ(1U, delta.server_tick);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.apply_request(request, delta));
    FT_ASSERT_EQ(1U, channel.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.collect_live_clients(live_clients));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), live_clients.size());
    FT_ASSERT_EQ(10U, live_clients[0U]);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, channel.acknowledge_revision(10U,
        2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.acknowledge_revision(10U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.recover_from(0U, recovered));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1U), recovered.size());
    FT_ASSERT_EQ(1U, recovered[0U].revision);
    FT_ASSERT_EQ(1U, recovered[0U].server_tick);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_chunk.apply_authoritative_block_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 2, 3,
        &block_id));
    FT_ASSERT_EQ(12U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(1, 2, 4, 14U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, channel.apply_request(request, delta));
    FT_ASSERT_EQ(2U, channel.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.recover_from(0U, recovered));
    FT_ASSERT_EQ(1U, recovered.size());
    FT_ASSERT_EQ(1U, recovered[0U].revision);
    return (1);
}

FT_TEST(test_game_world_delta_snapshot_checksum_rejects_corruption)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer snapshot;
    ft_byte_buffer corrupted_snapshot;
    uint32_t block_id;
    ft_size_t initial_read_position;

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
    initial_read_position = corrupted_snapshot.read_position();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        game_world_delta_snapshot_deserialize(destination_chunk,
            corrupted_snapshot));
    FT_ASSERT_EQ(initial_read_position, corrupted_snapshot.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(5, 6, 7,
        &block_id));
    FT_ASSERT_EQ(42U, block_id);
    return (1);
}

FT_TEST(test_game_world_delta_snapshot_append_preserves_prefix_and_cursor)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer buffer;
    uint32_t block_id;
    uint32_t prefix;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(1, 255, 15, 42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(8U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.set_read_position(2U));
    FT_ASSERT_EQ(FT_ERR_FULL, game_world_delta_snapshot_serialize(source_chunk, buffer));
    FT_ASSERT_EQ(2U, buffer.read_position());
    FT_ASSERT_EQ(4U, buffer.size());
    FT_ASSERT_EQ(FT_TRUE, buffer.is_fixed_capacity());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.reset_read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u32_le(&prefix));
    FT_ASSERT_EQ(0x12345678U, prefix);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.set_read_position(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_serialize(source_chunk, buffer));
    FT_ASSERT_EQ(2U, buffer.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.reset_read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read_u32_le(&prefix));
    FT_ASSERT_EQ(0x12345678U, prefix);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_deserialize(destination_chunk, buffer));
    FT_ASSERT_EQ(buffer.size(), buffer.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 255, 15, &block_id));
    FT_ASSERT_EQ(42U, block_id);

    ft_byte_buffer fixed_frame;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fixed_frame.initialize(buffer.size(), FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fixed_frame.append_u32_le(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_serialize(source_chunk, fixed_frame));
    FT_ASSERT_EQ(buffer.size(), fixed_frame.size());
    FT_ASSERT_EQ(0, ft_memcmp(buffer.data(), fixed_frame.data(), buffer.size()));
    return (1);
}

FT_TEST(test_game_world_delta_snapshot_decode_stays_inside_payload)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer valid_frame;
    ft_byte_buffer frames;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(1, 255, 15, 42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.write_block(1, 255, 15, 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_frame.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_serialize(source_chunk, valid_frame));

    uint32_t payload_size;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_frame.read_u32_le(&payload_size));
    FT_ASSERT(payload_size > 1U);
    const uint8_t *payload;
    const uint32_t truncated_size = payload_size - 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_frame.view(sizeof(uint32_t), truncated_size, &payload));
    uint32_t checksum = 2166136261U;
    uint32_t offset = 0U;
    while (offset < truncated_size)
    {
        checksum = (checksum ^ payload[offset]) * 16777619U;
        ++offset;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.append_u8(0xABU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.append_u32_le(truncated_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.append(payload, truncated_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.append_u32_le(checksum));
    const ft_size_t valid_frame_start = frames.size();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.append(valid_frame.data(), valid_frame.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.set_read_position(1U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, game_world_delta_snapshot_deserialize(destination_chunk, frames));
    FT_ASSERT_EQ(1U, frames.read_position());
    uint32_t block_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 255, 15, &block_id));
    FT_ASSERT_EQ(7U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frames.set_read_position(valid_frame_start));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_world_delta_snapshot_deserialize(destination_chunk, frames));
    FT_ASSERT_EQ(frames.size(), frames.read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 255, 15, &block_id));
    FT_ASSERT_EQ(42U, block_id);
    return (1);
}

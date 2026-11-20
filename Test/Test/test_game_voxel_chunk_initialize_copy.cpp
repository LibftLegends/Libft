#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/Game/game_world_delta.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_voxel_chunk_initialize_copy_preserves_replay_and_overrides)
{
    game_voxel_chunk source;
    game_voxel_chunk copy;
    game_block_change_request request = {};
    game_block_delta accepted = {};
    game_block_delta replay = {};
    game_voxel_generation_metadata metadata = {};
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize());
    metadata.valid = FT_TRUE;
    metadata.seed_value = 42U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_generation_metadata(metadata));
    source.set_biome_id(4U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.write_generated_block(3, 70, 2, 7U));
    request.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    request.session_id = 4U;
    request.request_id = 18U;
    request.world_id = 11U;
    request.expected_revision = source.get_revision();
    request.expected_block_id = 7U;
    request.requested_block_id = 9U;
    request.local_x = 3U;
    request.local_y = 70U;
    request.local_z = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source.apply_authoritative_block_change(request, &accepted));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_TRUE, copy.is_dirty());
    FT_ASSERT_EQ(4U, copy.get_biome_id());
    FT_ASSERT_EQ(42U, copy.get_generation_metadata().seed_value);
    FT_ASSERT_EQ(1U, copy.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.apply_authoritative_block_change(
        request, &replay));
    FT_ASSERT_EQ(accepted.revision, replay.revision);
    FT_ASSERT_EQ(accepted.revision, copy.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_generated_block(3, 70, 2, &block_id));
    FT_ASSERT_EQ(7U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.write_block(3, 70, 2, 7U));
    FT_ASSERT_EQ(0U, copy.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.read_block(3, 70, 2, &block_id));
    FT_ASSERT_EQ(9U, block_id);
    FT_ASSERT_EQ(1U, source.get_player_override_count());
    return (1);
}

FT_TEST(test_game_voxel_chunk_initialize_copy_can_retry_after_allocation_failure)
{
    game_voxel_chunk source;
    game_voxel_chunk destination;
    int32_t result;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.write_block(2, 33, 5, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    cma_set_alloc_limit(GAME_VOXEL_SECTION_BLOCKS - 1);
    result = destination.initialize(source);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.read_block(2, 33, 5, &block_id));
    FT_ASSERT_EQ(4U, block_id);
    return (1);
}

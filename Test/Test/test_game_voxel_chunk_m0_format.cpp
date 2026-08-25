#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_voxel_chunk_biome_id_defaults_to_zero_and_is_settable)
{
    game_voxel_chunk value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0U, value.get_biome_id());
    value.set_biome_id(4U);
    FT_ASSERT_EQ(4U, value.get_biome_id());
    return (1);
}

FT_TEST(test_game_voxel_chunk_records_and_lists_dirty_edits)
{
    game_voxel_chunk value;
    game_block_edit_op edit;
    game_block_edit_op fetched;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0U, value.get_dirty_edit_count());
    edit.world_x = 5;
    edit.world_y = 64;
    edit.world_z = -3;
    edit.block_type = 7U;
    edit.tick = 42U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.record_dirty_edit(edit));
    FT_ASSERT_EQ(1U, value.get_dirty_edit_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_dirty_edit(0U, &fetched));
    FT_ASSERT_EQ(edit.world_x, fetched.world_x);
    FT_ASSERT_EQ(edit.world_y, fetched.world_y);
    FT_ASSERT_EQ(edit.world_z, fetched.world_z);
    FT_ASSERT_EQ(edit.block_type, fetched.block_type);
    FT_ASSERT_EQ(edit.tick, fetched.tick);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_dirty_edit(1U, &fetched));
    value.clear_dirty_edits();
    FT_ASSERT_EQ(0U, value.get_dirty_edit_count());
    return (1);
}

FT_TEST(test_game_voxel_chunk_dirty_edit_overflow_drops_oldest)
{
    game_voxel_chunk value;
    game_block_edit_op edit;
    game_block_edit_op fetched;
    uint32_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    index = 0U;
    while (index < GAME_VOXEL_CHUNK_MAX_DIRTY_EDITS + 4U)
    {
        edit.world_x = static_cast<int32_t>(index);
        edit.world_y = 0;
        edit.world_z = 0;
        edit.block_type = 1U;
        edit.tick = index;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, value.record_dirty_edit(edit));
        index += 1U;
    }
    FT_ASSERT_EQ(GAME_VOXEL_CHUNK_MAX_DIRTY_EDITS, value.get_dirty_edit_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_dirty_edit(0U, &fetched));
    FT_ASSERT_EQ(4, fetched.world_x);
    return (1);
}

FT_TEST(test_game_voxel_chunk_serialization_round_trips_biome_and_dirty_edits)
{
    game_voxel_chunk value;
    game_voxel_chunk restored_value;
    game_block_edit_op edit;
    game_block_edit_op fetched;
    ft_byte_buffer buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    value.set_biome_id(2U);
    edit.world_x = 100;
    edit.world_y = 70;
    edit.world_z = -200;
    edit.block_type = 10U;
    edit.tick = UINT64_C(9999999999);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.record_dirty_edit(edit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.deserialize(buffer));
    FT_ASSERT_EQ(2U, restored_value.get_biome_id());
    FT_ASSERT_EQ(1U, restored_value.get_dirty_edit_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.get_dirty_edit(0U, &fetched));
    FT_ASSERT_EQ(edit.world_x, fetched.world_x);
    FT_ASSERT_EQ(edit.world_y, fetched.world_y);
    FT_ASSERT_EQ(edit.world_z, fetched.world_z);
    FT_ASSERT_EQ(edit.block_type, fetched.block_type);
    FT_ASSERT_EQ(edit.tick, fetched.tick);
    return (1);
}

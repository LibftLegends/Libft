#include "../../Modules/Game/game_block_edit_op.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_block_edit_op_round_trips_through_buffer)
{
    ft_byte_buffer buffer;
    game_block_edit_op source;
    game_block_edit_op restored;

    source.world_x = -12345;
    source.world_y = 90;
    source.world_z = 67890;
    source.block_type = 42U;
    source.tick = UINT64_C(1234567890123456789);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_edit_op_serialize(source, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.reset_read_position());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_block_edit_op_deserialize(restored, buffer));
    FT_ASSERT_EQ(source.world_x, restored.world_x);
    FT_ASSERT_EQ(source.world_y, restored.world_y);
    FT_ASSERT_EQ(source.world_z, restored.world_z);
    FT_ASSERT_EQ(source.block_type, restored.block_type);
    FT_ASSERT_EQ(source.tick, restored.tick);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

FT_TEST(test_game_block_edit_op_deserialize_reports_truncated_buffer)
{
    ft_byte_buffer buffer;
    game_block_edit_op restored;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.reset_read_position());
    FT_ASSERT_NEQ(FT_ERR_SUCCESS,
        game_block_edit_op_deserialize(restored, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    return (1);
}

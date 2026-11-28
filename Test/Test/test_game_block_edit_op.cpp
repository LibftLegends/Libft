#include "../../Modules/Game/game_block_edit_op.hpp"
#include "../../Modules/Basic/basic.hpp"
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
    const uint8_t expected[] = {0xc7, 0xcf, 0xff, 0xff, 90, 0, 0, 0,
        0x32, 0x09, 0x01, 0, 42, 0, 0, 0, 0x15, 0x81, 0xe9, 0x7d, 0xf4, 0x10, 0x22, 0x11};
    FT_ASSERT_EQ(sizeof(expected), buffer.size());
    uint8_t encoded[sizeof(expected)];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.read(encoded, sizeof(encoded)));
    FT_ASSERT_EQ(0, ft_memcmp(encoded, expected, sizeof(expected)));
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

FT_TEST(test_game_block_edit_op_deserialize_is_transactional)
{
    ft_byte_buffer encoded_buffer;
    ft_byte_buffer truncated_buffer;
    game_block_edit_op source;
    game_block_edit_op restored;
    ft_size_t prefix_length;
    int32_t error_code;

    source.world_x = -123;
    source.world_y = 64;
    source.world_z = 456;
    source.block_type = 789U;
    source.tick = UINT64_C(987654321);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_block_edit_op_serialize(source,
        encoded_buffer));
    prefix_length = 0U;
    while (prefix_length < encoded_buffer.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_buffer.slice(0U, prefix_length,
            truncated_buffer));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.reset_read_position());
        restored.world_x = 111;
        restored.world_y = 222;
        restored.world_z = 333;
        restored.block_type = 444U;
        restored.tick = UINT64_C(555);
        error_code = game_block_edit_op_deserialize(restored,
            truncated_buffer);
        FT_ASSERT_NEQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT_EQ(0U, truncated_buffer.read_position());
        FT_ASSERT_EQ(111, restored.world_x);
        FT_ASSERT_EQ(222, restored.world_y);
        FT_ASSERT_EQ(333, restored.world_z);
        FT_ASSERT_EQ(444U, restored.block_type);
        FT_ASSERT_EQ(UINT64_C(555), restored.tick);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.destroy());
        prefix_length += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded_buffer.destroy());
    return (1);
}

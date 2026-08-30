#include "game_block_edit_op.hpp"

int32_t game_block_edit_op_serialize(const game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept
{
    int32_t error_code;

    error_code = buffer.append_u32_le(static_cast<uint32_t>(edit.world_x));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(static_cast<uint32_t>(edit.world_y));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(static_cast<uint32_t>(edit.world_z));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(edit.block_type);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (buffer.append_u64_le(edit.tick));
}

int32_t game_block_edit_op_deserialize(game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept
{
    uint32_t raw_x;
    uint32_t raw_y;
    uint32_t raw_z;
    uint32_t block_type;
    uint64_t tick;
    int32_t error_code;

    error_code = buffer.read_u32_le(&raw_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&raw_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&raw_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&block_type);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&tick);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    edit.world_x = static_cast<int32_t>(raw_x);
    edit.world_y = static_cast<int32_t>(raw_y);
    edit.world_z = static_cast<int32_t>(raw_z);
    edit.block_type = block_type;
    edit.tick = tick;
    return (FT_ERR_SUCCESS);
}

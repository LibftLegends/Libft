#include "game_block_edit_op.hpp"
#include "../Buffer/buffer_endian.hpp"

static constexpr ft_size_t GAME_BLOCK_EDIT_RECORD_SIZE = 24U;

int32_t game_block_edit_op_serialize(const game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t bytes[GAME_BLOCK_EDIT_RECORD_SIZE];
    buffer_store_le(bytes, static_cast<uint32_t>(edit.world_x));
    buffer_store_le(bytes + 4U, static_cast<uint32_t>(edit.world_y));
    buffer_store_le(bytes + 8U, static_cast<uint32_t>(edit.world_z));
    buffer_store_le(bytes + 12U, edit.block_type);
    buffer_store_le(bytes + 16U, edit.tick);
    return (buffer.append(bytes, sizeof(bytes)));
}

int32_t game_block_edit_op_deserialize(game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t bytes[GAME_BLOCK_EDIT_RECORD_SIZE];
    const int32_t status = buffer.read(bytes, sizeof(bytes));
    if (status != FT_ERR_SUCCESS)
        return (status);
    edit.world_x = static_cast<int32_t>(buffer_load_le<uint32_t>(bytes));
    edit.world_y = static_cast<int32_t>(buffer_load_le<uint32_t>(bytes + 4U));
    edit.world_z = static_cast<int32_t>(buffer_load_le<uint32_t>(bytes + 8U));
    edit.block_type = buffer_load_le<uint32_t>(bytes + 12U);
    edit.tick = buffer_load_le<uint64_t>(bytes + 16U);
    return (FT_ERR_SUCCESS);
}

#ifndef GAME_BLOCK_EDIT_OP_HPP
# define GAME_BLOCK_EDIT_OP_HPP

#include "../Buffer/byte_buffer.hpp"
#include <stdint.h>

/*
 * M0 shared contract: the block-edit op format `{x, y, z, block_type, tick}`.
 * Reused verbatim by the chunk dirty-edit list (persistence/network wire
 * format), by the network edit-broadcast message, and by undo/redo in
 * src/edits/. Plain POD by design: it owns no resources, so it does not
 * follow the heavier Libft _initialised_state lifecycle used by classes
 * that manage memory (matches game_voxel_generation_metadata's precedent).
 */
struct game_block_edit_op
{
    int32_t world_x;
    int32_t world_y;
    int32_t world_z;
    uint32_t block_type;
    uint64_t tick;
};

int32_t game_block_edit_op_serialize(const game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept;
int32_t game_block_edit_op_deserialize(game_block_edit_op &edit,
    ft_byte_buffer &buffer) noexcept;

#endif

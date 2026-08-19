#ifndef VOXEL_BLOCK_REGISTRY_HPP
# define VOXEL_BLOCK_REGISTRY_HPP

# include "voxel.hpp"

const terrain_block_metadata *terrain_runtime_find_block_metadata(
    uint32_t block_id) noexcept;
ft_bool terrain_runtime_block_is_known(uint32_t block_id) noexcept;

#ifdef LIBFT_TEST_BUILD
void terrain_runtime_reset_for_tests(void) noexcept;
#endif

#endif

#ifndef VOXEL_SHADOW_HPP
# define VOXEL_SHADOW_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include <stdint.h>
# include "../Errno/errno.hpp"

typedef ft_bool (*voxel_shadow_solid_lookup_fn)(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z) noexcept;

int32_t voxel_shadow_find_receiver(int32_t world_x, int32_t start_y,
    int32_t world_z, int32_t max_distance,
    voxel_shadow_solid_lookup_fn lookup, void *user_data,
    int32_t *receiver_y) noexcept;

double voxel_shadow_height_fade(double entity_bottom_y, int32_t receiver_y,
    double max_height) noexcept;

#endif

#endif


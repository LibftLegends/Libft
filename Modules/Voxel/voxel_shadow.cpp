#include "voxel_shadow.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include <algorithm>

int32_t voxel_shadow_find_receiver(int32_t world_x, int32_t start_y,
    int32_t world_z, int32_t max_distance,
    voxel_shadow_solid_lookup_fn lookup, void *user_data,
    int32_t *receiver_y) noexcept
{
    if (lookup == nullptr || receiver_y == nullptr || max_distance < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    const int64_t requested_end = static_cast<int64_t>(start_y) - max_distance;
    const int32_t end_y = static_cast<int32_t>(std::max<int64_t>(INT32_MIN, requested_end));
    int32_t candidate_y = start_y;
    while (FT_TRUE)
    {
        if (lookup(user_data, world_x, candidate_y, world_z) == FT_TRUE)
        {
            *receiver_y = candidate_y;
            return (FT_ERR_SUCCESS);
        }
        if (candidate_y == end_y)
            break ;
        candidate_y -= 1;
    }
    return (FT_ERR_NOT_FOUND);
}

double voxel_shadow_height_fade(double entity_bottom_y, int32_t receiver_y,
    double max_height) noexcept
{
    if (max_height <= 0.0)
        return (0.0);
    const double height = entity_bottom_y - (static_cast<double>(receiver_y) + 1.0);
    return (std::max(0.0, std::min(1.0, 1.0 - height / max_height)));
}

#endif


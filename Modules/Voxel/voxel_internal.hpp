#ifndef VOXEL_INTERNAL_HPP
#define VOXEL_INTERNAL_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_types.hpp"
#include <stdint.h>

// The divisor must be positive.
constexpr int32_t voxel_floor_div(int32_t value, int32_t divisor) noexcept
{
    int32_t quotient = value / divisor;
    if (value % divisor < 0)
        quotient -= 1;
    return (quotient);
}

constexpr uint64_t voxel_mix_u64(uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31;
    return (value);
}

constexpr double voxel_lerp(double left_value, double right_value,
    double factor) noexcept
{
    return (left_value + ((right_value - left_value) * factor));
}

constexpr double voxel_smooth_factor(double factor) noexcept
{
    return (factor * factor * (3.0 - (2.0 * factor)));
}

uint64_t voxel_seed_value(const char *seed_string) noexcept;
uint64_t voxel_feature_seed(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, uint64_t salt) noexcept;
double voxel_signed_unit_noise(uint64_t seed_value, int32_t grid_x,
    int32_t grid_z) noexcept;
double voxel_value_noise(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, int32_t scale) noexcept;
voxel_biome voxel_pick_biome(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, int32_t biome_zone_width) noexcept;

#endif

#endif

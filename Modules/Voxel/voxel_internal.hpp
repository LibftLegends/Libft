#ifndef VOXEL_INTERNAL_HPP
#define VOXEL_INTERNAL_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel.hpp"
#include <stdint.h>

int32_t terrain_floor_div(int32_t value, int32_t divisor) noexcept;
uint64_t terrain_mix_u64(uint64_t value) noexcept;
double terrain_lerp(double left_value, double right_value,
    double factor) noexcept;
double terrain_smooth_factor(double factor) noexcept;
uint64_t terrain_seed_value(const char *seed_string) noexcept;
uint64_t terrain_feature_seed(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, uint64_t salt) noexcept;
double terrain_signed_unit_noise(uint64_t seed_value, int32_t grid_x,
    int32_t grid_z) noexcept;
double terrain_value_noise(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, int32_t scale) noexcept;
terrain_biome terrain_pick_biome(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, int32_t biome_zone_width) noexcept;

#endif

#endif

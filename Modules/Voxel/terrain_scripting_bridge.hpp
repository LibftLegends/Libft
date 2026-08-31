#ifndef TERRAIN_SCRIPTING_BRIDGE_HPP
# define TERRAIN_SCRIPTING_BRIDGE_HPP

#include "terrain_config.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Game/game_scripting_bridge.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

int32_t terrain_script_register_api(game_script_bridge &bridge) noexcept;
int32_t terrain_script_execute(game_script_bridge &bridge,
    const ft_string &script, game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, terrain_generation_config &config,
    const char *asset_root) noexcept;

#endif

#endif

Voxel_TARGET := Voxel.a
Voxel_DEBUG_TARGET := Voxel_debug.a

Voxel_SOURCES := voxel_data.cpp voxel_generation.cpp voxel_mesh.cpp voxel_mesh_frustum.cpp voxel_save.cpp voxel_json.cpp voxel_world_coordinate.cpp voxel_lighting.cpp voxel_shadow.cpp voxel_terrain_scripting_bridge.cpp voxel_block_registry.cpp

Voxel_CPP_FLAGS := -DGAME_USE_VOXEL_REGION_BACKEND=1

Voxel_HEADERS := voxel.hpp terrain_types.hpp terrain_config.hpp terrain_generation.hpp terrain_api.hpp voxel_mesh.hpp voxel_lighting.hpp voxel_shadow.hpp voxel_internal.hpp terrain_scripting_bridge.hpp voxel_block_registry.hpp

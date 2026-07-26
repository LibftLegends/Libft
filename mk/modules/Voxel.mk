TARGET := Voxel.a
DEBUG_TARGET := Voxel_debug.a

SRCS := voxel_data.cpp voxel_generation.cpp voxel_mesh.cpp voxel_mesh_frustum.cpp voxel_save.cpp voxel_world_coordinate.cpp

HEADERS := voxel.hpp voxel_mesh.hpp voxel_internal.hpp

include $(dir $(lastword $(MAKEFILE_LIST)))common/module_defaults.mk

#include "../test_internal.hpp"
#include "../../Modules/Voxel/terrain_scripting_bridge.hpp"
#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#ifdef GAME_USE_VOXEL_REGION_BACKEND

FT_TEST(test_terrain_runtime_block_registration_loads_assets)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    ft_string script;
    terrain_generation_config config;
    game_voxel_chunk chunk;
    int64_t registered_block_id;
    uint32_t block_id;
    ft_size_t asset_size;
    ft_string global_name;
    const uint8_t *asset_data;
    const terrain_block_metadata *metadata;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer.initialize(new game_world()));
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    if (file_read_all("Lua/terrain_register_block.lua", script)
        != FT_ERR_SUCCESS)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(
            "Test/Lua/terrain_register_block.lua", script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_execute(bridge, script,
        chunk, 0, 0, "test", config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, global_name.initialize(
        "registered_block_id"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_lua_global_integer(
        global_name, registered_block_id));
    FT_ASSERT(registered_block_id >= 19);
    block_id = static_cast<uint32_t>(registered_block_id);
    FT_ASSERT_EQ(FT_TRUE, terrain_block_is_known(block_id));
    metadata = &terrain_get_block_metadata(block_id);
    FT_ASSERT_EQ(FT_TRUE, metadata->solid);
    FT_ASSERT_EQ(7U, metadata->hardness);
    FT_ASSERT_EQ(FT_TRUE, metadata->breakable);
    FT_ASSERT_EQ(0, std::strcmp(terrain_get_block_name(block_id),
        "test:script_block"));
    FT_ASSERT_EQ(0, std::strcmp(terrain_get_block_asset_path(block_id,
        TERRAIN_BLOCK_ASSET_FACE_TOP), "Lua/export_values.lua"));
    asset_data = terrain_get_block_asset_data(block_id,
        TERRAIN_BLOCK_ASSET_FACE_TOP, &asset_size);
    FT_ASSERT(asset_data != ft_nullptr);
    FT_ASSERT(asset_size > 0U);
    FT_ASSERT_EQ(static_cast<uint8_t>('b'), asset_data[0]);
    return (1);
}

FT_TEST(test_terrain_runtime_block_registration_rejects_missing_asset)
{
    terrain_block_registration registration;
    uint32_t block_id;
    int32_t error_code;

    registration.name = "test:missing_asset_block";
    registration.metadata.solid = FT_TRUE;
    registration.metadata.transparent = FT_FALSE;
    registration.metadata.liquid = FT_FALSE;
    registration.metadata.replaceable = FT_FALSE;
    registration.metadata.can_host_ore = FT_TRUE;
    registration.metadata.is_ore = FT_FALSE;
    registration.metadata.light_emitting = FT_FALSE;
    registration.metadata.occludes_faces = FT_TRUE;
    registration.metadata.hardness = 1U;
    registration.metadata.breakable = FT_TRUE;
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_TOP] =
        "Lua/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_BOTTOM] =
        "Lua/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_NORTH] =
        "Lua/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_SOUTH] =
        "Lua/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_EAST] =
        "Lua/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_WEST] =
        "Lua/does_not_exist.asset";
    block_id = 0U;
    error_code = terrain_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, error_code);
    FT_ASSERT_EQ(0U, block_id);
    return (1);
}

#endif

#include "../test_internal.hpp"
#include "../../Modules/Voxel/terrain_scripting_bridge.hpp"
#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/Voxel/voxel_block_registry.hpp"
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
    int32_t find_error;
    uint32_t block_id;
    ft_size_t asset_size;
    const uint8_t *asset_data;
    const terrain_block_metadata *metadata;

    terrain_runtime_reset_for_tests();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer.initialize(new game_world()));
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    if (file_read_all("Scripting/terrain_register_block.script", script)
        != FT_ERR_SUCCESS)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(
            "Test/Scripting/terrain_register_block.script", script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_execute(bridge, script,
        chunk, 0, 0, "test", config));
    find_error = terrain_find_block_id_by_name("test:script_block",
        &block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, find_error);
    FT_ASSERT(block_id >= 19U);
    FT_ASSERT_EQ(FT_TRUE, terrain_block_is_known(block_id));
    metadata = &terrain_get_block_metadata(block_id);
    FT_ASSERT_EQ(FT_TRUE, metadata->solid);
    FT_ASSERT_EQ(7U, metadata->hardness);
    FT_ASSERT_EQ(FT_TRUE, metadata->breakable);
    FT_ASSERT_EQ(0, std::strcmp(terrain_get_block_name(block_id),
        "test:script_block"));
    FT_ASSERT_EQ(0, std::strcmp(terrain_get_block_asset_path(block_id,
        TERRAIN_BLOCK_ASSET_FACE_TOP), "Scripting/export_values.asset"));
    asset_data = terrain_get_block_asset_data(block_id,
        TERRAIN_BLOCK_ASSET_FACE_TOP, &asset_size);
    FT_ASSERT(asset_data != ft_nullptr);
    FT_ASSERT(asset_size > 0U);
    FT_ASSERT_EQ(static_cast<uint8_t>('b'), asset_data[0]);
    terrain_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_terrain_runtime_block_registration_rejects_missing_asset)
{
    terrain_block_registration registration;
    uint32_t block_id;
    int32_t error_code;

    terrain_runtime_reset_for_tests();
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
        "Scripting/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_BOTTOM] =
        "Scripting/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_NORTH] =
        "Scripting/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_SOUTH] =
        "Scripting/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_EAST] =
        "Scripting/does_not_exist.asset";
    registration.asset_paths[TERRAIN_BLOCK_ASSET_FACE_WEST] =
        "Scripting/does_not_exist.asset";
    block_id = 0U;
    error_code = terrain_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, error_code);
    FT_ASSERT_EQ(0U, block_id);
    terrain_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_terrain_runtime_block_handle_survives_unregister)
{
    terrain_block_registration registration;
    terrain_runtime_block_handle block_handle;
    uint32_t block_id;
    ft_size_t asset_size;
    const uint8_t *asset_data;
    uint32_t face_index;

    terrain_runtime_reset_for_tests();
    registration.name = "test:shared_handle_block";
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
    face_index = 0U;
    while (face_index < TERRAIN_BLOCK_ASSET_FACE_COUNT)
    {
        registration.asset_paths[face_index] =
            "Scripting/export_values.asset";
        face_index += 1U;
    }
    block_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_register_block(registration,
        &block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_acquire_block(block_id,
        block_handle));
    FT_ASSERT_EQ(FT_TRUE, block_handle.is_valid());
    FT_ASSERT_EQ(block_id, block_handle.get_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_unregister_block(block_id));
    FT_ASSERT_EQ(FT_FALSE, terrain_block_is_known(block_id));
    FT_ASSERT_EQ(FT_TRUE, block_handle.is_valid());
    asset_data = block_handle.get_asset_data(
        TERRAIN_BLOCK_ASSET_FACE_TOP, &asset_size);
    FT_ASSERT(asset_data != ft_nullptr);
    FT_ASSERT(asset_size > 0U);
    FT_ASSERT_EQ(static_cast<uint8_t>('b'), asset_data[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, block_handle.destroy());
    FT_ASSERT_EQ(FT_FALSE, block_handle.is_valid());
    terrain_runtime_reset_for_tests();
    return (1);
}

#endif

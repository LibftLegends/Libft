#include "../test_internal.hpp"
#include "../../Modules/Voxel/terrain_scripting_bridge.hpp"
#include "../../Modules/Voxel/voxel_block_registry.hpp"
#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

FT_TEST(test_terrain_script_execute_configures_and_generates_chunk)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    terrain_generation_config config;
    game_voxel_chunk chunk;
    ft_string script;
    uint32_t block_id;

    terrain_runtime_reset_for_tests();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer.initialize(new game_world()));
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    if (file_read_all("Lua/terrain_generation.lua", script)
        != FT_ERR_SUCCESS)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(
            "Test/Lua/terrain_generation.lua", script));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_execute(bridge, script, chunk,
        32, 64, "scripted-world", config));
    FT_ASSERT_EQ(40, config.sea_level);
    FT_ASSERT_EQ(40, config.large_noise_scale);
    FT_ASSERT_EQ(10, config.detail_noise_scale);
    FT_ASSERT_EQ(65, config.detail_noise_percent);
    FT_ASSERT_EQ(40, config.biomes[0].profile.surface_height);
    FT_ASSERT_EQ(10U, config.biomes[0].surface_block_id);
    FT_ASSERT_EQ(FT_TRUE, config.enable_biome_transitions);
    FT_ASSERT_EQ(23, config.biome_transition_noise_scale);
    FT_ASSERT_EQ(55U, config.biome_transition_noise_strength);
    FT_ASSERT_EQ(FT_TRUE, chunk.has_generation_metadata());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 255, 0, &block_id));
    FT_ASSERT_EQ(13U, block_id);
    terrain_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_terrain_script_execute_rejects_invalid_arguments)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    terrain_generation_config config;
    game_voxel_chunk chunk;
    ft_string script;

    terrain_runtime_reset_for_tests();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer.initialize(new game_world()));
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_default_generation_config(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize(
        "terrain_set_sea_level('invalid')\n"));

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, terrain_script_execute(bridge,
        script, chunk, 0, 0, "scripted-world", config));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, terrain_script_execute(bridge,
        script, chunk, 0, 0, ft_nullptr, config));
    terrain_runtime_reset_for_tests();
    return (1);
}

#endif

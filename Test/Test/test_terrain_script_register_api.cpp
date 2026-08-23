#include "../test_internal.hpp"
#include "../../Modules/Voxel/terrain_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

FT_TEST(test_terrain_script_register_api_is_idempotent)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer.initialize(new game_world()));
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(11U, bridge.get_callback_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_script_register_api(bridge));
    FT_ASSERT_EQ(11U, bridge.get_callback_count());
    return (1);
}

#endif

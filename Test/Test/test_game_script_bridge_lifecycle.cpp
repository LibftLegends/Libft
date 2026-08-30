#include "../test_internal.hpp"

#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_script_bridge_thread_safe_lifecycle)
{
    game_script_bridge bridge;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(0, ft_strcmp(bridge.get_language().c_str(), "custom"));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), bridge.get_callback_count());
    bridge.set_language("lua");
    bridge.set_max_operations(7);
    FT_ASSERT_EQ(7, bridge.get_max_operations());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

#include "../test_internal.hpp"

#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_map3d_thread_safe_lifecycle)
{
    game_map3d map_3d;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_3d.initialize(4, 3, 2, 9));
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), map_3d.get_width());
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), map_3d.get_height());
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), map_3d.get_depth());
    FT_ASSERT_EQ(9, map_3d.get(0, 0, 0));
    map_3d.set(1, 1, 1, 4);
    FT_ASSERT_EQ(4, map_3d.get(1, 1, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_3d.enable_thread_safety());
    FT_ASSERT(map_3d.is_thread_safe() == FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_3d.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_3d.destroy());
    return (1);
}

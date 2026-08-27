#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void game_achievement_move(game_achievement &value)
{
    (void)value.move(value);
    return ;
}
static void game_achievement_initialize_copy(game_achievement &value)
{
    if (value.initialize() != FT_ERR_SUCCESS)
        return ;
    (void)value.initialize(value);
    (void)value.destroy();
    return ;
}
static void game_achievement_destroy(game_achievement &value)
{
    (void)value.destroy();
    return ;
}
static void game_achievement_enable(game_achievement &value)
{
    (void)value.enable_thread_safety();
    return ;
}
static void game_achievement_disable(game_achievement &value)
{
    (void)value.disable_thread_safety();
    return ;
}
static void game_achievement_is_thread_safe(game_achievement &value)
{
    (void)value.is_thread_safe();
    return ;
}
static void game_achievement_is_goal_complete(game_achievement &value)
{
    (void)value.is_goal_complete(1);
    return ;
}
static void game_achievement_get_error_str(game_achievement &value)
{
    (void)value.get_error_str();
    return ;
}
static void game_achievement_set_id_again(game_achievement &value)
{
    value.set_id(8);
    return ;
}
static void game_achievement_get_id_again(game_achievement &value)
{
    (void)value.get_id();
    return ;
}

FT_TEST(test_game_achievement_move_self_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_move));
    return (1);
}
FT_TEST(test_game_achievement_initialize_copy_self_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_initialize_copy));
    return (1);
}
FT_TEST(test_game_achievement_destroy_is_non_aborting)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_destroy));
    return (1);
}
FT_TEST(test_game_achievement_enable_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_enable));
    return (1);
}
FT_TEST(test_game_achievement_disable_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_disable));
    return (1);
}
FT_TEST(test_game_achievement_is_thread_safe_aborts)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_is_thread_safe));
    return (1);
}
FT_TEST(test_game_achievement_is_goal_complete_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_is_goal_complete));
    return (1);
}
FT_TEST(test_game_achievement_get_error_str_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_get_error_str));
    return (1);
}
FT_TEST(test_game_achievement_second_set_id_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_set_id_again));
    return (1);
}
FT_TEST(test_game_achievement_second_get_id_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_achievement>(
                        game_achievement_get_id_again));
    return (1);
}

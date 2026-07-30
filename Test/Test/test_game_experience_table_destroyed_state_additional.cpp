#include "../../Modules/Game/game_experience_table.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_destroyed_state_helpers.hpp"

static void experience_destroyed_get_count(game_experience_table &value)
{
    (void)value.get_count();
    return ;
}

static void experience_destroyed_get_level(game_experience_table &value)
{
    (void)value.get_level(0);
    return ;
}

static void experience_destroyed_get_value(game_experience_table &value)
{
    (void)value.get_value(0);
    return ;
}

static void experience_destroyed_set_value(game_experience_table &value)
{
    value.set_value(0, 1);
    return ;
}

static void experience_destroyed_set_levels(game_experience_table &value)
{
    (void)value.set_levels(ft_nullptr, 0);
    return ;
}

static void experience_destroyed_generate_total(game_experience_table &value)
{
    (void)value.generate_levels_total(1, 1, 2.0);
    return ;
}

static void experience_destroyed_generate_scaled(game_experience_table &value)
{
    (void)value.generate_levels_scaled(1, 1, 2.0);
    return ;
}

static void experience_destroyed_resize(game_experience_table &value)
{
    (void)value.resize(1);
    return ;
}

static void
experience_destroyed_enable_thread_safety(game_experience_table &value)
{
    (void)value.enable_thread_safety();
    return ;
}

static void experience_destroyed_check_for_error(game_experience_table &value)
{
    (void)value.check_for_error();
    return ;
}

FT_TEST(test_game_experience_table_destroyed_get_count_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_get_count));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_get_level_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_get_level));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_get_value_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_get_value));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_set_value_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_set_value));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_set_levels_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_set_levels));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_generate_total_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_generate_total));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_generate_scaled_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_generate_scaled));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_resize_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_resize));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_enable_thread_safety_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_enable_thread_safety));
    return (1);
}

FT_TEST(test_game_experience_table_destroyed_check_for_error_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_experience_table>(
                        experience_destroyed_check_for_error));
    return (1);
}

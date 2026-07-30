#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void map3d_initialize_twice(game_map3d &value)
{
    (void)value.initialize();
    (void)value.initialize();
    return ;
}

static void map3d_move_uninitialised(game_map3d &value)
{
    game_map3d source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_map3d_initialize_twice_aborts)
{
    FT_ASSERT_EQ(
        1, expect_game_lifecycle_sigabrt<game_map3d>(map3d_initialize_twice));
    return (1);
}

FT_TEST(test_game_map3d_move_uninitialised_aborts)
{
    FT_ASSERT_EQ(
        1, expect_game_lifecycle_sigabrt<game_map3d>(map3d_move_uninitialised));
    return (1);
}

FT_TEST(test_game_map3d_initialize_dimensions)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(2, 3, 4, 7));
    FT_ASSERT_EQ(2, value.get_width());
    FT_ASSERT_EQ(3, value.get_height());
    FT_ASSERT_EQ(4, value.get_depth());
    FT_ASSERT_EQ(7, value.get(1, 2, 3));
    return (1);
}

FT_TEST(test_game_map3d_set_and_get_cell)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(2, 2, 2, 0));
    value.set(1, 1, 1, 9);
    FT_ASSERT_EQ(9, value.get(1, 1, 1));
    return (1);
}

FT_TEST(test_game_map3d_resize_replaces_dimensions)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 2));
    value.resize(3, 2, 1, 5);
    FT_ASSERT_EQ(3, value.get_width());
    FT_ASSERT_EQ(2, value.get_height());
    FT_ASSERT_EQ(1, value.get_depth());
    FT_ASSERT_EQ(5, value.get(2, 1, 0));
    return (1);
}

FT_TEST(test_game_map3d_out_of_range_get_reports_error)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(0, value.get(1, 0, 0));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    return (1);
}

FT_TEST(test_game_map3d_out_of_range_set_reports_error)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 0));
    value.set(0, 1, 0, 4);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    return (1);
}

FT_TEST(test_game_map3d_toggle_obstacle_sets_obstacle)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_FALSE, value.is_obstacle(0, 0, 0));
    value.toggle_obstacle(0, 0, 0);
    FT_ASSERT_EQ(FT_TRUE, value.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_toggle_obstacle_clears_obstacle)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 1));
    FT_ASSERT_EQ(FT_TRUE, value.is_obstacle(0, 0, 0));
    value.toggle_obstacle(0, 0, 0);
    FT_ASSERT_EQ(FT_FALSE, value.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_thread_safety_enable_disable_cycle)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_map3d_lock_unlock_initialised)
{
    game_map3d value;
    ft_bool lock_acquired = FT_FALSE;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.lock(&lock_acquired));
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    value.unlock(lock_acquired);
    return (1);
}

FT_TEST(test_game_map3d_destroy_is_idempotent)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_game_map3d_error_string_after_success)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_NEQ(ft_nullptr, value.get_error_str());
    return (1);
}

FT_TEST(test_game_map3d_zero_sized_map_reports_range)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(0, 0, 0, 3));
    FT_ASSERT_EQ(0, value.get(0, 0, 0));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    return (1);
}

FT_TEST(test_game_map3d_resize_initial_value)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.resize(2, 2, 2, 11);
    FT_ASSERT_EQ(11, value.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_multiple_cells_are_independent)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(2, 1, 1, 0));
    value.set(0, 0, 0, 3);
    value.set(1, 0, 0, 8);
    FT_ASSERT_EQ(3, value.get(0, 0, 0));
    FT_ASSERT_EQ(8, value.get(1, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_reinitialize_after_destroy)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 6));
    FT_ASSERT_EQ(6, value.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_self_move_is_safe)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(5, value.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_initial_value_is_used_after_resize)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.resize(1, 1, 1, 13);
    FT_ASSERT_EQ(13, value.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_obstacle_nonzero_value_is_true)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 2));
    FT_ASSERT_EQ(FT_TRUE, value.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_obstacle_zero_value_is_false)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_FALSE, value.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_error_after_valid_set)
{
    game_map3d value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1, 1, 1, 0));
    value.set(0, 0, 0, 6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

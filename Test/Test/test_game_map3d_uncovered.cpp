#include "../test_internal.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_game_map3d_default_state)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, map_instance._initialised_state);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), map_instance._width);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), map_instance._height);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), map_instance._depth);
    return (1);
}

FT_TEST(test_game_map3d_initialize_dimensions)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 3, 4, 7));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), map_instance.get_width());
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), map_instance.get_height());
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), map_instance.get_depth());
    return (1);
}

FT_TEST(test_game_map3d_initial_value)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, -7));
    FT_ASSERT_EQ(-7, map_instance.get(0, 0, 0));
    FT_ASSERT_EQ(-7, map_instance.get(1, 1, 1));
    return (1);
}

FT_TEST(test_game_map3d_set_origin)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, 0));
    map_instance.set(0, 0, 0, 11);
    FT_ASSERT_EQ(11, map_instance.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_set_far_edge)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 3, 4, 0));
    map_instance.set(1, 2, 3, 12);
    FT_ASSERT_EQ(12, map_instance.get(1, 2, 3));
    return (1);
}

FT_TEST(test_game_map3d_get_x_out_of_range)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, 0));
    FT_ASSERT_EQ(0, map_instance.get(2, 0, 0));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_get_y_out_of_range)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, 0));
    FT_ASSERT_EQ(0, map_instance.get(0, 2, 0));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_get_z_out_of_range)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, 0));
    FT_ASSERT_EQ(0, map_instance.get(0, 0, 2));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_set_out_of_range)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    map_instance.set(1, 0, 0, 1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_obstacle_default_false)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_FALSE, map_instance.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_obstacle_nonzero_true)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 4));
    FT_ASSERT_EQ(FT_TRUE, map_instance.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_toggle_on)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    map_instance.toggle_obstacle(0, 0, 0);
    FT_ASSERT_EQ(FT_TRUE, map_instance.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_toggle_off)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 1));
    map_instance.toggle_obstacle(0, 0, 0);
    FT_ASSERT_EQ(FT_FALSE, map_instance.is_obstacle(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_resize_dimensions)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 3));
    map_instance.resize(3, 2, 4, 8);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), map_instance.get_width());
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), map_instance.get_height());
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), map_instance.get_depth());
    FT_ASSERT_EQ(8, map_instance.get(2, 1, 3));
    return (1);
}

FT_TEST(test_game_map3d_resize_clears_values)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 2, 2, 0));
    map_instance.set(1, 1, 1, 9);
    map_instance.resize(2, 2, 2, 0);
    FT_ASSERT_EQ(0, map_instance.get(1, 1, 1));
    return (1);
}

FT_TEST(test_game_map3d_thread_safety_enable_disable)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_FALSE, map_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, map_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, map_instance.is_thread_safe());
    return (1);
}

FT_TEST(test_game_map3d_thread_safety_enable_idempotent)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, map_instance.is_thread_safe());
    return (1);
}

FT_TEST(test_game_map3d_lock_without_mutex)
{
    game_map3d map_instance;
    ft_bool acquired;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.lock(&acquired));
    FT_ASSERT_EQ(FT_TRUE, acquired);
    map_instance.unlock(acquired);
    return (1);
}

FT_TEST(test_game_map3d_destroy_state)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, map_instance._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.destroy());
    return (1);
}

FT_TEST(test_game_map3d_reinitialize_after_destroy)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(2, 1, 1, 6));
    FT_ASSERT_EQ(6, map_instance.get(1, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_toggle_out_of_range)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    map_instance.toggle_obstacle(1, 0, 0);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_resize_zero_width)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    map_instance.resize(0, 2, 2, 4);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), map_instance.get_width());
    (void)map_instance.get(0, 0, 0);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_set_negative_value_is_not_obstacle)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    map_instance.set(0, 0, 0, -1);
    FT_ASSERT_EQ(FT_TRUE, map_instance.is_obstacle(0, 0, 0));
    FT_ASSERT_EQ(-1, map_instance.get(0, 0, 0));
    return (1);
}

FT_TEST(test_game_map3d_disable_without_enable)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, map_instance.is_thread_safe());
    return (1);
}

FT_TEST(test_game_map3d_lock_null_output)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.lock(ft_nullptr));
    map_instance.unlock(FT_FALSE);
    return (1);
}

FT_TEST(test_game_map3d_error_clears_after_valid_get)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 0));
    (void)map_instance.get(2, 0, 0);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map_instance.get_error());
    (void)map_instance.get(0, 0, 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.get_error());
    return (1);
}

FT_TEST(test_game_map3d_resize_preserves_new_initial_value)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(1, 1, 1, 2));
    map_instance.resize(2, 2, 2, 13);
    FT_ASSERT_EQ(13, map_instance.get(0, 0, 0));
    FT_ASSERT_EQ(13, map_instance.get(1, 1, 1));
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/GPGR/ft_gpu_window.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_gpu_window *g_gpgr_test_window_pointer = nullptr;

static int32_t gpgr_window_expect_uninitialised_abort(
    void (*operation)(void))
{
    int32_t result;

    g_gpgr_test_window_pointer = ft_gpu_window::create();
    if (g_gpgr_test_window_pointer == nullptr)
        return (0);
    result = test_expect_sigabrt_signal(operation);
    delete g_gpgr_test_window_pointer;
    g_gpgr_test_window_pointer = nullptr;
    return (result);
}

static void gpgr_window_get_width_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->get_width();
    return ;
}

static void gpgr_window_get_height_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->get_height();
    return ;
}

static void gpgr_window_should_close_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->should_close();
    return ;
}

static void gpgr_window_get_mouse_x_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->get_mouse_x();
    return ;
}

static void gpgr_window_get_mouse_y_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->get_mouse_y();
    return ;
}

static void gpgr_window_was_mouse_clicked_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->was_mouse_clicked();
    return ;
}

static void gpgr_window_primary_press_count_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->primary_press_count();
    return ;
}

static void gpgr_window_was_settings_key_pressed_uninitialised(void)
{
    (void)g_gpgr_test_window_pointer->was_settings_key_pressed();
    return ;
}

static void gpgr_window_set_cursor_visible_uninitialised(void)
{
    g_gpgr_test_window_pointer->set_cursor_visible(FT_FALSE);
    return ;
}

static void gpgr_window_move_uninitialised_source(void)
{
    ft_gpu_window *destination_window;

    destination_window = ft_gpu_window::create();
    if (destination_window != nullptr)
    {
        (void)destination_window->move(*g_gpgr_test_window_pointer);
        delete destination_window;
    }
    return ;
}

FT_TEST(test_gpgr_window_factory_create_and_destroy)
{
    ft_gpu_window *window_pointer;

    window_pointer = ft_gpu_window::create();
    FT_ASSERT(window_pointer != nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_pointer->destroy());
    delete window_pointer;
    return (1);
}

FT_TEST(test_gpgr_window_destroy_is_safe_before_initialization)
{
    ft_gpu_window *window_pointer;

    window_pointer = ft_gpu_window::create();
    FT_ASSERT(window_pointer != nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_pointer->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_pointer->destroy());
    delete window_pointer;
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_get_width_aborts)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_get_width_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_get_height_aborts)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_get_height_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_should_close_aborts)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_should_close_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_mouse_getters_abort)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_get_mouse_x_uninitialised));
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_get_mouse_y_uninitialised));
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_was_mouse_clicked_uninitialised));
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_primary_press_count_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_settings_key_getter_aborts)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_was_settings_key_pressed_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_uninitialised_cursor_visibility_aborts)
{
    FT_ASSERT_EQ(1, gpgr_window_expect_uninitialised_abort(
        gpgr_window_set_cursor_visible_uninitialised));
    return (1);
}

FT_TEST(test_gpgr_window_move_self_is_noop)
{
    ft_gpu_window *window_pointer;

    window_pointer = ft_gpu_window::create();
    FT_ASSERT(window_pointer != nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_pointer->move(*window_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_pointer->destroy());
    delete window_pointer;
    return (1);
}

FT_TEST(test_gpgr_window_move_uninitialised_source_aborts)
{
    g_gpgr_test_window_pointer = ft_gpu_window::create();
    FT_ASSERT(g_gpgr_test_window_pointer != nullptr);
    FT_ASSERT_EQ(1, test_expect_sigabrt_signal(
        gpgr_window_move_uninitialised_source));
    delete g_gpgr_test_window_pointer;
    g_gpgr_test_window_pointer = nullptr;
    return (1);
}

FT_TEST(test_gpgr_window_factory_creates_independent_windows)
{
    ft_gpu_window *first_window;
    ft_gpu_window *second_window;

    first_window = ft_gpu_window::create();
    second_window = ft_gpu_window::create();
    FT_ASSERT(first_window != nullptr);
    FT_ASSERT(second_window != nullptr);
    FT_ASSERT(first_window != second_window);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_window->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_window->destroy());
    delete first_window;
    delete second_window;
    return (1);
}

FT_TEST(test_gpgr_window_move_destroyed_source)
{
    ft_gpu_window *source_window;
    ft_gpu_window *destination_window;

    source_window = ft_gpu_window::create();
    destination_window = ft_gpu_window::create();
    FT_ASSERT(source_window != nullptr);
    FT_ASSERT(destination_window != nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_window->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_window->move(*source_window));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_window->destroy());
    delete source_window;
    delete destination_window;
    return (1);
}

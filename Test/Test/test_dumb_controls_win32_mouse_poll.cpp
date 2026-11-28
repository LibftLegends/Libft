#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

static uint8_t g_test_window_token;
static HWND g_test_foreground;
static POINT g_test_position;
static ft_bool g_test_warp_succeeds;
static uint32_t g_test_cursor_queries;
static uint32_t g_test_coordinate_conversions;
static uint32_t g_test_warp_calls;

static HWND test_dumb_get_foreground_window()
{
    return (g_test_foreground);
}

static BOOL test_dumb_get_cursor_position(POINT *position)
{
    ++g_test_cursor_queries;
    *position = g_test_position;
    return (TRUE);
}

static BOOL test_dumb_get_client_rect(HWND, RECT *rect)
{
    *rect = {0, 0, 80, 40};
    return (TRUE);
}

static BOOL test_dumb_convert_position(HWND, POINT *position)
{
    ++g_test_coordinate_conversions;
    position->x += 10;
    position->y += 20;
    return (TRUE);
}

static BOOL test_dumb_set_cursor_position(int32_t x, int32_t y)
{
    ++g_test_warp_calls;
    if (!g_test_warp_succeeds)
        return (FALSE);
    g_test_position = {x, y};
    return (TRUE);
}

# define GetForegroundWindow test_dumb_get_foreground_window
# define GetCursorPos test_dumb_get_cursor_position
# define GetClientRect test_dumb_get_client_rect
# define ClientToScreen test_dumb_convert_position
# define SetCursorPos test_dumb_set_cursor_position
# define ft_dumb_controls_win32_register_capture_window test_dumb_register_capture_window
# define ft_dumb_controls_win32_unregister_capture_window test_dumb_unregister_capture_window
# define ft_dumb_platform_control_is_down test_dumb_control_is_down
# define ft_dumb_platform_mouse_delta test_dumb_mouse_delta
# include "../../Modules/Compatebility/Compatebility_dumb_controls_win32.cpp"
# undef GetForegroundWindow
# undef GetCursorPos
# undef GetClientRect
# undef ClientToScreen
# undef SetCursorPos
# undef ft_dumb_controls_win32_register_capture_window
# undef ft_dumb_controls_win32_unregister_capture_window
# undef ft_dumb_platform_control_is_down
# undef ft_dumb_platform_mouse_delta

static void test_dumb_reset_mouse_poll()
{
    g_dumb_controls_win32_capture_window = reinterpret_cast<HWND>(&g_test_window_token);
    g_dumb_controls_win32_has_previous_mouse_position = FT_FALSE;
    g_test_foreground = g_dumb_controls_win32_capture_window;
    g_test_position = {50, 40};
    g_test_warp_succeeds = FT_TRUE;
    g_test_cursor_queries = 0;
    g_test_coordinate_conversions = 0;
    g_test_warp_calls = 0;
    return ;
}

FT_TEST(test_dumb_win32_mouse_poll_skips_redundant_platform_calls)
{
    test_dumb_reset_mouse_poll();
    const ft_dumb_mouse_delta centered = test_dumb_mouse_delta();
    FT_ASSERT_EQ(0, centered.x);
    FT_ASSERT_EQ(1U, g_test_coordinate_conversions);
    FT_ASSERT_EQ(0U, g_test_warp_calls);
    g_test_position = {57, 34};
    const ft_dumb_mouse_delta moved = test_dumb_mouse_delta();
    FT_ASSERT_EQ(7, moved.x);
    FT_ASSERT_EQ(-6, moved.y);
    FT_ASSERT_EQ(2U, g_test_coordinate_conversions);
    FT_ASSERT_EQ(1U, g_test_warp_calls);
    g_test_foreground = nullptr;
    const ft_dumb_mouse_delta unfocused = test_dumb_mouse_delta();
    FT_ASSERT_EQ(0, unfocused.x);
    FT_ASSERT_EQ(2U, g_test_cursor_queries);
    return (1);
}

FT_TEST(test_dumb_win32_failed_warp_preserves_actual_cursor_position)
{
    test_dumb_reset_mouse_poll();
    g_test_position = {60, 40};
    g_test_warp_succeeds = FT_FALSE;
    (void)test_dumb_mouse_delta();
    const ft_dumb_mouse_delta unchanged = test_dumb_mouse_delta();
    FT_ASSERT_EQ(0, unchanged.x);
    FT_ASSERT_EQ(0, unchanged.y);
    FT_ASSERT_EQ(2U, g_test_warp_calls);
    return (1);
}
#endif

#include "../test_internal.hpp"
#include "../../Modules/DUMB/controls.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_bool g_test_dumb_control_down[FT_DUMB_CONTROL_COUNT];
static ft_dumb_mouse_delta g_test_dumb_mouse_delta;

static ft_bool test_dumb_platform_control_is_down(ft_dumb_control control)
{
    if (control < FT_DUMB_CONTROL_UP || control >= FT_DUMB_CONTROL_COUNT)
        return (FT_FALSE);
    return (g_test_dumb_control_down[control]);
}

static ft_dumb_mouse_delta test_dumb_platform_mouse_delta(void)
{
    return (g_test_dumb_mouse_delta);
}

#define ft_dumb_platform_control_is_down test_dumb_platform_control_is_down
#define ft_dumb_platform_mouse_delta test_dumb_platform_mouse_delta
#include "../../Modules/DUMB/dumb_controls.cpp"
#undef ft_dumb_platform_control_is_down
#undef ft_dumb_platform_mouse_delta

FT_TEST(test_dumb_controls_pressed_tracks_down_transitions)
{
    g_test_dumb_control_down[FT_DUMB_CONTROL_JUMP] = FT_FALSE;
    ft_dumb_controls_poll();
    FT_ASSERT_EQ(FT_FALSE, ft_dumb_control_was_pressed(FT_DUMB_CONTROL_JUMP));
    g_test_dumb_control_down[FT_DUMB_CONTROL_JUMP] = FT_TRUE;
    ft_dumb_controls_poll();
    FT_ASSERT_EQ(FT_TRUE, ft_dumb_control_is_down(FT_DUMB_CONTROL_JUMP));
    FT_ASSERT_EQ(FT_TRUE, ft_dumb_control_was_pressed(FT_DUMB_CONTROL_JUMP));
    ft_dumb_controls_poll();
    FT_ASSERT_EQ(FT_TRUE, ft_dumb_control_is_down(FT_DUMB_CONTROL_JUMP));
    FT_ASSERT_EQ(FT_FALSE, ft_dumb_control_was_pressed(FT_DUMB_CONTROL_JUMP));
    g_test_dumb_control_down[FT_DUMB_CONTROL_JUMP] = FT_FALSE;
    ft_dumb_controls_poll();
    FT_ASSERT_EQ(FT_FALSE, ft_dumb_control_is_down(FT_DUMB_CONTROL_JUMP));
    FT_ASSERT_EQ(FT_FALSE, ft_dumb_control_was_pressed(FT_DUMB_CONTROL_JUMP));
    return (1);
}

FT_TEST(test_dumb_controls_mouse_delta_is_refreshed_on_each_poll)
{
    ft_dumb_mouse_delta mouse_delta;

    std::memset(g_test_dumb_control_down, 0, sizeof(g_test_dumb_control_down));
    g_test_dumb_mouse_delta.x = 12;
    g_test_dumb_mouse_delta.y = -8;
    ft_dumb_controls_poll();
    mouse_delta = ft_dumb_controls_mouse_delta();
    FT_ASSERT_EQ(12, mouse_delta.x);
    FT_ASSERT_EQ(-8, mouse_delta.y);

    g_test_dumb_mouse_delta.x = -5;
    g_test_dumb_mouse_delta.y = 19;
    ft_dumb_controls_poll();
    mouse_delta = ft_dumb_controls_mouse_delta();
    FT_ASSERT_EQ(-5, mouse_delta.x);
    FT_ASSERT_EQ(19, mouse_delta.y);

    return (1);
}

FT_TEST(test_dumb_controls_mouse_delta_can_return_to_zero_after_motion)
{
    ft_dumb_mouse_delta mouse_delta;

    std::memset(g_test_dumb_control_down, 0, sizeof(g_test_dumb_control_down));
    g_test_dumb_mouse_delta.x = 3;
    g_test_dumb_mouse_delta.y = 4;
    ft_dumb_controls_poll();
    mouse_delta = ft_dumb_controls_mouse_delta();
    FT_ASSERT_EQ(3, mouse_delta.x);
    FT_ASSERT_EQ(4, mouse_delta.y);

    g_test_dumb_mouse_delta.x = 0;
    g_test_dumb_mouse_delta.y = 0;
    ft_dumb_controls_poll();
    mouse_delta = ft_dumb_controls_mouse_delta();
    FT_ASSERT_EQ(0, mouse_delta.x);
    FT_ASSERT_EQ(0, mouse_delta.y);

    return (1);
}

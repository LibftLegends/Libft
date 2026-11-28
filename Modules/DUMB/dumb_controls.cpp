#include "dumb_controls_internal.hpp"

static ft_bool g_mouse_captured = FT_TRUE;
static ft_bool g_dumb_control_down[FT_DUMB_CONTROL_COUNT];
static ft_bool g_dumb_control_pressed[FT_DUMB_CONTROL_COUNT];
static ft_dumb_mouse_delta g_dumb_mouse_delta;
static ft_dumb_keyboard_layout g_dumb_keyboard_layout =
    FT_DUMB_KEYBOARD_LAYOUT_QWERTY;

#ifdef LIBFT_TEST_BUILD
static ft_dumb_test_platform_control_is_down_fn g_dumb_test_platform_control_is_down =
    ft_dumb_platform_control_is_down;
static ft_dumb_test_platform_mouse_delta_fn g_dumb_test_platform_mouse_delta =
    ft_dumb_platform_mouse_delta;
#endif

#ifdef LIBFT_TEST_BUILD
void ft_dumb_controls_set_test_platform_control_is_down(
    ft_dumb_test_platform_control_is_down_fn control_provider)
{
    if (control_provider == nullptr)
        g_dumb_test_platform_control_is_down = ft_dumb_platform_control_is_down;
    else
        g_dumb_test_platform_control_is_down = control_provider;
    return ;
}

void ft_dumb_controls_set_test_platform_mouse_delta(
    ft_dumb_test_platform_mouse_delta_fn mouse_delta_provider)
{
    if (mouse_delta_provider == nullptr)
        g_dumb_test_platform_mouse_delta = ft_dumb_platform_mouse_delta;
    else
        g_dumb_test_platform_mouse_delta = mouse_delta_provider;
    return ;
}

void ft_dumb_controls_clear_test_platform_hooks(void)
{
    g_dumb_test_platform_control_is_down = ft_dumb_platform_control_is_down;
    g_dumb_test_platform_mouse_delta = ft_dumb_platform_mouse_delta;
    return ;
}
#endif

void ft_dumb_controls_set_mouse_captured(ft_bool captured)
{
    g_mouse_captured = captured;
}

ft_bool ft_dumb_controls_get_mouse_captured(void)
{
    return (g_mouse_captured);
}

void ft_dumb_controls_poll(void)
{
    uint32_t index_control;
    ft_bool  current_down;

    index_control = 0U;
    while (index_control < FT_DUMB_CONTROL_COUNT)
    {
#ifdef LIBFT_TEST_BUILD
        current_down = g_dumb_test_platform_control_is_down(
            static_cast<ft_dumb_control>(index_control));
#else
        current_down = ft_dumb_platform_control_is_down(
            static_cast<ft_dumb_control>(index_control));
#endif
        g_dumb_control_pressed[index_control] = current_down == FT_TRUE
            && g_dumb_control_down[index_control] == FT_FALSE;
        g_dumb_control_down[index_control] = current_down;
        index_control = index_control + 1U;
    }
#ifdef LIBFT_TEST_BUILD
    g_dumb_mouse_delta = g_dumb_test_platform_mouse_delta();
#else
    g_dumb_mouse_delta = ft_dumb_platform_mouse_delta();
#endif
    return ;
}

ft_bool ft_dumb_control_is_down(ft_dumb_control control)
{
    if (control < FT_DUMB_CONTROL_UP || control >= FT_DUMB_CONTROL_COUNT)
    {
        return (FT_FALSE);
    }
    return (g_dumb_control_down[control]);
}

ft_bool ft_dumb_control_was_pressed(ft_dumb_control control)
{
    if (control < FT_DUMB_CONTROL_UP || control >= FT_DUMB_CONTROL_COUNT)
    {
        return (FT_FALSE);
    }
    return (g_dumb_control_pressed[control]);
}

ft_dumb_mouse_delta ft_dumb_controls_mouse_delta(void)
{
    return (g_dumb_mouse_delta);
}

void ft_dumb_controls_set_keyboard_layout(ft_dumb_keyboard_layout layout)
{
    if (layout != FT_DUMB_KEYBOARD_LAYOUT_AZERTY)
        g_dumb_keyboard_layout = FT_DUMB_KEYBOARD_LAYOUT_QWERTY;
    else
        g_dumb_keyboard_layout = FT_DUMB_KEYBOARD_LAYOUT_AZERTY;
    return ;
}

ft_dumb_keyboard_layout ft_dumb_controls_get_keyboard_layout(void)
{
    return (g_dumb_keyboard_layout);
}

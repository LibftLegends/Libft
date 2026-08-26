#include "../test_internal.hpp"
#include "../../Modules/DUMB/render_window.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t test_dumb_gradient_shader(
    const ft_render_shader_input *input,
    ft_render_shader_output *output)
{
    output->color = 0xFF000000U
        | (static_cast<uint32_t>(input->coordinate_x) << 16)
        | (static_cast<uint32_t>(input->coordinate_y) << 8)
        | static_cast<uint32_t>(input->current_depth);
    output->depth = static_cast<uint8_t>(
        input->coordinate_x + input->coordinate_y);
    output->write_depth = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static int32_t test_dumb_tint_shader(
    const ft_render_shader_input *input,
    ft_render_shader_output *output)
{
    uint32_t *tint_color;

    tint_color = static_cast<uint32_t *>(input->user_data);
    output->color = input->current_color ^ *tint_color;
    output->depth = input->current_depth;
    output->write_depth = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static int32_t test_dumb_failing_shader(
    const ft_render_shader_input *input,
    ft_render_shader_output *output)
{
    (void)input;
    (void)output;
    return (FT_ERR_INVALID_ARGUMENT);
}

FT_TEST(test_dumb_render_shader_runs_cpu_side_for_each_pixel)
{
    ft_render_window render_window_instance;
    uint32_t pixels[6];
    uint8_t *depth_values;

    depth_values = new uint8_t[6];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    pixels[0] = 0U;
    pixels[1] = 0U;
    pixels[2] = 0U;
    pixels[3] = 0U;
    pixels[4] = 0U;
    pixels[5] = 0U;
    depth_values[0] = 10U;
    depth_values[1] = 20U;
    depth_values[2] = 30U;
    depth_values[3] = 40U;
    depth_values[4] = 50U;
    depth_values[5] = 60U;
    render_window_instance._framebuffer.width = 3;
    render_window_instance._framebuffer.height = 2;
    render_window_instance._framebuffer.pixels = pixels;
    render_window_instance._depth_buffer.width = 3;
    render_window_instance._depth_buffer.height = 2;
    render_window_instance._depth_buffer.values = depth_values;
    render_window_instance._is_initialised = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        render_window_instance.shade(test_dumb_gradient_shader, ft_nullptr));
    FT_ASSERT_EQ(0xFF00000AU, pixels[0]);
    FT_ASSERT_EQ(0xFF010014U, pixels[1]);
    FT_ASSERT_EQ(0xFF02001EU, pixels[2]);
    FT_ASSERT_EQ(0xFF000128U, pixels[3]);
    FT_ASSERT_EQ(0xFF010132U, pixels[4]);
    FT_ASSERT_EQ(0xFF02013CU, pixels[5]);
    FT_ASSERT_EQ(0U, depth_values[0]);
    FT_ASSERT_EQ(1U, depth_values[1]);
    FT_ASSERT_EQ(2U, depth_values[2]);
    FT_ASSERT_EQ(1U, depth_values[3]);
    FT_ASSERT_EQ(2U, depth_values[4]);
    FT_ASSERT_EQ(3U, depth_values[5]);
    render_window_instance._is_initialised = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_shader_uses_user_data_and_keeps_depth_when_requested)
{
    ft_render_window render_window_instance;
    uint32_t pixels[2];
    uint8_t *depth_values;
    uint32_t tint_color;

    depth_values = new uint8_t[2];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    pixels[0] = 0x00FF00FFU;
    pixels[1] = 0x0000FFFFU;
    depth_values[0] = 12U;
    depth_values[1] = 34U;
    tint_color = 0x000F0F0FU;
    render_window_instance._framebuffer.width = 2;
    render_window_instance._framebuffer.height = 1;
    render_window_instance._framebuffer.pixels = pixels;
    render_window_instance._depth_buffer.width = 2;
    render_window_instance._depth_buffer.height = 1;
    render_window_instance._depth_buffer.values = depth_values;
    render_window_instance._is_initialised = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        render_window_instance.shade(test_dumb_tint_shader, &tint_color));
    FT_ASSERT_EQ(0x00F00FF0U, pixels[0]);
    FT_ASSERT_EQ(0x000FF0F0U, pixels[1]);
    FT_ASSERT_EQ(12U, depth_values[0]);
    FT_ASSERT_EQ(34U, depth_values[1]);
    render_window_instance._is_initialised = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_shader_reports_invalid_inputs)
{
    ft_render_window render_window_instance;
    uint32_t pixels[1];
    uint8_t *depth_values;

    depth_values = new uint8_t[1];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        render_window_instance.shade(ft_nullptr, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        render_window_instance.shade(test_dumb_gradient_shader, ft_nullptr));
    pixels[0] = 0x12345678U;
    depth_values[0] = 7U;
    render_window_instance._framebuffer.width = 1;
    render_window_instance._framebuffer.height = 1;
    render_window_instance._framebuffer.pixels = pixels;
    render_window_instance._depth_buffer.width = 1;
    render_window_instance._depth_buffer.height = 1;
    render_window_instance._depth_buffer.values = depth_values;
    render_window_instance._is_initialised = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        render_window_instance.shade(test_dumb_failing_shader, ft_nullptr));
    FT_ASSERT_EQ(0x12345678U, pixels[0]);
    FT_ASSERT_EQ(7U, depth_values[0]);
    delete[] depth_values;
    depth_values = ft_nullptr;
    render_window_instance._depth_buffer.values = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        render_window_instance.shade(test_dumb_gradient_shader, ft_nullptr));
    render_window_instance._is_initialised = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

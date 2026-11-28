#include "../../../Modules/DUMB/render_window.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

static int32_t tint_fragment(const ft_render_shader_input *input,
    ft_render_shader_output *output)
{
    output->color = input->current_color + static_cast<uint32_t>(input->coordinate_x);
    output->depth = static_cast<uint8_t>(input->current_depth + 1U);
    output->write_depth = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int main()
{
    constexpr int32_t WIDTH = 640;
    constexpr int32_t HEIGHT = 480;
    constexpr uint32_t FRAMES = 180U;
    std::vector<uint32_t> pixels(WIDTH * HEIGHT);
    std::vector<uint8_t> depth(WIDTH * HEIGHT);
    ft_render_window window;
    if (window.initialize() != FT_ERR_SUCCESS)
        return (1);
    window._framebuffer = {WIDTH, HEIGHT, pixels.data()};
    window._depth_buffer = {WIDTH, HEIGHT, depth.data()};
    window._is_initialised = FT_TRUE;
    uint32_t frame = 0U;
    int32_t status = FT_ERR_SUCCESS;
    const auto start = std::chrono::steady_clock::now();
    while (frame < FRAMES && status == FT_ERR_SUCCESS)
    {
        status = window.shade(tint_fragment, ft_nullptr);
        ++frame;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    window._is_initialised = FT_FALSE;
    window._depth_buffer.values = ft_nullptr;
    if (status != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < pixels.size())
    {
        checksum = checksum * UINT64_C(31) + pixels[index] + depth[index];
        ++index;
    }
    std::printf("shader checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / FRAMES);
    return (0);
}

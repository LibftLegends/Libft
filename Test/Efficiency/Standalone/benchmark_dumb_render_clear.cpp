#include "../../../Modules/DUMB/render_window.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

#ifndef LIBFT_TEST_BUILD
# error "The offscreen benchmark requires LIBFT_TEST_BUILD."
#endif

int main(int32_t argument_count, char **arguments)
{
    constexpr int32_t WIDTH = 1920;
    constexpr int32_t HEIGHT = 1080;
    constexpr uint32_t FRAMES = 128;
    const ft_bool repeated = argument_count == 2 && ft_strcmp(arguments[1], "--repeated") == 0;
    std::vector<uint32_t> pixels(WIDTH * HEIGHT);
    ft_render_window window;
    if (window.initialize() != FT_ERR_SUCCESS)
        return (1);
    window._framebuffer = {WIDTH, HEIGHT, pixels.data()};
    window._is_initialised = FT_TRUE;

    uint32_t frame = 0;
    const auto start = std::chrono::steady_clock::now();
    while (frame < FRAMES)
    {
        uint32_t color = 0xFE012300U + frame;
        if (repeated)
            color = frame * 0x01010101U;
        if (window.clear(color) != FT_ERR_SUCCESS)
        {
            window._is_initialised = FT_FALSE;
            return (1);
        }
        ++frame;
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    window._is_initialised = FT_FALSE;

    uint64_t checksum = 0;
    ft_size_t index = 0;
    while (index < pixels.size())
    {
        checksum = checksum * 31U + pixels[index];
        ++index;
    }
    std::printf("clear %.3f ms/frame checksum=%" PRIu64 "\n", elapsed / FRAMES, checksum);
    return (0);
}

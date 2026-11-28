#if defined(_WIN32)

#include "../../../Modules/DUMB/render_window.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct window_search
{
    const char *title;
    HWND handle;
};

static BOOL CALLBACK find_window_by_title(HWND handle, LPARAM parameter)
{
    window_search *search = reinterpret_cast<window_search *>(parameter);
    char title[64];
    if (GetWindowTextA(handle, title, sizeof(title)) != 0
        && lstrcmpA(title, search->title) == 0)
    {
        search->handle = handle;
        return (FALSE);
    }
    return (TRUE);
}

static int32_t check_windows()
{
    constexpr int32_t FRAME_COUNT = 1000;
    constexpr uint32_t CLEAR_COLOR = 0x00336699;
    const ft_render_window_desc first_description = {
        640, 480, "Libft Win32 lifecycle first", FT_RENDER_WINDOW_FLAG_NONE
    };
    const ft_render_window_desc second_description = {
        320, 240, "Libft Win32 lifecycle second", FT_RENDER_WINDOW_FLAG_NONE
    };
    ft_render_window first;
    ft_render_window second;
    ft_render_window moved;
    if (first.initialize() != FT_ERR_SUCCESS
        || second.initialize() != FT_ERR_SUCCESS
        || first.initialize(first_description) != FT_ERR_SUCCESS
        || second.initialize(second_description) != FT_ERR_SUCCESS
        || moved.initialize() != FT_ERR_SUCCESS
        || first.clear(CLEAR_COLOR) != FT_ERR_SUCCESS)
        return (1);
    uint32_t *pixels = first.framebuffer().pixels;
    uint8_t *depth_values = first.depth_buffer().values;
    if (moved.move(first) != FT_ERR_SUCCESS
        || moved.framebuffer().pixels != pixels
        || moved.depth_buffer().values != depth_values
        || moved.framebuffer().pixels[0] != CLEAR_COLOR)
        return (2);

    window_search first_search = { first_description.title, NULL };
    window_search second_search = { second_description.title, NULL };
    EnumThreadWindows(GetCurrentThreadId(), find_window_by_title,
        reinterpret_cast<LPARAM>(&first_search));
    EnumThreadWindows(GetCurrentThreadId(), find_window_by_title,
        reinterpret_cast<LPARAM>(&second_search));
    HWND first_handle = first_search.handle;
    HWND second_handle = second_search.handle;
    if (first_handle == NULL || second_handle == NULL)
        return (3);

    const auto start = std::chrono::steady_clock::now();
    int32_t frame_index = 0;
    while (frame_index < FRAME_COUNT)
    {
        if (moved.present() != FT_ERR_SUCCESS)
            return (4);
        ++frame_index;
    }
    const std::chrono::duration<double, std::micro> elapsed =
        std::chrono::steady_clock::now() - start;
    std::printf("%.3f us/present\n", elapsed.count() / FRAME_COUNT);

    if (PostMessageA(first_handle, WM_CLOSE, 0, 0) == 0
        || second.poll_events() != FT_ERR_SUCCESS || second.should_close() != FT_FALSE
        || moved.poll_events() != FT_ERR_SUCCESS || moved.should_close() != FT_TRUE
        || moved.poll_events() != FT_ERR_SUCCESS || moved.should_close() != FT_TRUE
        || IsWindowVisible(first_handle) != 0)
        return (5);
    moved.shutdown();
    if (IsWindow(first_handle) != 0
        || second.poll_events() != FT_ERR_SUCCESS || second.should_close() != FT_FALSE
        || second.present() != FT_ERR_SUCCESS)
        return (6);
    second.shutdown();
    if (IsWindow(second_handle) != 0)
        return (7);

    if (moved.initialize(first_description) != FT_ERR_SUCCESS
        || moved.poll_events() != FT_ERR_SUCCESS || moved.should_close() != FT_FALSE
        || moved.present() != FT_ERR_SUCCESS)
        return (8);
    moved.shutdown();
    return (0);
}

int main()
{
    char desktop_name[64];
    std::snprintf(desktop_name, sizeof(desktop_name), "LibftDumb-%" PRIu32,
        static_cast<uint32_t>(GetCurrentProcessId()));
    HDESK private_desktop = NULL;
    int32_t status = 0;
    std::thread renderer_thread([&desktop_name, &private_desktop, &status]()
        {
            private_desktop = CreateDesktopA(desktop_name, NULL, NULL, 0, GENERIC_ALL, NULL);
            if (private_desktop == NULL)
            {
                std::fprintf(stderr, "CreateDesktopA failed: %" PRIu32 "\n",
                    static_cast<uint32_t>(GetLastError()));
                status = 9;
                return ;
            }
            status = check_windows();
            return ;
        });
    renderer_thread.join();
    if (private_desktop != NULL && CloseDesktop(private_desktop) == 0)
    {
        std::fprintf(stderr, "CloseDesktop failed: %" PRIu32 "\n",
            static_cast<uint32_t>(GetLastError()));
        return (10);
    }
    if (status != 0)
        std::fprintf(stderr, "Win32 window lifecycle check failed at step %d\n", status);
    return (status);
}

#else

int main()
{
    return (0);
}

#endif

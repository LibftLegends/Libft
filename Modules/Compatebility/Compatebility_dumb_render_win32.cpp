#if defined(_WIN32)

#include "../DUMB/dumb_render_internal.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct ft_render_win32_state
{
    HINSTANCE       instance_handle;
    HWND            window_handle;
    HDC             window_device_context;

    HDC             memory_device_context;
    HBITMAP         dib_section;
    HGDIOBJ         previous_bitmap;

    int32_t             width;
    int32_t             height;

    ft_bool            is_fullscreen;

    DWORD           window_style;
    RECT            windowed_rect;
};

static LRESULT CALLBACK ft_render_win32_wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CLOSE)
    {
        // Keep the request on the HWND so it follows a moved renderer.
        SetWindowLongPtr(hwnd, GWLP_USERDATA, FT_TRUE);
        ShowWindow(hwnd, SW_HIDE);
        return (0);
    }
    return (DefWindowProc(hwnd, message, wparam, lparam));
}

static void ft_render_win32_destroy_state(ft_render_win32_state *state)
{
    if (state->memory_device_context != NULL)
    {
        if (state->previous_bitmap != NULL)
            SelectObject(state->memory_device_context, state->previous_bitmap);
        DeleteDC(state->memory_device_context);
    }
    if (state->dib_section != NULL)
        DeleteObject(state->dib_section);
    if (state->window_device_context != NULL)
        ReleaseDC(state->window_handle, state->window_device_context);
    if (state->window_handle != NULL)
        DestroyWindow(state->window_handle);
    HeapFree(GetProcessHeap(), 0, state);
    return ;
}

static ft_render_platform_result ft_render_win32_create_dib(
    ft_render_win32_state *state,
    ft_render_framebuffer *out_framebuffer
)
{
    BITMAPINFO      bitmap_info;
    void            *pixels;

    ZeroMemory(&bitmap_info, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = state->width;
    bitmap_info.bmiHeader.biHeight = -state->height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    pixels = NULL;
    state->dib_section = CreateDIBSection(
        state->window_device_context,
        &bitmap_info,
        DIB_RGB_COLORS,
        &pixels,
        NULL,
        0
    );

    if (state->dib_section == NULL || pixels == NULL)
    {
        ft_render_platform_result result;

        result.error_code = FT_ERR_INITIALIZATION_FAILED;
        result.system_error_code = static_cast<int32_t>(GetLastError());
        return (result);
    }

    state->memory_device_context = CreateCompatibleDC(state->window_device_context);
    if (state->memory_device_context == NULL)
        return ((ft_render_platform_result){ FT_ERR_INITIALIZATION_FAILED, static_cast<int32_t>(GetLastError()) });
    state->previous_bitmap = SelectObject(state->memory_device_context, state->dib_section);
    if (state->previous_bitmap == NULL)
        return ((ft_render_platform_result){ FT_ERR_INITIALIZATION_FAILED, static_cast<int32_t>(GetLastError()) });

    out_framebuffer->width = state->width;
    out_framebuffer->height = state->height;
    out_framebuffer->pixels = static_cast<uint32_t *>(pixels);

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

ft_render_platform_result ft_render_platform_get_primary_screen_size(ft_render_screen_size *out_size)
{
    if (out_size == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_INVALID_ARGUMENT, 0 });
    }

    out_size->width = GetSystemMetrics(SM_CXSCREEN);
    out_size->height = GetSystemMetrics(SM_CYSCREEN);

    if (out_size->width <= 0 || out_size->height <= 0)
    {
        return ((ft_render_platform_result){ FT_ERR_INITIALIZATION_FAILED, static_cast<int32_t>(GetLastError()) });
    }

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

ft_render_platform_result ft_render_platform_create_window(
    void **out_platform_state,
    ft_render_framebuffer *out_framebuffer,
    const ft_render_window_desc &desc
)
{
    ft_render_win32_state  *state;
    WNDCLASS               window_class;
    DWORD                  style;
    RECT                   rect;
    HWND                   hwnd;
    ft_bool                fullscreen;
    int32_t                window_x;
    int32_t                window_y;

    if (out_platform_state == NULL || out_framebuffer == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_INVALID_ARGUMENT, 0 });
    }

    state = static_cast<ft_render_win32_state *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ft_render_win32_state)));
    if (state == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_NO_MEMORY, 0 });
    }

    state->instance_handle = GetModuleHandle(NULL);
    fullscreen = FT_FALSE;
    if ((desc.flags & FT_RENDER_WINDOW_FLAG_FULLSCREEN) != 0)
        fullscreen = FT_TRUE;
    if (fullscreen == FT_TRUE)
    {
        state->width = GetSystemMetrics(SM_CXSCREEN);
        state->height = GetSystemMetrics(SM_CYSCREEN);
    }
    else
    {
        state->width = desc.width;
        state->height = desc.height;
    }
    state->is_fullscreen = fullscreen;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = ft_render_win32_wndproc;
    window_class.hInstance = state->instance_handle;
    window_class.lpszClassName = "ft_render_window_class";

    if (RegisterClass(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        ft_render_platform_result result;

        result.error_code = FT_ERR_INITIALIZATION_FAILED;
        result.system_error_code = static_cast<int32_t>(GetLastError());
        HeapFree(GetProcessHeap(), 0, state);
        return (result);
    }

    if (fullscreen == FT_TRUE)
        style = WS_POPUP;
    else
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (fullscreen == FT_FALSE && (desc.flags & FT_RENDER_WINDOW_FLAG_RESIZABLE) != 0)
    {
        style = style | WS_SIZEBOX | WS_MAXIMIZEBOX;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = state->width;
    rect.bottom = state->height;
    AdjustWindowRect(&rect, style, FALSE);
    window_x = CW_USEDEFAULT;
    window_y = CW_USEDEFAULT;
    if (fullscreen == FT_TRUE)
    {
        window_x = 0;
        window_y = 0;
    }

    hwnd = CreateWindowEx(
        0,
        window_class.lpszClassName,
        desc.title,
        style,
        window_x,
        window_y,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        state->instance_handle,
        NULL
    );

    if (hwnd == NULL)
    {
        ft_render_platform_result result;

        result.error_code = FT_ERR_INITIALIZATION_FAILED;
        result.system_error_code = static_cast<int32_t>(GetLastError());
        HeapFree(GetProcessHeap(), 0, state);
        return (result);
    }

    state->window_handle = hwnd;
    state->window_style = style;
    SetRect(&state->windowed_rect, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

    state->window_device_context = GetDC(hwnd);
    if (state->window_device_context == NULL)
    {
        ft_render_platform_result result;

        result.error_code = FT_ERR_INITIALIZATION_FAILED;
        result.system_error_code = static_cast<int32_t>(GetLastError());
        ft_render_win32_destroy_state(state);
        return (result);
    }

    {
        ft_render_platform_result dib_result;

        dib_result = ft_render_win32_create_dib(state, out_framebuffer);
        if (dib_result.error_code != FT_ERR_SUCCESS)
        {
            ft_render_win32_destroy_state(state);
            return (dib_result);
        }
    }

    SetStretchBltMode(state->window_device_context, COLORONCOLOR);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    *out_platform_state = state;
    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

ft_render_platform_result ft_render_platform_destroy_window(
    void **platform_state,
    ft_render_framebuffer *framebuffer
)
{
    if (platform_state == NULL || *platform_state == NULL)
        return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });

    ft_render_win32_destroy_state(static_cast<ft_render_win32_state *>(*platform_state));
    *platform_state = NULL;

    if (framebuffer != NULL)
    {
        framebuffer->width = 0;
        framebuffer->height = 0;
        framebuffer->pixels = NULL;
    }

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

ft_render_platform_result ft_render_platform_poll_events(
    void *platform_state,
    ft_bool *out_should_close
)
{
    MSG message;
    ft_render_win32_state *state;

    if (platform_state == NULL || out_should_close == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_INVALID_ARGUMENT, 0 });
    }

    *out_should_close = FT_FALSE;

    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) != 0)
    {
        if (message.message == WM_QUIT)
        {
            *out_should_close = FT_TRUE;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    state = static_cast<ft_render_win32_state *>(platform_state);
    if (GetWindowLongPtr(state->window_handle, GWLP_USERDATA) == FT_TRUE)
        *out_should_close = FT_TRUE;

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

ft_render_platform_result ft_render_platform_present(
    void *platform_state,
    ft_render_framebuffer *framebuffer,
    const ft_render_depth_buffer *depth_buffer
)
{
    ft_render_win32_state  *state;
    RECT                   client_rect;

    (void)depth_buffer;

    if (platform_state == NULL || framebuffer == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_INVALID_ARGUMENT, 0 });
    }

    state = static_cast<ft_render_win32_state *>(platform_state);

    if (GetClientRect(state->window_handle, &client_rect) == 0)
        return ((ft_render_platform_result){ FT_ERR_IO, static_cast<int32_t>(GetLastError()) });
    if (client_rect.right == client_rect.left || client_rect.bottom == client_rect.top)
        return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
    if (StretchBlt(
        state->window_device_context,
        0,
        0,
        client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top,
        state->memory_device_context,
        0,
        0,
        framebuffer->width,
        framebuffer->height,
        SRCCOPY
    ) == 0)
        return ((ft_render_platform_result){ FT_ERR_IO, 0 });

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

static void ft_render_win32_enter_fullscreen(ft_render_win32_state *state)
{
    MONITORINFO monitor_info;
    HMONITOR    monitor;

    GetWindowRect(state->window_handle, &state->windowed_rect);

    monitor = MonitorFromWindow(state->window_handle, MONITOR_DEFAULTTONEAREST);
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(monitor, &monitor_info);

    SetWindowLong(state->window_handle, GWL_STYLE, state->window_style & ~WS_OVERLAPPEDWINDOW);

    SetWindowPos(
        state->window_handle,
        HWND_TOP,
        monitor_info.rcMonitor.left,
        monitor_info.rcMonitor.top,
        monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
        monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED
    );

    state->is_fullscreen = FT_TRUE;
    return ;
}

static void ft_render_win32_leave_fullscreen(ft_render_win32_state *state)
{
    SetWindowLong(state->window_handle, GWL_STYLE, state->window_style);

    SetWindowPos(
        state->window_handle,
        NULL,
        state->windowed_rect.left,
        state->windowed_rect.top,
        state->windowed_rect.right - state->windowed_rect.left,
        state->windowed_rect.bottom - state->windowed_rect.top,
        SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
    );

    state->is_fullscreen = FT_FALSE;
    return ;
}

ft_render_platform_result ft_render_platform_set_fullscreen(
    void *platform_state,
    ft_bool enabled
)
{
    ft_render_win32_state  *state;

    if (platform_state == NULL)
    {
        return ((ft_render_platform_result){ FT_ERR_INVALID_ARGUMENT, 0 });
    }

    state = static_cast<ft_render_win32_state *>(platform_state);

    if (enabled == FT_TRUE && state->is_fullscreen == FT_FALSE)
    {
        ft_render_win32_enter_fullscreen(state);
    }
    else if (enabled == FT_FALSE && state->is_fullscreen == FT_TRUE)
    {
        ft_render_win32_leave_fullscreen(state);
    }

    return ((ft_render_platform_result){ FT_ERR_SUCCESS, 0 });
}

#endif

#if defined(_WIN32)

#include "gpgr_window_windows.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"

#define WIN32_LEAN_AND_MEAN

class ft_gpu_window_windows : public ft_gpu_window
{
    HWND _hwnd;
    HDC _dc;
    HGLRC _ctx;
    uint8_t _initialised_state;
    ft_bool _close_flag;
    int32_t _width;
    int32_t _height;
    int32_t _mouse_x;
    int32_t _mouse_y;
    uint32_t _primary_presses;
    ft_bool _settings_key_pressed;
    ft_bool _cursor_visible;

    public:
        ft_gpu_window_windows() noexcept
            : _hwnd(nullptr), _dc(nullptr), _ctx(nullptr),
              _initialised_state(FT_CLASS_STATE_UNINITIALISED),
              _close_flag(FT_FALSE), _width(0), _height(0), _mouse_x(0),
              _mouse_y(0), _primary_presses(0U),
              _settings_key_pressed(FT_FALSE), _cursor_visible(FT_TRUE) {}
        ft_gpu_window_windows(const ft_gpu_window_windows &other) = delete;
        ft_gpu_window_windows(ft_gpu_window_windows &&other) = delete;
        ft_gpu_window_windows &operator=(const ft_gpu_window_windows &other)
            = delete;
        ft_gpu_window_windows &operator=(ft_gpu_window_windows &&other)
            = delete;
        ~ft_gpu_window_windows() override { (void)destroy(); }

        ft_bool initialize(const char *title, int32_t width, int32_t height,
            ft_bool fullscreen) noexcept override;
        int32_t destroy() noexcept override;
        void poll_events() noexcept override;
        void swap_buffers() noexcept override;
        ft_bool should_close() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::should_close");
            return (this->_close_flag);
        }
        int32_t get_width() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::get_width");
            return (this->_width);
        }
        int32_t get_height() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::get_height");
            return (this->_height);
        }
        int32_t get_mouse_x() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::get_mouse_x");
            return (this->_mouse_x);
        }
        int32_t get_mouse_y() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::get_mouse_y");
            return (this->_mouse_y);
        }
        uint32_t primary_press_count() const noexcept override;
        ft_bool was_mouse_clicked() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::was_mouse_clicked");
            return (this->_primary_presses != 0U);
        }
        void set_cursor_visible(ft_bool visible) noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::set_cursor_visible");
            if (this->_cursor_visible == visible)
                return ;
            if (visible == FT_TRUE)
            {
                ft_dumb_controls_win32_unregister_capture_window();
                ShowCursor(TRUE);
            }
            else
            {
                ShowCursor(FALSE);
                if (this->_hwnd != nullptr)
                    ft_dumb_controls_win32_register_capture_window(this->_hwnd);
            }
            this->_cursor_visible = visible;
            return ;
        }
        ft_bool was_settings_key_pressed() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_windows::was_settings_key_pressed");
            return (this->_settings_key_pressed);
        }
        int32_t move(ft_gpu_window &other) noexcept override;
        void request_close() noexcept { this->_close_flag = FT_TRUE; }
        void set_size(int32_t width, int32_t height) noexcept
        {
            this->_width = width;
            this->_height = height;
        }
        void set_mouse_position(int32_t x, int32_t y) noexcept
        {
            this->_mouse_x = x;
            this->_mouse_y = y;
        }
        void record_primary_press() noexcept
        {
            if (this->_primary_presses < UINT32_MAX)
                this->_primary_presses += 1U;
        }
        void set_settings_key_pressed(ft_bool pressed) noexcept
        {
            this->_settings_key_pressed = pressed;
            return ;
        }
        ft_bool is_cursor_visible() const noexcept
        {
            return (this->_cursor_visible);
        }
};

static LRESULT CALLBACK gpgr_window_proc(HWND hwnd, UINT msg, WPARAM wparam,
    LPARAM lparam)
{
    ft_gpu_window_windows *window = reinterpret_cast<ft_gpu_window_windows *>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (msg)
    {
        case WM_NCCREATE:
        {
            CREATESTRUCTA *create = reinterpret_cast<CREATESTRUCTA *>(lparam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return (TRUE);
        }
        case WM_CLOSE:
            if (window != nullptr)
                window->request_close();
            DestroyWindow(hwnd);
            return (0);
        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE)
                ft_dumb_controls_win32_unregister_capture_window();
            else if (window != nullptr && window->is_cursor_visible() == FT_FALSE)
                ft_dumb_controls_win32_register_capture_window(hwnd);
            return (0);
        case WM_KEYDOWN:
            if (window != nullptr && wparam == VK_F1)
                window->set_settings_key_pressed(FT_TRUE);
            return (0);
        case WM_LBUTTONDOWN:
            if (window != nullptr)
                window->record_primary_press();
            return (0);
        case WM_MOUSEMOVE:
            if (window != nullptr)
                window->set_mouse_position(GET_X_LPARAM(lparam),
                    GET_Y_LPARAM(lparam));
            return (0);
        case WM_DESTROY:
            ft_dumb_controls_win32_unregister_capture_window();
            PostQuitMessage(0);
            return (0);
        case WM_SIZE:
            if (window != nullptr)
                window->set_size(LOWORD(lparam), HIWORD(lparam));
            return (0);
        default:
            return (DefWindowProcA(hwnd, msg, wparam, lparam));
    }
    return (DefWindowProcA(hwnd, msg, wparam, lparam));
}

ft_bool ft_gpu_window_windows::initialize(const char *title, int32_t width,
    int32_t height, ft_bool fullscreen) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_gpu_window_windows::initialize", "called while object is already initialised");
    static const char *class_name = "ft_vox_gpu_window";

    HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSA wc;
    ft_memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = gpgr_window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = class_name;

    if (RegisterClassA(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return (FT_FALSE);

    DWORD style;
    int32_t window_x;
    int32_t window_y;

    if (fullscreen == FT_TRUE)
    {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }

    if (fullscreen == FT_TRUE)
        style = WS_POPUP;
    else
        style = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, style, FALSE);

    this->_width = width;
    this->_height = height;
    this->_close_flag = FT_FALSE;
    this->_mouse_x = 0;
    this->_mouse_y = 0;
    this->_primary_presses = 0U;
    this->_settings_key_pressed = FT_FALSE;
    this->_cursor_visible = FT_TRUE;
    window_x = CW_USEDEFAULT;
    window_y = CW_USEDEFAULT;
    if (fullscreen == FT_TRUE)
    {
        window_x = 0;
        window_y = 0;
    }

    this->_hwnd = CreateWindowExA(0, class_name, title, style,
        window_x, window_y, rect.right - rect.left,
        rect.bottom - rect.top, nullptr, nullptr, instance, this);
    if (this->_hwnd == nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    this->_dc = GetDC(this->_hwnd);
    if (this->_dc == nullptr)
    {
        (void)destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    PIXELFORMATDESCRIPTOR pfd;
    ft_memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int32_t pixel_format = ChoosePixelFormat(this->_dc, &pfd);
    if (pixel_format == 0 || SetPixelFormat(this->_dc, pixel_format, &pfd) == FALSE)
    {
        (void)destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    this->_ctx = wglCreateContext(this->_dc);
    if (this->_ctx == nullptr || wglMakeCurrent(this->_dc, this->_ctx) == FALSE)
    {
        (void)destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    if (gpgr_load_gl_functions() == FT_FALSE)
    {
        (void)destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    ShowWindow(this->_hwnd, SW_SHOW);
    UpdateWindow(this->_hwnd);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_TRUE);
}

int32_t ft_gpu_window_windows::destroy() noexcept
{
    if (this->_ctx != nullptr)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(this->_ctx);
        this->_ctx = nullptr;
    }
    if (this->_dc != nullptr && this->_hwnd != nullptr)
    {
        ReleaseDC(this->_hwnd, this->_dc);
        this->_dc = nullptr;
    }
    if (this->_hwnd != nullptr)
    {
        ft_dumb_controls_win32_unregister_capture_window();
        DestroyWindow(this->_hwnd);
        this->_hwnd = nullptr;
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_gpu_window_windows::move(ft_gpu_window &other) noexcept
{
    ft_gpu_window_windows &other_window =
        static_cast<ft_gpu_window_windows &>(other);

    if (this == &other_window)
        return (FT_ERR_SUCCESS);
    if (other_window._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other_window._initialised_state,
            "ft_gpu_window_windows::move", "source object is uninitialised");
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other_window._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_hwnd = nullptr;
        this->_dc = nullptr;
        this->_ctx = nullptr;
        this->_close_flag = FT_FALSE;
        this->_width = 0;
        this->_height = 0;
        this->_mouse_x = 0;
        this->_mouse_y = 0;
        this->_primary_presses = 0U;
        this->_settings_key_pressed = FT_FALSE;
        this->_cursor_visible = FT_TRUE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_hwnd = other_window._hwnd;
    this->_dc = other_window._dc;
    this->_ctx = other_window._ctx;
    this->_close_flag = other_window._close_flag;
    this->_width = other_window._width;
    this->_height = other_window._height;
    this->_mouse_x = other_window._mouse_x;
    this->_mouse_y = other_window._mouse_y;
    this->_primary_presses = other_window._primary_presses;
    this->_settings_key_pressed = other_window._settings_key_pressed;
    this->_cursor_visible = other_window._cursor_visible;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other_window._hwnd = nullptr;
    other_window._dc = nullptr;
    other_window._ctx = nullptr;
    other_window._close_flag = FT_FALSE;
    other_window._width = 0;
    other_window._height = 0;
    other_window._mouse_x = 0;
    other_window._mouse_y = 0;
    other_window._primary_presses = 0U;
    other_window._settings_key_pressed = FT_FALSE;
    other_window._cursor_visible = FT_TRUE;
    other_window._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t ft_gpu_window_windows::primary_press_count() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_windows::primary_press_count");
    return (this->_primary_presses);
}

void ft_gpu_window_windows::poll_events() noexcept
{
    MSG msg;

    this->_primary_presses = 0U;
    this->_settings_key_pressed = FT_FALSE;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (msg.message == WM_QUIT)
            this->_close_flag = FT_TRUE;
    }
}

void ft_gpu_window_windows::swap_buffers() noexcept
{
    if (this->_dc != nullptr)
        SwapBuffers(this->_dc);
}

ft_gpu_window *ft_gpu_window::create() noexcept
{
    return (new (std::nothrow) ft_gpu_window_windows());
}

#endif

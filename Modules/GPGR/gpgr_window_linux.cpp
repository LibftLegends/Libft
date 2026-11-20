#if !defined(_WIN32) && !defined(__APPLE__)

#include "gpgr_window_linux.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"

typedef GLXContext (*glx_create_ctx_fn)(Display *, GLXFBConfig, GLXContext,
    Bool, const int *);

class ft_gpu_window_linux : public ft_gpu_window
{
    Display    *_display;
    Window      _window;
    GLXContext  _ctx;
    Atom        _wm_delete;
    uint8_t     _initialised_state;
    ft_bool     _close_flag;
    int32_t     _width;
    int32_t     _height;
    int32_t     _mouse_x;
    int32_t     _mouse_y;
    uint32_t    _primary_presses;
    ft_bool     _settings_key;
    ft_bool     _cursor_visible;
    ft_bool     _grab_active;
    Cursor      _invisible_cursor;

    public:
        ft_gpu_window_linux() noexcept
            : _display(nullptr), _window(0), _ctx(nullptr), _wm_delete(0),
              _initialised_state(FT_CLASS_STATE_UNINITIALISED),
              _close_flag(FT_FALSE), _width(0), _height(0), _mouse_x(0),
              _mouse_y(0), _primary_presses(0U),
              _settings_key(FT_FALSE), _cursor_visible(FT_TRUE),
              _grab_active(FT_FALSE), _invisible_cursor(0) {}
        ft_gpu_window_linux(const ft_gpu_window_linux &other) = delete;
        ft_gpu_window_linux(ft_gpu_window_linux &&other) = delete;
        ft_gpu_window_linux &operator=(const ft_gpu_window_linux &other)
            = delete;
        ft_gpu_window_linux &operator=(ft_gpu_window_linux &&other) = delete;
        ~ft_gpu_window_linux() override { (void)destroy(); }

        ft_bool initialize(const char *title, int32_t width, int32_t height,
            ft_bool fullscreen) noexcept override;
        int32_t destroy() noexcept override;
        void poll_events() noexcept override;
        void swap_buffers() noexcept override;
        ft_bool should_close() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::should_close");
            return (this->_close_flag);
        }
        int32_t get_width() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::get_width");
            return (this->_width);
        }
        int32_t get_height() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::get_height");
            return (this->_height);
        }
        int32_t get_mouse_x() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::get_mouse_x");
            return (this->_mouse_x);
        }
        int32_t get_mouse_y() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::get_mouse_y");
            return (this->_mouse_y);
        }
        uint32_t primary_press_count() const noexcept override;
        ft_bool was_mouse_clicked() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::was_mouse_clicked");
            return (this->_primary_presses != 0U);
        }
        void set_cursor_visible(ft_bool visible) noexcept override;
        ft_bool was_settings_key_pressed() const noexcept override
        {
            errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
                "ft_gpu_window_linux::was_settings_key_pressed");
            return (this->_settings_key);
        }
        int32_t move(ft_gpu_window &other) noexcept override;
};

ft_bool ft_gpu_window_linux::initialize(const char *title, int32_t width,
    int32_t height, ft_bool fullscreen) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_gpu_window_linux::initialize",
            "called while object is already initialised");
    this->_display = XOpenDisplay(nullptr);
    if (this->_display == nullptr)
        return (FT_FALSE);

    int32_t screen = DefaultScreen(this->_display);

    int32_t fb_attribs[] = {
        GLX_X_RENDERABLE,  True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_DEPTH_SIZE,    24,
        GLX_DOUBLEBUFFER,  True,
        None
    };

    int32_t fb_count = 0;
    GLXFBConfig *configs = glXChooseFBConfig(this->_display, screen, fb_attribs,
        &fb_count);
    if (configs == nullptr || fb_count == 0)
    {
        XCloseDisplay(this->_display);
        this->_display = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }
    GLXFBConfig fb = configs[0];
    XFree(configs);

    XVisualInfo *vi = glXGetVisualFromFBConfig(this->_display, fb);
    if (vi == nullptr)
    {
        XCloseDisplay(this->_display);
        this->_display = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    if (fullscreen == FT_TRUE)
    {
        this->_width = DisplayWidth(this->_display, screen);
        this->_height = DisplayHeight(this->_display, screen);
    }
    else
    {
        this->_width = width;
        this->_height = height;
    }

    XSetWindowAttributes swa;
    ft_memset(&swa, 0, sizeof(swa));
    swa.colormap   = XCreateColormap(this->_display, RootWindow(this->_display, vi->screen),
        vi->visual, AllocNone);
    swa.event_mask = ExposureMask | StructureNotifyMask
        | ButtonPressMask | ButtonReleaseMask
        | PointerMotionMask | KeyPressMask | KeyReleaseMask
        | FocusChangeMask;
    swa.border_pixel = 0;

    this->_window = XCreateWindow(this->_display,
        RootWindow(this->_display, vi->screen),
        0, 0,
        static_cast<unsigned int>(this->_width),
        static_cast<unsigned int>(this->_height),
        0, vi->depth, InputOutput, vi->visual,
        CWBorderPixel | CWColormap | CWEventMask, &swa);
    XFree(vi);

    if (this->_window == 0)
    {
        XCloseDisplay(this->_display);
        this->_display = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    XStoreName(this->_display, this->_window, title);
    this->_wm_delete = XInternAtom(this->_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(this->_display, this->_window, &this->_wm_delete, 1);

    if (fullscreen == FT_TRUE)
    {
        Atom wm_state = XInternAtom(this->_display, "_NET_WM_STATE", False);
        Atom wm_fs    = XInternAtom(this->_display, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(this->_display, this->_window, wm_state, XA_ATOM, 32,
            PropModeReplace,
            reinterpret_cast<unsigned char *>(&wm_fs), 1);
    }

    XMapWindow(this->_display, this->_window);
    XFlush(this->_display);

    char blank_data = 0;
    Pixmap blank_pm = XCreateBitmapFromData(this->_display, this->_window, &blank_data, 1, 1);
    XColor blank_col;
    ft_memset(&blank_col, 0, sizeof(blank_col));
    this->_invisible_cursor = XCreatePixmapCursor(this->_display, blank_pm, blank_pm,
        &blank_col, &blank_col, 0, 0);
    XFreePixmap(this->_display, blank_pm);

    glx_create_ctx_fn create_ctx =
        reinterpret_cast<glx_create_ctx_fn>(glXGetProcAddressARB(
            reinterpret_cast<const GLubyte *>("glXCreateContextAttribsARB")));

    if (create_ctx != nullptr)
    {
        int32_t ctx_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };
        this->_ctx = create_ctx(this->_display, fb, nullptr, True, ctx_attribs);
    }
    else
    {
        this->_ctx = glXCreateNewContext(this->_display, fb, GLX_RGBA_TYPE, nullptr, True);
    }

    if (this->_ctx == nullptr)
    {
        XDestroyWindow(this->_display, this->_window);
        XCloseDisplay(this->_display);
        this->_display = nullptr;
        this->_window  = 0;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    glXMakeCurrent(this->_display, this->_window, this->_ctx);

    if (gpgr_load_gl_functions() == FT_FALSE)
    {
        glXMakeCurrent(this->_display, None, nullptr);
        glXDestroyContext(this->_display, this->_ctx);
        XDestroyWindow(this->_display, this->_window);
        XCloseDisplay(this->_display);
        this->_display = nullptr;
        this->_window  = 0;
        this->_ctx     = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    ft_dumb_controls_linux_register_window(
        static_cast<void *>(this->_display),
        this->_window);

    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_TRUE);
}

int32_t ft_gpu_window_linux::destroy() noexcept
{
    if (this->_display == nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->_window != 0)
        ft_dumb_controls_linux_unregister_window(
            this->_window);
    if (this->_grab_active)
    {
        XUngrabPointer(this->_display, CurrentTime);
        this->_grab_active = FT_FALSE;
    }
    if (this->_invisible_cursor != 0)
    {
        XFreeCursor(this->_display, this->_invisible_cursor);
        this->_invisible_cursor = 0;
    }
    glXMakeCurrent(this->_display, None, nullptr);
    if (this->_ctx != nullptr)
    {
        glXDestroyContext(this->_display, this->_ctx);
        this->_ctx = nullptr;
    }
    if (this->_window != 0)
    {
        XDestroyWindow(this->_display, this->_window);
        this->_window = 0;
    }
    XCloseDisplay(this->_display);
    this->_display = nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_gpu_window_linux::move(ft_gpu_window &other) noexcept
{
    ft_gpu_window_linux &other_window =
        static_cast<ft_gpu_window_linux &>(other);

    if (this == &other_window)
        return (FT_ERR_SUCCESS);
    if (other_window._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other_window._initialised_state,
            "ft_gpu_window_linux::move", "source object is uninitialised");
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other_window._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_display = nullptr;
        this->_window = 0;
        this->_ctx = nullptr;
        this->_wm_delete = 0;
        this->_close_flag = FT_FALSE;
        this->_width = 0;
        this->_height = 0;
        this->_mouse_x = 0;
        this->_mouse_y = 0;
        this->_primary_presses = 0U;
        this->_settings_key = FT_FALSE;
        this->_cursor_visible = FT_TRUE;
        this->_grab_active = FT_FALSE;
        this->_invisible_cursor = 0;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_display = other_window._display;
    this->_window = other_window._window;
    this->_ctx = other_window._ctx;
    this->_wm_delete = other_window._wm_delete;
    this->_close_flag = other_window._close_flag;
    this->_width = other_window._width;
    this->_height = other_window._height;
    this->_mouse_x = other_window._mouse_x;
    this->_mouse_y = other_window._mouse_y;
    this->_primary_presses = other_window._primary_presses;
    this->_settings_key = other_window._settings_key;
    this->_cursor_visible = other_window._cursor_visible;
    this->_grab_active = other_window._grab_active;
    this->_invisible_cursor = other_window._invisible_cursor;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other_window._display = nullptr;
    other_window._window = 0;
    other_window._ctx = nullptr;
    other_window._wm_delete = 0;
    other_window._close_flag = FT_FALSE;
    other_window._width = 0;
    other_window._height = 0;
    other_window._mouse_x = 0;
    other_window._mouse_y = 0;
    other_window._primary_presses = 0U;
    other_window._settings_key = FT_FALSE;
    other_window._cursor_visible = FT_TRUE;
    other_window._grab_active = FT_FALSE;
    other_window._invisible_cursor = 0;
    other_window._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t ft_gpu_window_linux::primary_press_count() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_linux::primary_press_count");
    return (this->_primary_presses);
}

void ft_gpu_window_linux::poll_events() noexcept
{
    this->_primary_presses = 0U;
    this->_settings_key  = FT_FALSE;
    while (XPending(this->_display) > 0)
    {
        XEvent ev;
        XNextEvent(this->_display, &ev);
        if (ev.type == ClientMessage)
        {
            if (static_cast<Atom>(ev.xclient.data.l[0]) == this->_wm_delete)
                this->_close_flag = FT_TRUE;
        }
        else if (ev.type == ButtonPress && ev.xbutton.button == Button1)
        {
            if (this->_primary_presses < UINT32_MAX)
                this->_primary_presses += 1U;
        }
        else if (ev.type == MotionNotify)
        {
            this->_mouse_x = ev.xmotion.x;
            this->_mouse_y = ev.xmotion.y;
        }
        else if (ev.type == ButtonPress || ev.type == ButtonRelease
            || ev.type == MotionNotify)
        {
            this->_mouse_x = ev.xbutton.x;
            this->_mouse_y = ev.xbutton.y;
        }
        else if (ev.type == KeyPress)
        {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            if (sym == XK_m || sym == XK_M)
                this->_settings_key = FT_TRUE;
        }
        else if (ev.type == FocusOut)
        {
            if (this->_grab_active)
            {
                XUngrabPointer(this->_display, CurrentTime);
                this->_grab_active = FT_FALSE;
            }
        }
        else if (ev.type == FocusIn)
        {
            if (!this->_cursor_visible && this->_window != 0
                && this->_invisible_cursor != 0 && !this->_grab_active)
            {
                int32_t r = XGrabPointer(this->_display,
                    this->_window, False,
                    PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                    GrabModeAsync, GrabModeAsync,
                    this->_window, this->_invisible_cursor, CurrentTime);
                if (r == GrabSuccess)
                    this->_grab_active = FT_TRUE;
                else
                    this->_grab_active = FT_FALSE;
            }
        }
    }
}

void ft_gpu_window_linux::swap_buffers() noexcept
{
    glXSwapBuffers(this->_display, this->_window);
}

void ft_gpu_window_linux::set_cursor_visible(ft_bool visible) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_linux::set_cursor_visible");
    if (visible == FT_FALSE)
    {
        XDefineCursor(this->_display, this->_window, this->_invisible_cursor);
        XWarpPointer(this->_display, None, this->_window,
            0, 0, 0, 0, this->_width / 2, this->_height / 2);
        XFlush(this->_display);
        if (this->_grab_active == FT_FALSE)
        {
            int32_t r = XGrabPointer(this->_display, this->_window,
                False,
                PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync,
                this->_window, this->_invisible_cursor, CurrentTime);
            if (r == GrabSuccess)
                this->_grab_active = FT_TRUE;
            else
                this->_grab_active = FT_FALSE;
        }
    }
    else
    {
        if (this->_grab_active == FT_TRUE)
        {
            XUngrabPointer(this->_display, CurrentTime);
            this->_grab_active = FT_FALSE;
        }
        XUndefineCursor(this->_display, this->_window);
    }
    this->_cursor_visible = visible;
    XFlush(this->_display);
    return ;
}

ft_gpu_window *ft_gpu_window::create() noexcept
{
    return (new (std::nothrow) ft_gpu_window_linux());
}

#endif

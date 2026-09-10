#include "window_linux.hpp"

#include <cstdio>

namespace vre
{

WindowLinux::WindowLinux()
    : _display(nullptr), _window(0), _wm_delete_window(0), _width(0), _height(0),
      _close_requested(false), _resized(false)
{
}

WindowLinux::~WindowLinux()
{
    destroy();
}

bool WindowLinux::initialize(const char *title, int32_t width, int32_t height)
{
    _display = XOpenDisplay(nullptr);
    if (_display == nullptr)
    {
        std::fprintf(stderr, "WindowLinux: XOpenDisplay failed\n");
        return false;
    }

    int screen = DefaultScreen(_display);
    ::Window root = RootWindow(_display, screen);

    XSetWindowAttributes attributes;
    attributes.event_mask = ExposureMask | StructureNotifyMask
        | KeyPressMask | KeyReleaseMask
        | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    _width = width;
    _height = height;
    _window = XCreateWindow(_display, root, 0, 0,
        static_cast<unsigned int>(width), static_cast<unsigned int>(height),
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWEventMask, &attributes);

    if (_window == 0)
    {
        std::fprintf(stderr, "WindowLinux: XCreateWindow failed\n");
        XCloseDisplay(_display);
        _display = nullptr;
        return false;
    }

    XStoreName(_display, _window, title);

    _wm_delete_window = XInternAtom(_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(_display, _window, &_wm_delete_window, 1);

    XMapWindow(_display, _window);
    XFlush(_display);

    return true;
}

void WindowLinux::destroy()
{
    if (_display == nullptr)
        return;
    if (_window != 0)
    {
        XDestroyWindow(_display, _window);
        _window = 0;
    }
    XCloseDisplay(_display);
    _display = nullptr;
}

static bool keysym_to_keycode(KeySym sym, KeyCode *out_code)
{
    if (sym == XK_h || sym == XK_H)
    {
        *out_code = KeyCode::H;
        return true;
    }
    if (sym == XK_Escape)
    {
        *out_code = KeyCode::Escape;
        return true;
    }
    return false;
}

void WindowLinux::poll_events()
{
    for (bool &pressed : _key_pressed_this_poll)
        pressed = false;

    while (XPending(_display) > 0)
    {
        XEvent event;
        XNextEvent(_display, &event);

        if (event.type == KeyPress)
        {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            KeyCode code;
            if (keysym_to_keycode(sym, &code))
                _key_pressed_this_poll[static_cast<size_t>(code)] = true;
        }
        else if (event.type == ClientMessage)
        {
            if (static_cast<Atom>(event.xclient.data.l[0]) == _wm_delete_window)
                _close_requested = true;
        }
        else if (event.type == ConfigureNotify)
        {
            int32_t new_width = event.xconfigure.width;
            int32_t new_height = event.xconfigure.height;
            if (new_width != _width || new_height != _height)
            {
                _width = new_width;
                _height = new_height;
                _resized = true;
            }
        }
    }
}

void WindowLinux::get_required_instance_extensions(
    const char **out_extensions, uint32_t *out_count) const
{
    static const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };
    if (out_extensions != nullptr)
    {
        out_extensions[0] = extensions[0];
        out_extensions[1] = extensions[1];
    }
    if (out_count != nullptr)
        *out_count = 2;
}

VkResult WindowLinux::create_vulkan_surface(
    VkInstance instance, VkSurfaceKHR *out_surface) const
{
    VkXlibSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.dpy = _display;
    create_info.window = _window;
    return vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, out_surface);
}

} // namespace vre

vre::Window *vre::Window::create()
{
    return new vre::WindowLinux();
}

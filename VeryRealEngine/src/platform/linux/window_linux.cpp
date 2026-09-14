#include "window_linux.hpp"

namespace vre
{

WindowLinux::WindowLinux() : _display(nullptr), _window(0),
	_wm_delete_window(0), _width(0), _height(0), _close_requested(false),
	_resized(false)
{
	for (bool &pressed : _key_pressed_this_poll)
		pressed = false;
	for (bool &held : _key_held)
		held = false;
}

WindowLinux::~WindowLinux()
{
	destroy();
}

bool WindowLinux::should_close() const
{
	return (_close_requested);
}

int32_t WindowLinux::get_width() const
{
	return (_width);
}

int32_t WindowLinux::get_height() const
{
	return (_height);
}

bool WindowLinux::was_resized() const
{
	return (_resized);
}

void WindowLinux::clear_resized_flag()
{
	_resized = false;
}

bool WindowLinux::was_key_pressed(Key key) const
{
	return (_key_pressed_this_poll[static_cast<size_t>(key)]);
}

bool WindowLinux::is_key_held(Key key) const
{
	return (_key_held[static_cast<size_t>(key)]);
}

bool WindowLinux::initialize(const char *title, int32_t width, int32_t height)
{
	int						screen;
	XSetWindowAttributes	attributes;

	_display = XOpenDisplay(nullptr);
	if (_display == nullptr)
	{
		std::fprintf(stderr, "WindowLinux: XOpenDisplay failed\n");
		return (false);
	}
	screen = DefaultScreen(_display);
	::Window root = RootWindow(_display, screen);
	attributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	_width = width;
	_height = height;
	_window = XCreateWindow(_display, root, 0, 0,
			static_cast<unsigned int>(width), static_cast<unsigned int>(height),
			0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask,
			&attributes);
	if (_window == 0)
	{
		std::fprintf(stderr, "WindowLinux: XCreateWindow failed\n");
		XCloseDisplay(_display);
		_display = nullptr;
		return (false);
	}
	XStoreName(_display, _window, title);
	_wm_delete_window = XInternAtom(_display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(_display, _window, &_wm_delete_window, 1);
	XMapWindow(_display, _window);
	XFlush(_display);
	return (true);
}

void WindowLinux::destroy()
{
	if (_display == nullptr)
		return ;
	if (_window != 0)
	{
		XDestroyWindow(_display, _window);
		_window = 0;
	}
	XCloseDisplay(_display);
	_display = nullptr;
}

bool WindowLinux::keysym_to_key(KeySym sym, Key *out_key)
{
	switch (sym)
	{
	case XK_h:
	case XK_H:
		*out_key = Key::H;
		return (true);
	case XK_Escape:
		*out_key = Key::Escape;
		return (true);
	case XK_w:
	case XK_W:
		*out_key = Key::W;
		return (true);
	case XK_a:
	case XK_A:
		*out_key = Key::A;
		return (true);
	case XK_s:
	case XK_S:
		*out_key = Key::S;
		return (true);
	case XK_d:
	case XK_D:
		*out_key = Key::D;
		return (true);
	case XK_Left:
		*out_key = Key::Left;
		return (true);
	case XK_Right:
		*out_key = Key::Right;
		return (true);
	case XK_Up:
		*out_key = Key::Up;
		return (true);
	case XK_Down:
		*out_key = Key::Down;
		return (true);
	case XK_e:
	case XK_E:
		*out_key = Key::E;
		return (true);
	case XK_f:
	case XK_F:
		*out_key = Key::F;
		return (true);
	default:
		return (false);
	}
}

void WindowLinux::get_required_instance_extensions(const char **out_extensions,
	uint32_t *out_count) const
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

VkResult WindowLinux::create_vulkan_surface(VkInstance instance,
	VkSurfaceKHR *out_surface) const
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

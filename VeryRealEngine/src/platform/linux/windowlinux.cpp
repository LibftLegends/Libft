#include "windowlinux.hpp"

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

void WindowLinux::poll_events()
{
	XEvent	event;
	KeySym	sym;
	Key		key;
	bool	is_repeat;
	XEvent	next_event;
	int32_t	new_width;
	int32_t	new_height;

	for (bool &pressed : _key_pressed_this_poll)
		pressed = false;
	while (XPending(_display) > 0)
	{
		XNextEvent(_display, &event);
		if (event.type == KeyPress)
		{
			sym = XLookupKeysym(&event.xkey, 0);
			if (LinuxKeyMap::from_keysym(sym, &key))
			{
				_key_pressed_this_poll[static_cast<size_t>(key)] = true;
				_key_held[static_cast<size_t>(key)] = true;
			}
		}
		else if (event.type == KeyRelease)
		{
			// X11 auto-repeat sends a release immediately followed by a
			// press (same keycode, same timestamp) while a key is held
			// down — without filtering that out, is_key_held() would
			// flicker false between repeats instead of staying true.
			is_repeat = false;
			if (XEventsQueued(_display, QueuedAfterReading) > 0)
			{
				XPeekEvent(_display, &next_event);
				if (next_event.type == KeyPress
					&& next_event.xkey.keycode == event.xkey.keycode
					&& next_event.xkey.time == event.xkey.time)
					is_repeat = true;
			}
			if (!is_repeat)
			{
				sym = XLookupKeysym(&event.xkey, 0);
				if (LinuxKeyMap::from_keysym(sym, &key))
					_key_held[static_cast<size_t>(key)] = false;
			}
		}
		else if (event.type == ClientMessage)
		{
			if (static_cast<Atom>(event.xclient.data.l[0])
				== _wm_delete_window)
				_close_requested = true;
		}
		else if (event.type == ConfigureNotify)
		{
			new_width = event.xconfigure.width;
			new_height = event.xconfigure.height;
			if (new_width != _width || new_height != _height)
			{
				_width = new_width;
				_height = new_height;
				_resized = true;
			}
		}
	}
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
	attributes.event_mask = ExposureMask | StructureNotifyMask
		| KeyPressMask | KeyReleaseMask | ButtonPressMask
		| ButtonReleaseMask | PointerMotionMask;
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

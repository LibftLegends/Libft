/**
 * @file window_linux.hpp
 * @brief Linux windowing backend: plain Xlib, same choice GPGR's Linux
 * backend made (Modules/GPGR/gpgr_window_linux.cpp), so window/event-loop
 * shape is familiar — but this backend exposes a VkSurfaceKHR via
 * VK_KHR_xlib_surface instead of creating a GLX context.
 */
#pragma once

#include "../../vre.hpp"
#include "../window.hpp"

namespace vre
{

/// Xlib + VK_KHR_xlib_surface implementation of Window.
class WindowLinux : public Window
{
  public:
	WindowLinux();
	~WindowLinux() override;

	bool initialize(const char *title, int32_t width, int32_t height) override;
	void destroy() override;

	void poll_events() override;
	bool should_close() const override;

	int32_t get_width() const override;
	int32_t get_height() const override;
	bool was_resized() const override;
	void clear_resized_flag() override;

	bool was_key_pressed(Key key) const override;
	bool is_key_held(Key key) const override;

	void get_required_instance_extensions(const char **out_extensions,
		uint32_t *out_count) const override;

	VkResult create_vulkan_surface(VkInstance instance,
		VkSurfaceKHR *out_surface) const override;

  private:
	// Owns an X11 Display connection and window handle — copying would
	// leave two objects owning the same OS resource. The pre-C++11
	// idiom of a private, never-defined copy constructor/assignment
	// operator (this project avoids `= delete`).
	WindowLinux(const WindowLinux &other);
	WindowLinux &operator=(const WindowLinux &other);

	static bool keysym_to_key(KeySym sym, Key *out_key);

	Display *_display;
	::Window _window;
	Atom _wm_delete_window;
	int32_t _width;
	int32_t _height;
	bool _close_requested;
	bool _resized;
	bool _key_pressed_this_poll[static_cast<size_t>(Key::Count)];
	bool _key_held[static_cast<size_t>(Key::Count)];
};

} // namespace vre

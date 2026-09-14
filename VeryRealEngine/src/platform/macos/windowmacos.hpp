/**
 * @file windowmacos.hpp
 * @brief macOS windowing backend: Cocoa (NSWindow/NSView) for the window
 * and event pump, VK_EXT_metal_surface for the VkSurfaceKHR — same shape
 * as WindowLinux (Xlib + VK_KHR_xlib_surface), just a different native
 * window system and surface extension.
 *
 * Native Cocoa/Metal types are kept out of this header (as untyped
 * pointers) so it can be included from plain C++ translation units
 * without pulling in Objective-C; the actual Cocoa/Metal calls live in
 * windowmacos.mm.
 */
#pragma once

#include "../../vre.hpp"
#include "../window.hpp"
#include "macoskeymap.hpp"

namespace vre
{
/// Cocoa + VK_EXT_metal_surface (via MoltenVK) implementation of Window.
class WindowMacOS : public Window
{
  public:
	WindowMacOS();
	~WindowMacOS() override;

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

	/// Called from the Objective-C++ window delegate (VREWindowDelegate)
	/// when the close button is pressed.
	void on_close_requested();
	/**
		* @brief Called from the Objective-C++ window delegate
		* (VREWindowDelegate) when the window is resized.
		* @param width New width, in pixels.
		* @param height New height, in pixels.
		*/
	void on_resized(int32_t width, int32_t height);

  private:
	// Owns Cocoa/Metal objects (via untyped pointers) — copying would
	// leave two objects owning the same native window. The pre-C++11
	// idiom of a private, never-defined copy constructor/assignment
	// operator (this project avoids `= delete`).
	WindowMacOS(const WindowMacOS &other);
	WindowMacOS &operator=(const WindowMacOS &other);

	void *_ns_window;   ///< NSWindow*
	void *_metal_layer; ///< CAMetalLayer*
	void *_delegate;    ///< Internal NSWindowDelegate subclass instance.
	int32_t _width;
	int32_t _height;
	bool _close_requested;
	bool _resized;
	bool _key_pressed_this_poll[static_cast<size_t>(Key::Count)];
	bool _key_held[static_cast<size_t>(Key::Count)];
};

} // namespace vre

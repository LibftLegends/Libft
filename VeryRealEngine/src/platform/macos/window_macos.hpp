/**
 * @file window_macos.hpp
 * @brief macOS windowing backend: Cocoa (NSWindow/NSView) for the window
 * and event pump, VK_EXT_metal_surface for the VkSurfaceKHR — same shape
 * as WindowLinux (Xlib + VK_KHR_xlib_surface), just a different native
 * window system and surface extension.
 *
 * Native Cocoa/Metal types are kept out of this header (as untyped
 * pointers) so it can be included from plain C++ translation units
 * (renderer.cpp) without pulling in Objective-C; the actual Cocoa/Metal
 * calls live in window_macos.mm.
 */
#pragma once

#include "../window.hpp"

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
        bool should_close() const override { return _close_requested; }

        int32_t get_width() const override { return _width; }
        int32_t get_height() const override { return _height; }
        bool was_resized() const override { return _resized; }
        void clear_resized_flag() override { _resized = false; }

        bool was_key_pressed(KeyCode key) const override
        {
            return _key_pressed_this_poll[static_cast<size_t>(key)];
        }
        bool is_key_held(KeyCode key) const override
        {
            return _key_held[static_cast<size_t>(key)];
        }

        void get_required_instance_extensions(
            const char **out_extensions, uint32_t *out_count) const override;

        VkResult create_vulkan_surface(
            VkInstance instance, VkSurfaceKHR *out_surface) const override;

        /// Called from the Objective-C++ window delegate in window_macos.mm when the close button is pressed.
        /// Not part of the abstract Window interface.
        void on_close_requested() { _close_requested = true; }
        /// Called from the Objective-C++ window delegate in window_macos.mm when the window is resized.
        /// @param width New width, in pixels.
        /// @param height New height, in pixels.
        void on_resized(int32_t width, int32_t height);

    private:
        void *_ns_window;    ///< NSWindow*
        void *_metal_layer;  ///< CAMetalLayer*
        void *_delegate;     ///< Internal NSWindowDelegate subclass instance.
        int32_t _width;
        int32_t _height;
        bool _close_requested;
        bool _resized;
        bool _key_pressed_this_poll[static_cast<size_t>(KeyCode::Count)] = {};
        bool _key_held[static_cast<size_t>(KeyCode::Count)] = {};
};

} // namespace vre

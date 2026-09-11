/**
 * @file window_linux.hpp
 * @brief Linux windowing backend: plain Xlib, same choice GPGR's Linux
 * backend made (Modules/GPGR/gpgr_window_linux.cpp), so window/event-loop
 * shape is familiar — but this backend exposes a VkSurfaceKHR via
 * VK_KHR_xlib_surface instead of creating a GLX context.
 */
#pragma once

#include "../window.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <vulkan/vulkan_xlib.h>

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

    private:
        Display *_display;
        ::Window _window;
        Atom _wm_delete_window;
        int32_t _width;
        int32_t _height;
        bool _close_requested;
        bool _resized;
        bool _key_pressed_this_poll[static_cast<size_t>(KeyCode::Count)] = {};
        bool _key_held[static_cast<size_t>(KeyCode::Count)] = {};
};

} // namespace vre

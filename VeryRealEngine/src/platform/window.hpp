// Abstract platform window interface.
//
// The per-OS window-creation *approach* here follows the same shape as
// FullLibft's Modules/GPGR (one Xlib/Win32/Cocoa backend behind a common
// interface, see ../../verdict.md for why the GPGR code itself isn't reused
// verbatim): open a native window, pump its event queue, report size/input,
// and — the one thing that differs from GPGR, which set up an OpenGL
// context — hand back a VkSurfaceKHR for Vulkan to render into instead of
// swapping GL buffers.
#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vre
{

// Small, deliberately short list: just what the current demo needs to
// drive (toggling scene-node visibility). Extend as later steps need more.
enum class KeyCode
{
    H,
    Escape,
    Count,
};

class Window
{
    public:
        virtual ~Window() = default;

        virtual bool initialize(const char *title, int32_t width, int32_t height) = 0;
        virtual void destroy() = 0;

        virtual void poll_events() = 0;
        virtual bool should_close() const = 0;

        virtual int32_t get_width() const = 0;
        virtual int32_t get_height() const = 0;
        // True exactly once, the poll_events() call after a resize is observed.
        virtual bool was_resized() const = 0;
        virtual void clear_resized_flag() = 0;

        // Edge-triggered: true only on the poll_events() call where the key
        // transitioned from up to down (a held key doesn't repeat this).
        virtual bool was_key_pressed(KeyCode key) const = 0;

        // Instance extensions this backend needs for VK_KHR_surface creation
        // (e.g. VK_KHR_xlib_surface on Linux).
        virtual void get_required_instance_extensions(
            const char **out_extensions, uint32_t *out_count) const = 0;

        virtual VkResult create_vulkan_surface(
            VkInstance instance, VkSurfaceKHR *out_surface) const = 0;

        static Window *create();
};

} // namespace vre

/**
 * @file window.hpp
 * @brief Abstract platform window interface.
 *
 * The per-OS window-creation *approach* here follows the same shape as
 * FullLibft's Modules/GPGR (one Xlib/Win32/Cocoa backend behind a common
 * interface, see ../../verdict.md for why the GPGR code itself isn't reused
 * verbatim): open a native window, pump its event queue, report size/input,
 * and — the one thing that differs from GPGR, which set up an OpenGL
 * context — hand back a VkSurfaceKHR for Vulkan to render into instead of
 * swapping GL buffers.
 */
#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vre
{

/**
 * @brief Keys this engine's demo cares about.
 *
 * Small, deliberately short list: just what the current demo needs to
 * drive (visibility toggling, first-person navigation, interaction).
 * Extend as later steps need more.
 */
enum class KeyCode
{
    H,
    Escape,
    W,
    A,
    S,
    D,
    Left,
    Right,
    Up,
    Down,
    E,
    F,
    Count, ///< Not a real key — array-sizing sentinel for per-key state tables.
};

/// Abstract per-OS window: native window/event-pump plus enough Vulkan glue to create a VkSurfaceKHR.
class Window
{
    public:
        virtual ~Window() = default;

        /**
         * @brief Creates the native window.
         * @param title Window title.
         * @param width Initial width, in pixels.
         * @param height Initial height, in pixels.
         * @return true on success.
         */
        virtual bool initialize(const char *title, int32_t width, int32_t height) = 0;
        /// Destroys the native window and releases any OS resources.
        virtual void destroy() = 0;

        /// Pumps the native event queue, updating resize/key/close state for this poll.
        virtual void poll_events() = 0;
        /// @return true once the user has requested the window be closed.
        virtual bool should_close() const = 0;

        /// @return Current window width, in pixels.
        virtual int32_t get_width() const = 0;
        /// @return Current window height, in pixels.
        virtual int32_t get_height() const = 0;
        /// @return true exactly once, the poll_events() call after a resize is observed.
        virtual bool was_resized() const = 0;
        /// Clears the one-shot flag returned by was_resized().
        virtual void clear_resized_flag() = 0;

        /**
         * @brief Edge-triggered key check.
         * @return true only on the poll_events() call where `key` transitioned
         * from up to down (a held key doesn't repeat this).
         */
        virtual bool was_key_pressed(KeyCode key) const = 0;

        /**
         * @brief Level-triggered key check.
         *
         * Used for continuous movement (WASD/arrow-key navigation), where
         * an edge-triggered "was pressed" would only move one step.
         * @return true for as long as `key` is physically held down.
         */
        virtual bool is_key_held(KeyCode key) const = 0;

        /**
         * @brief Reports the Vulkan instance extensions this backend needs
         * for VK_KHR_surface creation (e.g. VK_KHR_xlib_surface on Linux).
         * @param out_extensions Receives pointers to extension name strings.
         * @param out_count Receives the number of extensions written.
         */
        virtual void get_required_instance_extensions(
            const char **out_extensions, uint32_t *out_count) const = 0;

        /**
         * @brief Creates a VkSurfaceKHR targeting this window.
         * @param instance Vulkan instance to create the surface under.
         * @param out_surface Receives the created surface on success.
         * @return The Vulkan result of surface creation.
         */
        virtual VkResult create_vulkan_surface(
            VkInstance instance, VkSurfaceKHR *out_surface) const = 0;

        /// @return A newly constructed window backend appropriate for the current platform.
        static Window *create();
};

} // namespace vre

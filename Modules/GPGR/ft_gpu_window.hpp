#ifndef GPGR_FT_GPU_WINDOW_HPP
# define GPGR_FT_GPU_WINDOW_HPP

#include "../Basic/basic.hpp"

class ft_gpu_window
{
    public:
        ft_gpu_window() noexcept;
        ft_gpu_window(const ft_gpu_window &other) = delete;
        ft_gpu_window(ft_gpu_window &&other) = delete;
        ft_gpu_window &operator=(const ft_gpu_window &other) = delete;
        ft_gpu_window &operator=(ft_gpu_window &&other) = delete;
        virtual ~ft_gpu_window() noexcept;


        virtual ft_bool initialize(const char *title, int32_t width,
            int32_t height, ft_bool fullscreen) noexcept = 0;
        virtual int32_t destroy() noexcept = 0;
        virtual int32_t move(ft_gpu_window &other) noexcept = 0;
        virtual void poll_events() noexcept = 0;
        virtual void swap_buffers() noexcept = 0;
        virtual ft_bool should_close() const noexcept = 0;
        virtual int32_t get_width() const noexcept = 0;
        virtual int32_t get_height() const noexcept = 0;

        virtual int32_t get_mouse_x() const noexcept = 0;
        virtual int32_t get_mouse_y() const noexcept = 0;
        virtual ft_bool was_mouse_clicked() const noexcept = 0;
        virtual void set_cursor_visible(ft_bool visible) noexcept = 0;
        virtual ft_bool was_settings_key_pressed() const noexcept = 0;

        /* Primary button presses collected by the most recent event poll. */
        virtual uint32_t primary_press_count() const noexcept;

        static ft_gpu_window *create() noexcept;
};

#endif

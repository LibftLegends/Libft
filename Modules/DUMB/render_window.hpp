#pragma once

#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"

enum ft_render_window_flags
{
    FT_RENDER_WINDOW_FLAG_NONE = 0,
    FT_RENDER_WINDOW_FLAG_RESIZABLE = 1 << 0,
    FT_RENDER_WINDOW_FLAG_FULLSCREEN = 1 << 1
};

struct ft_render_window_desc
{
    int32_t         width;
    int32_t         height;
    const char      *title;
    uint32_t        flags;
};

struct ft_render_framebuffer
{
    int32_t         width;
    int32_t         height;
    uint32_t        *pixels;
};

struct ft_render_depth_buffer
{
    int32_t         width;
    int32_t         height;
    uint8_t         *values;
};

struct ft_render_shader_input
{
    int32_t         coordinate_x;
    int32_t         coordinate_y;
    int32_t         width;
    int32_t         height;
    uint32_t        current_color;
    uint8_t         current_depth;
    void            *user_data;
};

struct ft_render_shader_output
{
    uint32_t        color;
    uint8_t         depth;
    ft_bool         write_depth;
};

typedef int32_t (*ft_render_fragment_shader)(
    const ft_render_shader_input *input,
    ft_render_shader_output *output
);

struct ft_render_screen_size
{
    int32_t         width;
    int32_t         height;
};

ft_render_screen_size  ft_render_get_primary_screen_size(void);

class ft_render_window
{
 #ifdef LIBFT_TEST_BUILD
    public:
 #else
    private:
 #endif

        pt_recursive_mutex           *_mutex = ft_nullptr;
        uint8_t                      _initialised_state = FT_CLASS_STATE_UNINITIALISED;

        ft_render_framebuffer        _framebuffer;
        ft_render_depth_buffer       _depth_buffer;
        ft_bool                      _is_initialised = FT_FALSE;
        ft_bool                      _should_close = FT_FALSE;
        ft_bool                      _shade_active = FT_FALSE;

        void                         *_platform_state = ft_nullptr;
        void                         reset_render_window_runtime(void) noexcept;
        int32_t                      prepare_thread_safety(void) noexcept;
        uint32_t                     teardown_thread_safety(void) noexcept;

    public:
        ft_render_window(void);
        ft_render_window(const ft_render_window &other) = delete;
        ft_render_window(ft_render_window &&other) = delete;
        ft_render_window &operator=(const ft_render_window &other) = delete;
        ft_render_window &operator=(ft_render_window &&other) = delete;
        ~ft_render_window(void);

        int32_t                     initialize(void);
        int32_t                     initialize(const ft_render_window_desc &desc);
        int32_t                     initialize(const ft_render_window &other);
        int32_t                     initialize(ft_render_window &&other);
        int32_t                      destroy(void);
        int32_t                      move(ft_render_window &other);
        void                         shutdown(void);

        int32_t                      poll_events(void);
        int32_t                      present(void);

        ft_render_framebuffer        &framebuffer(void);
        ft_render_depth_buffer       &depth_buffer(void);
        int32_t                      clear(uint32_t color);
        int32_t                      put_pixel(int32_t coordinate_x,
            int32_t coordinate_y, uint32_t color);
        int32_t                      shade(ft_render_fragment_shader shader,
            void *user_data);

        int32_t                      set_fullscreen(ft_bool enabled);

        uint32_t                     enable_thread_safety() noexcept;
        uint32_t                     disable_thread_safety() noexcept;
        ft_bool                      is_thread_safe() const noexcept;

        ft_bool                      should_close(void) const;
};

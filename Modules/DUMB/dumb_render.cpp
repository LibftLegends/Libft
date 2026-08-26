#include "dumb_render_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t create_recursive_mutex(pt_recursive_mutex **mutex_pointer)
{
    pt_recursive_mutex *created_mutex;
    int32_t initialize_error;

    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    created_mutex = new (std::nothrow) pt_recursive_mutex();
    if (created_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = created_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete created_mutex;
        return (initialize_error);
    }
    *mutex_pointer = created_mutex;
    return (FT_ERR_SUCCESS);
}

static uint32_t destroy_recursive_mutex(pt_recursive_mutex **mutex_pointer)
{
    uint32_t destroy_error;

    if (mutex_pointer == ft_nullptr || *mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = (*mutex_pointer)->destroy();
    delete *mutex_pointer;
    *mutex_pointer = ft_nullptr;
    return (destroy_error);
}

static int32_t lock_ordered_mutexes(pt_recursive_mutex *mutex_left,
    pt_recursive_mutex *mutex_right, pt_recursive_mutex **first_mutex,
    pt_recursive_mutex **second_mutex)
{
    int32_t lock_error;

    if (reinterpret_cast<uintptr_t>(mutex_left)
        <= reinterpret_cast<uintptr_t>(mutex_right))
    {
        *first_mutex = mutex_left;
        *second_mutex = mutex_right;
    }
    else
    {
        *first_mutex = mutex_right;
        *second_mutex = mutex_left;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(*first_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (*second_mutex == *first_mutex)
        return (FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(*second_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(*first_mutex);
        return (lock_error);
    }
    return (FT_ERR_SUCCESS);
}

static void unlock_ordered_mutexes(pt_recursive_mutex *first_mutex,
    pt_recursive_mutex *second_mutex)
{
    if (second_mutex != first_mutex)
        (void)pt_recursive_mutex_unlock_if_not_null(second_mutex);
    (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
    return ;
}

static int32_t create_depth_buffer(ft_render_depth_buffer *depth_buffer,
    int32_t width, int32_t height)
{
    ft_size_t pixel_count;

    if (depth_buffer == ft_nullptr || width <= 0 || height <= 0)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    depth_buffer->width = 0;
    depth_buffer->height = 0;
    depth_buffer->values = ft_nullptr;
    pixel_count = static_cast<ft_size_t>(width) * static_cast<ft_size_t>(height);
    depth_buffer->values = new (std::nothrow) uint8_t[pixel_count];
    if (depth_buffer->values == ft_nullptr)
    {
        return (FT_ERR_NO_MEMORY);
    }
    ft_memset(depth_buffer->values, 0, pixel_count);
    depth_buffer->width = width;
    depth_buffer->height = height;
    return (FT_ERR_SUCCESS);
}

static void destroy_depth_buffer(ft_render_depth_buffer *depth_buffer)
{
    if (depth_buffer == ft_nullptr)
    {
        return ;
    }
    if (depth_buffer->values != ft_nullptr)
    {
        delete[] depth_buffer->values;
        depth_buffer->values = ft_nullptr;
    }
    depth_buffer->width = 0;
    depth_buffer->height = 0;
    return ;
}

ft_render_screen_size ft_render_get_primary_screen_size(void)
{
    ft_render_platform_result  platform_result;
    ft_render_screen_size      size;

    size.width = 0;
    size.height = 0;

    platform_result = ft_render_platform_get_primary_screen_size(&size);
    if (platform_result.error_code != FT_ERR_SUCCESS)
        return (size);
    
    return (size);
}

ft_render_window::ft_render_window(void)
    : _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _framebuffer(), _depth_buffer(), _is_initialised(FT_FALSE),
      _should_close(FT_FALSE), _platform_state(ft_nullptr)
{
    this->reset_render_window_runtime();
    return ;
}

ft_render_window::~ft_render_window(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    this->reset_render_window_runtime();
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

int32_t ft_render_window::initialize(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_render_window::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_framebuffer.width = 0;
    this->_framebuffer.height = 0;
    this->_framebuffer.pixels = ft_nullptr;
    this->_depth_buffer.width = 0;
    this->_depth_buffer.height = 0;
    this->_depth_buffer.values = ft_nullptr;
    this->_is_initialised = FT_FALSE;
    this->_should_close = FT_FALSE;
    this->_platform_state = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::initialize(const ft_render_window &other)
{
    uint32_t destroy_error;
    int32_t lock_error;
    pt_recursive_mutex *first_mutex;
    pt_recursive_mutex *second_mutex;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_render_window::initialize(const ft_render_window &) source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
        }
        this->_framebuffer.width = 0;
        this->_framebuffer.height = 0;
        this->_framebuffer.pixels = ft_nullptr;
        destroy_depth_buffer(&this->_depth_buffer);
        this->_is_initialised = FT_FALSE;
        this->_should_close = FT_FALSE;
        this->_platform_state = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_render_window::initialize(const ft_render_window &) source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = lock_ordered_mutexes(this->_mutex, other._mutex,
        &first_mutex, &second_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    if (other._is_initialised == FT_TRUE)
    {
        unlock_ordered_mutexes(first_mutex, second_mutex);
        (void)this->destroy();
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_should_close = other._should_close;
    unlock_ordered_mutexes(first_mutex, second_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::initialize(ft_render_window &&other)
{
    uint32_t initialization_error;
    uint32_t move_error;
    uint32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_render_window::initialize(ft_render_window &&) source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
        }
        this->_framebuffer.width = 0;
        this->_framebuffer.height = 0;
        this->_framebuffer.pixels = ft_nullptr;
        destroy_depth_buffer(&this->_depth_buffer);
        this->_is_initialised = FT_FALSE;
        this->_should_close = FT_FALSE;
        this->_platform_state = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_render_window::initialize(ft_render_window &&) source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    initialization_error = this->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
        return (initialization_error);
    move_error = this->move(other);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (move_error);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::destroy(void)
{
    uint32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (this->_is_initialised == FT_TRUE)
        this->shutdown();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t ft_render_window::move(ft_render_window &other)
{
    int32_t lock_error;
    int32_t destroy_error;
    pt_recursive_mutex *first_mutex;
    pt_recursive_mutex *second_mutex;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_render_window::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
            return (this->destroy());
        this->_framebuffer.width = 0;
        this->_framebuffer.height = 0;
        this->_framebuffer.pixels = ft_nullptr;
        destroy_depth_buffer(&this->_depth_buffer);
        this->_is_initialised = FT_FALSE;
        this->_should_close = FT_FALSE;
        this->_platform_state = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_render_window::move source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        if (this->initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    lock_error = lock_ordered_mutexes(this->_mutex, other._mutex,
        &first_mutex, &second_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    this->_framebuffer = other._framebuffer;
    this->_depth_buffer = other._depth_buffer;
    this->_is_initialised = other._is_initialised;
    this->_should_close = other._should_close;
    this->_platform_state = other._platform_state;
    other._framebuffer.width = 0;
    other._framebuffer.height = 0;
    other._framebuffer.pixels = ft_nullptr;
    other._depth_buffer.width = 0;
    other._depth_buffer.height = 0;
    other._depth_buffer.values = ft_nullptr;
    other._is_initialised = FT_FALSE;
    other._should_close = FT_FALSE;
    other._platform_state = ft_nullptr;
    unlock_ordered_mutexes(first_mutex, second_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::initialize(const ft_render_window_desc &desc)
{
    int32_t                   lock_status;
    ft_render_platform_result platform_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::initialize(desc)");
    lock_status = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_status != FT_ERR_SUCCESS)
        return (lock_status);
    if (this->_is_initialised == FT_TRUE)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_render_window::initialize(desc)",
            "called while render window is already initialised");
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (desc.width <= 0 || desc.height <= 0 || desc.title == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    platform_result = ft_render_platform_create_window(
        &this->_platform_state,
        &this->_framebuffer,
        desc
    );
    if (platform_result.error_code != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        (void)destroy_recursive_mutex(&this->_mutex);
        this->reset_render_window_runtime();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (platform_result.error_code);
    }
    if (create_depth_buffer(&this->_depth_buffer, this->_framebuffer.width,
            this->_framebuffer.height) != FT_ERR_SUCCESS)
    {
        (void)ft_render_platform_destroy_window(
            &this->_platform_state,
            &this->_framebuffer
        );
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        (void)destroy_recursive_mutex(&this->_mutex);
        this->reset_render_window_runtime();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_is_initialised = FT_TRUE;
    this->_should_close = FT_FALSE;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_render_window::shutdown(void)
{
    int32_t                    lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::shutdown");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (this->_is_initialised != FT_FALSE)
    {
        (void)ft_render_platform_destroy_window(
            &this->_platform_state,
            &this->_framebuffer
        );

        this->_platform_state = ft_nullptr;
        this->_framebuffer.width = 0;
        this->_framebuffer.height = 0;
        this->_framebuffer.pixels = ft_nullptr;
        destroy_depth_buffer(&this->_depth_buffer);

        this->_is_initialised = FT_FALSE;
        this->_should_close = FT_TRUE;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_render_window::poll_events(void)
{
    ft_render_platform_result  platform_result;
    int32_t                    lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::poll_events");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_is_initialised == FT_FALSE)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    platform_result = ft_render_platform_poll_events(
        this->_platform_state,
        &this->_should_close
    );
    if (platform_result.error_code != FT_ERR_SUCCESS)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (platform_result.error_code);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::present(void)
{
    ft_render_platform_result  platform_result;
    int32_t                    lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::present");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_is_initialised == FT_FALSE)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    platform_result = ft_render_platform_present(
        this->_platform_state,
        &this->_framebuffer,
        &this->_depth_buffer
    );
    if (platform_result.error_code != FT_ERR_SUCCESS)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (platform_result.error_code);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

ft_render_framebuffer &ft_render_window::framebuffer(void)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::framebuffer");
    return (this->_framebuffer);
}

ft_render_depth_buffer &ft_render_window::depth_buffer(void)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::depth_buffer");
    return (this->_depth_buffer);
}

int32_t ft_render_window::clear(uint32_t color)
{
    int32_t                      index_width;
    int32_t                      index_height;
    int32_t                      width;
    int32_t                      height;
    uint32_t                     *pixels;
    int32_t                      index;
    int32_t                      lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::clear");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    if (this->_is_initialised == FT_FALSE)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    width = this->_framebuffer.width;
    height = this->_framebuffer.height;
    pixels = this->_framebuffer.pixels;

    index_height = 0;
    while (index_height < height)
    {
        index_width = 0;
        while (index_width < width)
        {
            index = (index_height * width) + index_width;
            pixels[index] = color;
            index_width = index_width + 1;
        }
        index_height = index_height + 1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::put_pixel(int32_t coordinate_x, int32_t coordinate_y,
    uint32_t color)
{
    int32_t lock_error;
    int32_t width;
    int32_t height;
    int32_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::put_pixel");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_is_initialised == FT_FALSE)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    width = this->_framebuffer.width;
    height = this->_framebuffer.height;
    if (coordinate_x < 0 || coordinate_y < 0
        || coordinate_x >= width || coordinate_y >= height)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    index = (coordinate_y * width) + coordinate_x;
    this->_framebuffer.pixels[index] = color;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::shade(ft_render_fragment_shader shader,
    void *user_data)
{
    ft_render_shader_input       input;
    ft_render_shader_output      output;
    int32_t                      index_width;
    int32_t                      index_height;
    int32_t                      index;
    int32_t                      shader_error;
    int32_t                      lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::shade");
    if (shader == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_is_initialised == FT_FALSE)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    if (this->_framebuffer.pixels == ft_nullptr
        || this->_depth_buffer.values == ft_nullptr
        || this->_framebuffer.width != this->_depth_buffer.width
        || this->_framebuffer.height != this->_depth_buffer.height)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    input.width = this->_framebuffer.width;
    input.height = this->_framebuffer.height;
    input.user_data = user_data;
    index_height = 0;
    while (index_height < this->_framebuffer.height)
    {
        index_width = 0;
        while (index_width < this->_framebuffer.width)
        {
            index = (index_height * this->_framebuffer.width) + index_width;
            input.coordinate_x = index_width;
            input.coordinate_y = index_height;
            input.current_color = this->_framebuffer.pixels[index];
            input.current_depth = this->_depth_buffer.values[index];
            output.color = input.current_color;
            output.depth = input.current_depth;
            output.write_depth = FT_FALSE;
            shader_error = shader(&input, &output);
            if (shader_error != FT_ERR_SUCCESS)
            {
                (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
                return (shader_error);
            }
            this->_framebuffer.pixels[index] = output.color;
            if (output.write_depth == FT_TRUE)
                this->_depth_buffer.values[index] = output.depth;
            index_width = index_width + 1;
        }
        index_height = index_height + 1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_render_window::set_fullscreen(ft_bool enabled)
{
    ft_render_platform_result  platform_result;
    int32_t                    lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::set_fullscreen");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_is_initialised == FT_FALSE)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_NOT_INITIALISED);
    }
    platform_result = ft_render_platform_set_fullscreen(
        this->_platform_state,
        enabled
    );
    if (platform_result.error_code != FT_ERR_SUCCESS)
    {
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (platform_result.error_code);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_render_window::should_close(void) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::should_close");
    return (this->_should_close);
}

void ft_render_window::reset_render_window_runtime(void) noexcept
{
    this->_framebuffer.width = 0;
    this->_framebuffer.height = 0;
    this->_framebuffer.pixels = ft_nullptr;
    destroy_depth_buffer(&this->_depth_buffer);
    this->_is_initialised = FT_FALSE;
    this->_should_close = FT_FALSE;
    this->_platform_state = ft_nullptr;
    return ;
}

int32_t ft_render_window::prepare_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::prepare_thread_safety");
    if (this->_mutex)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer = ft_nullptr;
    int32_t mutex_error;

    mutex_error = create_recursive_mutex(&mutex_pointer);
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

uint32_t ft_render_window::teardown_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::teardown_thread_safety");
    return (destroy_recursive_mutex(&this->_mutex));
}

uint32_t ft_render_window::enable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::enable_thread_safety");
    return (this->prepare_thread_safety());
}

uint32_t ft_render_window::disable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::disable_thread_safety");
    return (this->teardown_thread_safety());
}

ft_bool ft_render_window::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_render_window::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

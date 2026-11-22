#include "time.hpp"
#include "time_fps.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"

#include <chrono>
#include <new>
#include "../PThread/mutex.hpp"

time_fps::time_fps(void) noexcept
{
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    this->_frame_duration_ms = 0.0;
    this->_frames_per_second = static_cast<int64_t>(0);
    this->_last_frame_time = std::chrono::steady_clock::time_point();
    return ;
}

time_fps::~time_fps() noexcept
{
    (void)this->destroy();
    this->_mutex = ft_nullptr;
    this->_frame_duration_ms = 0.0;
    this->_frames_per_second = static_cast<int64_t>(0);
    this->_last_frame_time = std::chrono::steady_clock::time_point();
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

int32_t time_fps::initialize(void) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "time_fps::initialize", "already initialised");
    this->_frame_duration_ms = 0.0;
    this->_frames_per_second = static_cast<int64_t>(0);
    this->_last_frame_time = std::chrono::steady_clock::now();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t time_fps::initialize(const time_fps &other) noexcept
{
    int32_t destroy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_fps::initialize(copy)", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_frame_duration_ms = other._frame_duration_ms;
    this->_frames_per_second = other._frames_per_second;
    this->_last_frame_time = other._last_frame_time;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t time_fps::initialize(time_fps &&other) noexcept
{
    int32_t destroy_error;
    int32_t move_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_fps::initialize(move)", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    move_error = this->move(other);
    if (move_error != FT_ERR_SUCCESS)
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (move_error);
}

int32_t time_fps::move(time_fps &other) noexcept
{
    int32_t lock_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_fps::move", "source is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    this->_frame_duration_ms = other._frame_duration_ms;
    this->_frames_per_second = other._frames_per_second;
    this->_last_frame_time = other._last_frame_time;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._frame_duration_ms = 0.0;
    other._frames_per_second = static_cast<int64_t>(0);
    other._last_frame_time = std::chrono::steady_clock::time_point();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t time_fps::destroy(void) noexcept
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    this->_frame_duration_ms = 0.0;
    this->_frames_per_second = static_cast<int64_t>(0);
    this->_last_frame_time = std::chrono::steady_clock::time_point();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int64_t time_fps::get_frames_per_second(void) noexcept
{
    int64_t frames_per_second_value;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_fps::get_frames_per_second");
    frames_per_second_value = static_cast<int64_t>(0);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (frames_per_second_value);
    if (this->_frames_per_second > static_cast<int64_t>(0))
        frames_per_second_value = this->_frames_per_second;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (frames_per_second_value);
}

int32_t time_fps::set_frames_per_second(int64_t frames_per_second) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_fps::set_frames_per_second");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (frames_per_second < static_cast<int64_t>(24))
    {
        this->_frames_per_second = static_cast<int64_t>(0);
        this->_frame_duration_ms = 0.0;
        this->_last_frame_time = std::chrono::steady_clock::now();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_frames_per_second = frames_per_second;
    this->_frame_duration_ms = 1000.0 / static_cast<double>(frames_per_second);
    this->_last_frame_time = std::chrono::steady_clock::now();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

void time_fps::sleep_to_next_frame(void) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_fps::sleep_to_next_frame");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (this->_frame_duration_ms > 0.0)
    {
        std::chrono::steady_clock::time_point now;
        int64_t elapsed;
        double remaining_ms;

        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_last_frame_time).count();
        remaining_ms = this->_frame_duration_ms - static_cast<double>(elapsed);
        if (remaining_ms >= 1.0)
        {
            time_sleep_ms(static_cast<uint32_t>(remaining_ms));
            now = std::chrono::steady_clock::now();
        }
        this->_last_frame_time = now;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t time_fps::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_fps::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t time_fps::disable_thread_safety(void) noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool time_fps::is_thread_safe(void) const noexcept
{
    return (this->_mutex != ft_nullptr);
}

#include "time.hpp"
#include "time_timer.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"

#include <chrono>
#include <new>
#include "../PThread/mutex.hpp"

time_timer::time_timer() noexcept
{
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    this->_duration_ms = static_cast<int64_t>(0);
    this->_start_time = std::chrono::steady_clock::time_point();
    this->_running = FT_FALSE;
    return ;
}

time_timer::~time_timer() noexcept
{
    (void)this->destroy();
    this->_mutex = ft_nullptr;
    this->_duration_ms = static_cast<int64_t>(0);
    this->_start_time = std::chrono::steady_clock::time_point();
    this->_running = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

int32_t time_timer::initialize(void) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "time_timer::initialize", "already initialised");
    this->_duration_ms = static_cast<int64_t>(0);
    this->_start_time = std::chrono::steady_clock::time_point();
    this->_running = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t time_timer::initialize(const time_timer &other) noexcept
{
    int32_t destroy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_timer::initialize(copy)", "source is uninitialised");
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
    this->_duration_ms = other._duration_ms;
    this->_start_time = other._start_time;
    this->_running = other._running;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t time_timer::initialize(time_timer &&other) noexcept
{
    int32_t destroy_error;
    int32_t move_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_timer::initialize(move)", "source is uninitialised");
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

int32_t time_timer::move(time_timer &other) noexcept
{
    int32_t lock_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "time_timer::move", "source is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    this->_duration_ms = other._duration_ms;
    this->_start_time = other._start_time;
    this->_running = other._running;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._duration_ms = static_cast<int64_t>(0);
    other._start_time = std::chrono::steady_clock::time_point();
    other._running = FT_FALSE;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t time_timer::destroy(void) noexcept
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
    this->_duration_ms = static_cast<int64_t>(0);
    this->_start_time = std::chrono::steady_clock::time_point();
    this->_running = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

void time_timer::start(int64_t duration_ms) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::start");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (duration_ms < static_cast<int64_t>(0))
        this->_running = FT_FALSE;
    else
    {
        this->_duration_ms = duration_ms;
        this->_start_time = std::chrono::steady_clock::now();
        this->_running = FT_TRUE;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int64_t time_timer::update(void) noexcept
{
    int64_t remaining;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::update");
    remaining = static_cast<int64_t>(-1);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (remaining);
    if (this->_running == FT_TRUE)
    {
        std::chrono::steady_clock::time_point now;
        int64_t elapsed;

        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_start_time).count();
        if (elapsed >= this->_duration_ms)
        {
            this->_running = FT_FALSE;
            remaining = static_cast<int64_t>(0);
        }
        else
            remaining = this->_duration_ms - elapsed;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (remaining);
}

int64_t time_timer::add_time(int64_t amount_ms) noexcept
{
    int64_t result;
    int32_t lock_error;

    result = static_cast<int64_t>(-1);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::add_time");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (result);
    if (this->_running == FT_TRUE && amount_ms >= static_cast<int64_t>(0)
        && this->_duration_ms <= INT64_MAX - amount_ms)
    {
        std::chrono::steady_clock::time_point now;
        int64_t elapsed;

        this->_duration_ms += amount_ms;
        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_start_time).count();
        if (elapsed >= this->_duration_ms)
        {
            this->_running = FT_FALSE;
            result = static_cast<int64_t>(0);
        }
        else
            result = this->_duration_ms - elapsed;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

int64_t time_timer::remove_time(int64_t amount_ms) noexcept
{
    int64_t result;
    int32_t lock_error;

    result = static_cast<int64_t>(-1);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::remove_time");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (result);
    if (this->_running == FT_TRUE && amount_ms >= static_cast<int64_t>(0))
    {
        std::chrono::steady_clock::time_point now;
        int64_t elapsed;

        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_start_time).count();
        if (amount_ms >= this->_duration_ms - elapsed)
        {
            this->_duration_ms = elapsed;
            this->_running = FT_FALSE;
            result = static_cast<int64_t>(0);
        }
        else
        {
            this->_duration_ms -= amount_ms;
            result = this->_duration_ms - elapsed;
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

void time_timer::sleep_remaining(void) noexcept
{
    int64_t remaining;
    int32_t lock_error;

    remaining = static_cast<int64_t>(-1);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::sleep_remaining");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (this->_running == FT_TRUE)
    {
        std::chrono::steady_clock::time_point now;
        int64_t elapsed;

        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_start_time).count();
        if (elapsed >= this->_duration_ms)
        {
            this->_running = FT_FALSE;
            remaining = static_cast<int64_t>(0);
        }
        else
            remaining = this->_duration_ms - elapsed;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    while (remaining > 0)
    {
        // UINT32_MAX is the infinite sleep sentinel on Windows.
        constexpr uint32_t MAX_SLEEP_MS = UINT32_MAX - 1U;
        uint32_t chunk = MAX_SLEEP_MS;
        if (remaining < MAX_SLEEP_MS)
            chunk = static_cast<uint32_t>(remaining);
        time_sleep_ms(chunk);
        remaining -= chunk;
    }
    return ;
}

int32_t time_timer::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "time_timer::enable_thread_safety");
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

int32_t time_timer::disable_thread_safety(void) noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool time_timer::is_thread_safe(void) const noexcept
{
    return (this->_mutex != ft_nullptr);
}

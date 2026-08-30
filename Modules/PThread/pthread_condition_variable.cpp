#include "condition.hpp"
#include "mutex.hpp"
#include "pthread.hpp"
#include "recursive_mutex.hpp"
#include "pthread_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include <new>
#include <cerrno>
#include <time.h>

static bool compute_wait_deadline(const struct timespec &relative_time, struct timespec *absolute_time, int *error_code)
{
    struct timespec current_time;

    if (relative_time.tv_sec < 0)
    {
        *error_code = FT_ERR_INVALID_ARGUMENT;
        return (false);
    }
    if (relative_time.tv_nsec < 0)
    {
        *error_code = FT_ERR_INVALID_ARGUMENT;
        return (false);
    }
    if (clock_gettime(CLOCK_REALTIME, &current_time) != 0)
    {
        *error_code = cmp_map_system_error_to_ft(errno);
        return (false);
    }
    absolute_time->tv_sec = current_time.tv_sec + relative_time.tv_sec;
    absolute_time->tv_nsec = current_time.tv_nsec + relative_time.tv_nsec;
    while (absolute_time->tv_nsec >= 1000000000L)
    {
        absolute_time->tv_nsec -= 1000000000L;
        absolute_time->tv_sec += 1;
    }
    *error_code = FT_ERR_SUCCESS;
    return (true);
}

pt_condition_variable::pt_condition_variable()
    : _condition(), _mutex(), _condition_initialised(false), _mutex_initialised(false),
    _state_mutex(ft_nullptr)
{
    return ;
}

pt_condition_variable::~pt_condition_variable()
{
    if (this->_condition_initialised)
        pt_cond_destroy(&this->_condition);
    if (this->_mutex_initialised)
        pthread_mutex_destroy(&this->_mutex);
    this->teardown_thread_safety();
    return ;
}

int pt_condition_variable::ensure_native_sync_objects()
{
    int native_error;
    bool mutex_created;

    this->_initialization_mutex.lock();
    if (this->_mutex_initialised && this->_condition_initialised)
    {
        this->_initialization_mutex.unlock();
        return (FT_ERR_SUCCESS);
    }
    mutex_created = false;
    if (!this->_mutex_initialised)
    {
        native_error = pthread_mutex_init(&this->_mutex, ft_nullptr);
        if (native_error != 0)
        {
            this->_initialization_mutex.unlock();
            return (cmp_map_system_error_to_ft(native_error));
        }
        this->_mutex_initialised = true;
        mutex_created = true;
    }
    native_error = pt_cond_init(&this->_condition, ft_nullptr);
    if (native_error != 0)
    {
        if (mutex_created)
        {
            (void)pthread_mutex_destroy(&this->_mutex);
            this->_mutex_initialised = false;
        }
        this->_initialization_mutex.unlock();
        return (cmp_map_system_error_to_ft(native_error));
    }
    this->_condition_initialised = true;
    this->_initialization_mutex.unlock();
    return (FT_ERR_SUCCESS);
}

int pt_condition_variable::lock_internal(bool *lock_acquired) const
{
    int state_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    state_error = pt_mutex_lock_if_not_null(this->_state_mutex);
    if (state_error != FT_ERR_SUCCESS)
    {
        if (state_error == FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            bool state_lock_acquired;

            state_lock_acquired = false;
            if (this->_state_mutex->lock_state(&state_lock_acquired) == 0)
                this->_state_mutex->unlock_state(state_lock_acquired);
            return (FT_ERR_SUCCESS);
        }
        return (state_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

void pt_condition_variable::unlock_internal(bool lock_acquired) const
{
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
        return ;
    (void)pt_mutex_unlock_if_not_null(this->_state_mutex);
    return ;
}

void pt_condition_variable::teardown_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        this->_state_mutex->destroy();
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return ;
}

int pt_condition_variable::enable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_state_mutex = new (std::nothrow) pt_mutex();
    if (this->_state_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int init_error = this->_state_mutex->initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
        return (init_error);
    }
    return (FT_ERR_SUCCESS);
}

int pt_condition_variable::disable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return (FT_ERR_SUCCESS);
}

bool pt_condition_variable::is_thread_safe() const
{
    return (this->_state_mutex != ft_nullptr);
}

int pt_condition_variable::lock_state(bool *lock_acquired) const
{
    int result;

    result = this->lock_internal(lock_acquired);
    return (result);
}

void pt_condition_variable::unlock_state(bool lock_acquired) const
{
    this->unlock_internal(lock_acquired);
    return ;
}

int pt_condition_variable::wait(pt_mutex &mutex)
{
    int initialize_error;

    initialize_error = this->ensure_native_sync_objects();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (!mutex.lockState())
        return (FT_ERR_MUTEX_NOT_OWNER);

    int native_lock_error = pthread_mutex_lock(&this->_mutex);
    if (native_lock_error != 0)
        return (cmp_map_system_error_to_ft(native_lock_error));
    int unlock_error = mutex.unlock();
    if (unlock_error != FT_ERR_SUCCESS)
    {
        pthread_mutex_unlock(&this->_mutex);
        return (unlock_error);
    }

    int wait_result = pt_cond_wait(&this->_condition, &this->_mutex);
    int native_unlock_error = pthread_mutex_unlock(&this->_mutex);
    int relock_error = mutex.lock();
    if (native_unlock_error != 0)
    {
        if (relock_error != FT_ERR_SUCCESS)
            return (relock_error);
        return (cmp_map_system_error_to_ft(native_unlock_error));
    }
    if (relock_error != FT_ERR_SUCCESS)
        return (relock_error);
    if (wait_result == 0)
        return (FT_ERR_SUCCESS);
    return (cmp_map_system_error_to_ft(wait_result));
}

int pt_condition_variable::wait_for(pt_mutex &mutex, const struct timespec &relative_time)
{
    struct timespec absolute_time;
    int conversion_error;

    if (!compute_wait_deadline(relative_time, &absolute_time, &conversion_error))
        return (conversion_error);
    return (this->wait_until(mutex, absolute_time));
}

int pt_condition_variable::wait_until(pt_mutex &mutex, const struct timespec &absolute_time)
{
    int initialize_error;

    initialize_error = this->ensure_native_sync_objects();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (!mutex.lockState())
        return (FT_ERR_MUTEX_NOT_OWNER);

    int native_lock_error = pthread_mutex_lock(&this->_mutex);
    if (native_lock_error != 0)
        return (cmp_map_system_error_to_ft(native_lock_error));
    int unlock_error = mutex.unlock();
    if (unlock_error != FT_ERR_SUCCESS)
    {
        pthread_mutex_unlock(&this->_mutex);
        return (unlock_error);
    }

    int wait_result = pthread_cond_timedwait(&this->_condition, &this->_mutex, &absolute_time);
    int native_unlock_error = pthread_mutex_unlock(&this->_mutex);
    int relock_error = mutex.lock();
    if (native_unlock_error != 0)
    {
        if (relock_error != FT_ERR_SUCCESS)
            return (relock_error);
        return (cmp_map_system_error_to_ft(native_unlock_error));
    }
    if (relock_error != FT_ERR_SUCCESS)
        return (relock_error);
    if (wait_result == 0)
        return (FT_ERR_SUCCESS);
    if (wait_result == ETIMEDOUT)
        return (ETIMEDOUT);
    return (cmp_map_system_error_to_ft(wait_result));
}

int pt_condition_variable::signal()
{
    int initialize_error;

    initialize_error = this->ensure_native_sync_objects();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    int native_lock_error = pthread_mutex_lock(&this->_mutex);
    if (native_lock_error != 0)
        return (cmp_map_system_error_to_ft(native_lock_error));
    int signal_error = pt_cond_signal(&this->_condition);
    int native_unlock_error = pthread_mutex_unlock(&this->_mutex);
    if (signal_error != 0)
    {
        if (native_unlock_error != 0)
            return (cmp_map_system_error_to_ft(native_unlock_error));
        return (cmp_map_system_error_to_ft(signal_error));
    }
    if (native_unlock_error != 0)
        return (cmp_map_system_error_to_ft(native_unlock_error));
    return (FT_ERR_SUCCESS);
}

int pt_condition_variable::broadcast()
{
    int initialize_error;

    initialize_error = this->ensure_native_sync_objects();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    int native_lock_error = pthread_mutex_lock(&this->_mutex);
    if (native_lock_error != 0)
        return (cmp_map_system_error_to_ft(native_lock_error));
    int broadcast_error = pt_cond_broadcast(&this->_condition);
    int native_unlock_error = pthread_mutex_unlock(&this->_mutex);
    if (broadcast_error != 0)
    {
        if (native_unlock_error != 0)
            return (cmp_map_system_error_to_ft(native_unlock_error));
        return (cmp_map_system_error_to_ft(broadcast_error));
    }
    if (native_unlock_error != 0)
        return (cmp_map_system_error_to_ft(native_unlock_error));
    return (FT_ERR_SUCCESS);
}

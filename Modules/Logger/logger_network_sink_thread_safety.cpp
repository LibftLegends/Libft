#include "logger_internal.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>

int32_t network_sink_prepare_thread_safety(s_network_sink *sink)
{
    if (!sink)
        return (FT_ERR_INVALID_ARGUMENT);
    if (sink->thread_safe_enabled && sink->mutex)
        return (FT_ERR_SUCCESS);
    pt_mutex *mutex_pointer = new (std::nothrow) pt_mutex();
    if (!mutex_pointer)
        return (FT_ERR_NO_MEMORY);
    const int32_t initialize_result = mutex_pointer->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_result);
    }
    sink->mutex = mutex_pointer;
    sink->thread_safe_enabled = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void network_sink_teardown_thread_safety(s_network_sink *sink)
{
    if (!sink)
        return ;
    if (sink->mutex)
    {
        (void)sink->mutex->destroy();
        delete sink->mutex;
        sink->mutex = ft_nullptr;
    }
    sink->thread_safe_enabled = FT_FALSE;
    return ;
}

int32_t network_sink_lock(const s_network_sink *sink, ft_bool *lock_acquired)
{
    if (lock_acquired)
        *lock_acquired = FT_FALSE;
    if (!sink)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!sink->thread_safe_enabled || !sink->mutex)
        return (FT_ERR_SUCCESS);
    const int32_t error_code_value = pt_mutex_lock_if_not_null(sink->mutex);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    if (lock_acquired)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void network_sink_unlock(const s_network_sink *sink, ft_bool lock_acquired)
{
    if (!sink || !lock_acquired)
        return ;
    (void)pt_mutex_unlock_if_not_null(sink->mutex);
    return ;
}

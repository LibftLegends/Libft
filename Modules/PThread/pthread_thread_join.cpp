#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "../Errno/errno.hpp"
#include "pthread.hpp"
#include "pthread_lock_tracking.hpp"

int pt_thread_join(pthread_t thread, void **retval)
{
    pt_thread_id_type thread_identifier;
    int return_value;
    int tracking_error;

    if (!thread)
    {
        return_value = ESRCH;
        return (return_value);
    }
    return_value = pthread_join(thread, retval);
    if (return_value != 0)
        return (return_value);
    thread_identifier = FT_THREAD_ID_FROM_PTHREAD(thread);
    tracking_error = pt_lock_tracking::notify_thread_exit(thread_identifier);
    if (tracking_error != FT_ERR_SUCCESS)
        return (tracking_error);
    return (return_value);
}

int pt_thread_timed_join(pthread_t thread, void **retval, long timeout_ms)
{
    pt_thread_id_type thread_identifier;
    int return_value;
    int tracking_error;

    if (!thread)
    {
        return_value = ESRCH;
        return (return_value);
    }
    if (timeout_ms < 0)
    {
        return_value = EINVAL;
        return (return_value);
    }
#ifdef __linux__
    struct timespec absolute_timeout;
    long additional_nanoseconds;

    return_value = clock_gettime(CLOCK_REALTIME, &absolute_timeout);
    if (return_value != 0)
    {
        return_value = errno;
        return (return_value);
    }
    absolute_timeout.tv_sec += timeout_ms / 1000;
    additional_nanoseconds = (timeout_ms % 1000) * 1000000;
    absolute_timeout.tv_nsec += additional_nanoseconds;
    while (absolute_timeout.tv_nsec >= 1000000000)
    {
        absolute_timeout.tv_nsec -= 1000000000;
        absolute_timeout.tv_sec += 1;
    }
    return_value = pthread_timedjoin_np(thread, retval, &absolute_timeout);
    if (return_value != 0)
        return (return_value);
    thread_identifier = FT_THREAD_ID_FROM_PTHREAD(thread);
    tracking_error = pt_lock_tracking::notify_thread_exit(thread_identifier);
    if (tracking_error != FT_ERR_SUCCESS)
        return (tracking_error);
    return (return_value);
#else
    long elapsed_timeout_ms;

    elapsed_timeout_ms = 0;
    while (elapsed_timeout_ms <= timeout_ms)
    {
        return_value = _pthread_tryjoin(thread, retval);
        if (return_value == 0)
        {
            thread_identifier = FT_THREAD_ID_FROM_PTHREAD(thread);
            tracking_error = pt_lock_tracking::notify_thread_exit(
                    thread_identifier);
            if (tracking_error != FT_ERR_SUCCESS)
                return (tracking_error);
            return (return_value);
        }
        if (return_value != EBUSY)
            return (return_value);
        if (elapsed_timeout_ms == timeout_ms)
            break ;
        (void)pt_thread_sleep(1);
        elapsed_timeout_ms += 1;
    }
    return (ETIMEDOUT);
#endif
}

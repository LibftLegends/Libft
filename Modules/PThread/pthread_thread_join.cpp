#include "pthread.hpp"
#include "pthread_lock_tracking.hpp"
#include <cerrno>
#include <ctime>

static pt_thread_id_type pt_join_thread_identifier(pthread_t thread)
{
#if defined(_WIN32)
    const HANDLE handle = pthread_gethandle(thread);
    if (handle == ft_nullptr || handle == INVALID_HANDLE_VALUE)
        return (0U);
    return (GetThreadId(handle));
#else
    return (thread);
#endif
}

static int pt_finish_join(int result, pt_thread_id_type thread_identifier)
{
    if (result != 0 || thread_identifier == 0)
        return (result);
    return (pt_lock_tracking::notify_thread_exit(thread_identifier));
}

int pt_thread_join(pthread_t thread, void **retval)
{
    if (!thread)
        return (ESRCH);
    const pt_thread_id_type thread_identifier = pt_join_thread_identifier(thread);
    return (pt_finish_join(pthread_join(thread, retval), thread_identifier));
}

int pt_thread_timed_join(pthread_t thread, void **retval, long timeout_ms)
{
    if (!thread)
        return (ESRCH);
    if (timeout_ms < 0)
        return (EINVAL);
#if defined(__linux__)
    struct timespec absolute_timeout;
    if (clock_gettime(CLOCK_REALTIME, &absolute_timeout) != 0)
        return (errno);
    constexpr long NANOSECONDS_PER_SECOND = 1000000000L;
    const long nanoseconds = absolute_timeout.tv_nsec + (timeout_ms % 1000) * 1000000L;
    const long seconds = timeout_ms / 1000 + nanoseconds / NANOSECONDS_PER_SECOND;
    if (__builtin_add_overflow(absolute_timeout.tv_sec, seconds, &absolute_timeout.tv_sec))
        return (EINVAL);
    absolute_timeout.tv_nsec = nanoseconds % NANOSECONDS_PER_SECOND;
    return (pt_finish_join(pthread_timedjoin_np(thread, retval, &absolute_timeout),
            pt_join_thread_identifier(thread)));
#elif defined(_WIN32)
    const HANDLE handle = pthread_gethandle(thread);
    pt_thread_id_type thread_identifier = 0U;
    if (handle != ft_nullptr && handle != INVALID_HANDLE_VALUE)
        thread_identifier = GetThreadId(handle);
    const int first_attempt = _pthread_tryjoin(thread, retval);
    if (first_attempt != EBUSY)
        return (pt_finish_join(first_attempt, thread_identifier));
    if (timeout_ms == 0)
        return (ETIMEDOUT);
    if (handle == ft_nullptr || handle == INVALID_HANDLE_VALUE)
        return (ESRCH);
    const DWORD wait_result = WaitForSingleObject(handle, static_cast<DWORD>(timeout_ms));
    if (wait_result == WAIT_TIMEOUT)
        return (ETIMEDOUT);
    if (wait_result != WAIT_OBJECT_0)
        return (ESRCH);
    return (pt_finish_join(_pthread_tryjoin(thread, retval), thread_identifier));
#else
    (void)timeout_ms;
    return (pt_thread_join(thread, retval));
#endif
}

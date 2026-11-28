#include "pthread.hpp"
#include "../Basic/limits.hpp"
#include "mutex.hpp"
#include "recursive_mutex.hpp"
#include "../Time/time.hpp"
#include "../Errno/errno.hpp"

t_thread_id ft_this_thread_get_id()
{
    t_thread_id thread_identifier;

    thread_identifier.native_id = pt_thread_self();
    return (thread_identifier);
}

static void ft_this_thread_sleep_milliseconds(int64_t remaining)
{
    constexpr uint32_t MAX_FINITE_SLEEP_MS = UINT32_MAX - 1U;
    while (remaining > 0)
    {
        uint32_t chunk = MAX_FINITE_SLEEP_MS;
        if (remaining < MAX_FINITE_SLEEP_MS)
            chunk = static_cast<uint32_t>(remaining);
        pt_thread_sleep(chunk);
        remaining -= chunk;
    }
    return ;
}

void ft_this_thread_sleep_for(t_duration_milliseconds duration)
{
    ft_this_thread_sleep_milliseconds(duration.milliseconds);
    return ;
}

void ft_this_thread_sleep_until(t_monotonic_time_point time_point)
{
    const int64_t now = time_monotonic();
    if (time_point.milliseconds <= now)
        return ;
    int64_t remaining;
    if (__builtin_sub_overflow(time_point.milliseconds, now, &remaining))
        remaining = INT64_MAX;
    ft_this_thread_sleep_milliseconds(remaining);
    return ;
}

void ft_this_thread_yield()
{
    pt_thread_yield();
    return ;
}

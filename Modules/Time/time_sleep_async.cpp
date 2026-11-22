#include "time.hpp"

int32_t event_loop_run(event_loop *loop, int32_t timeout_milliseconds);

void time_async_sleep_init(t_time_async_sleep *sleep_state, int64_t delay_milliseconds)
{
    if (sleep_state == ft_nullptr)
        return ;
    if (delay_milliseconds < 0)
        delay_milliseconds = 0;
    const int64_t now = time_monotonic();
    int64_t deadline;
    if (__builtin_add_overflow(now, delay_milliseconds, &deadline))
        deadline = INT64_MAX;
    sleep_state->deadline = time_monotonic_point_create(deadline);
    sleep_state->completed = (delay_milliseconds == 0);
    return ;
}

ft_bool time_async_sleep_is_complete(const t_time_async_sleep *sleep_state)
{
    return (sleep_state == ft_nullptr || sleep_state->completed);
}

int64_t time_async_sleep_remaining_ms(t_time_async_sleep *sleep_state)
{
    if (sleep_state == ft_nullptr || sleep_state->completed)
        return (0);
    const int64_t now = time_monotonic();
    const int64_t deadline = sleep_state->deadline.milliseconds;
    if (deadline <= now)
    {
        sleep_state->completed = FT_TRUE;
        return (0);
    }
    int64_t remaining;
    if (__builtin_sub_overflow(deadline, now, &remaining))
        return (INT64_MAX);
    return (remaining);
}

int32_t time_async_sleep_poll(event_loop *loop, t_time_async_sleep *sleep_state)
{
    if (loop == ft_nullptr || sleep_state == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const int64_t remaining = time_async_sleep_remaining_ms(sleep_state);
    if (sleep_state->completed)
        return (FT_ERR_SUCCESS);
    int32_t timeout = INT32_MAX;
    if (remaining < INT32_MAX)
        timeout = static_cast<int32_t>(remaining);
    const int32_t result = event_loop_run(loop, timeout);
    if (result < 0)
        return (FT_ERR_IO);
    if (result == 0 && time_async_sleep_remaining_ms(sleep_state) == 0)
        return (FT_ERR_SUCCESS);
    return (FT_ERR_TIMEOUT);
}

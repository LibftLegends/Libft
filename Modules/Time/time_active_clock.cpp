#include "time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"

static uint64_t time_active_clock_saturating_add(uint64_t first_value,
    uint64_t second_value)
{
    if (second_value > UINT64_MAX - first_value)
        return (UINT64_MAX);
    return (first_value + second_value);
}

static ft_bool time_active_clock_read_now(uint64_t *microseconds_out)
{
    if (cmp_active_clock_now_microseconds(microseconds_out)
        != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

void time_active_clock_init(t_active_clock *clock)
{
    if (clock == ft_nullptr)
        return ;
    clock->accumulated_microseconds = 0;
    clock->started_microseconds = 0;
    clock->running = FT_FALSE;
    return ;
}

ft_bool time_active_clock_start(t_active_clock *clock)
{
    uint64_t current_microseconds;

    if (clock == ft_nullptr)
        return (FT_FALSE);
    if (clock->running == FT_TRUE)
        return (FT_TRUE);
    if (!time_active_clock_read_now(&current_microseconds))
        return (FT_FALSE);
    clock->started_microseconds = current_microseconds;
    clock->running = FT_TRUE;
    return (FT_TRUE);
}

ft_bool time_active_clock_stop(t_active_clock *clock)
{
    uint64_t current_microseconds;
    uint64_t elapsed_microseconds;

    if (clock == ft_nullptr)
        return (FT_FALSE);
    if (clock->running == FT_FALSE)
        return (FT_TRUE);
    if (!time_active_clock_read_now(&current_microseconds))
        return (FT_FALSE);
    elapsed_microseconds = current_microseconds - clock->started_microseconds;
    clock->accumulated_microseconds = time_active_clock_saturating_add(
        clock->accumulated_microseconds, elapsed_microseconds);
    clock->started_microseconds = 0;
    clock->running = FT_FALSE;
    return (FT_TRUE);
}

ft_bool time_active_clock_resume(t_active_clock *clock)
{
    return (time_active_clock_start(clock));
}

ft_bool time_active_clock_restart(t_active_clock *clock)
{
    uint64_t current_microseconds;

    if (clock == ft_nullptr)
        return (FT_FALSE);
    if (!time_active_clock_read_now(&current_microseconds))
        return (FT_FALSE);
    clock->accumulated_microseconds = 0;
    clock->started_microseconds = current_microseconds;
    clock->running = FT_TRUE;
    return (FT_TRUE);
}

uint64_t time_active_clock_report(const t_active_clock *clock)
{
    uint64_t current_microseconds;
    uint64_t elapsed_microseconds;

    if (clock == ft_nullptr)
        return (0);
    if (clock->running == FT_FALSE)
        return (clock->accumulated_microseconds);
    if (!time_active_clock_read_now(&current_microseconds))
        return (clock->accumulated_microseconds);
    elapsed_microseconds = current_microseconds - clock->started_microseconds;
    return (time_active_clock_saturating_add(clock->accumulated_microseconds,
        elapsed_microseconds));
}

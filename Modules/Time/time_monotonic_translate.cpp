#include "time.hpp"

ft_bool time_get_monotonic_wall_anchor(t_monotonic_time_point &anchor_monotonic,
    int64_t &anchor_wall_ms)
{
    const int64_t before = time_monotonic();
    const int64_t wall = time_now_ms();
    const int64_t after = time_monotonic();
    int64_t elapsed;
    if (__builtin_sub_overflow(after, before, &elapsed)
        || elapsed == INT64_MIN || elapsed == INT64_MAX)
        return (FT_FALSE);
    anchor_monotonic = time_monotonic_point_create(before + elapsed / 2);
    anchor_wall_ms = wall;
    return (FT_TRUE);
}

ft_bool time_monotonic_to_wall_ms(t_monotonic_time_point monotonic_point,
    t_monotonic_time_point anchor_monotonic, int64_t anchor_wall_ms,
    int64_t &out_wall_ms)
{
    int64_t delta;
    int64_t translated;
    if (__builtin_sub_overflow(monotonic_point.milliseconds,
            anchor_monotonic.milliseconds, &delta)
        || delta == INT64_MIN || delta == INT64_MAX
        || __builtin_add_overflow(anchor_wall_ms, delta, &translated))
        return (FT_FALSE);
    out_wall_ms = translated;
    return (FT_TRUE);
}

ft_bool time_wall_ms_to_monotonic(int64_t wall_time_ms,
    t_monotonic_time_point anchor_monotonic, int64_t anchor_wall_ms,
    t_monotonic_time_point &out_monotonic)
{
    int64_t delta;
    int64_t translated;
    if (__builtin_sub_overflow(wall_time_ms, anchor_wall_ms, &delta)
        || __builtin_add_overflow(anchor_monotonic.milliseconds, delta, &translated))
        return (FT_FALSE);
    out_monotonic = time_monotonic_point_create(translated);
    return (FT_TRUE);
}

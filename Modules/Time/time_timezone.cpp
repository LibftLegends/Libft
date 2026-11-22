#include "time.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <cerrno>
#include <limits>

ft_bool time_get_local_offset(t_time time_value, int32_t *offset_minutes,
    ft_bool *is_daylight_saving)
{
    if ((offset_minutes == ft_nullptr && is_daylight_saving == ft_nullptr)
        || time_value < std::numeric_limits<std::time_t>::min()
        || time_value > std::numeric_limits<std::time_t>::max())
        return (FT_FALSE);
    const std::time_t standard_time = static_cast<std::time_t>(time_value);
    std::tm local_time;
    if (cmp_localtime(&standard_time, &local_time) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_bool daylight_saving = local_time.tm_isdst > 0;
    if (offset_minutes != ft_nullptr)
    {
        const std::time_t local_as_utc = cmp_timegm(&local_time);
        if (local_as_utc == static_cast<std::time_t>(-1) && errno != 0)
            return (FT_FALSE);
        int64_t difference;
        if (__builtin_sub_overflow(local_as_utc, standard_time, &difference))
            return (FT_FALSE);
        const int64_t offset = difference / 60;
        if (offset < INT32_MIN || offset > INT32_MAX)
            return (FT_FALSE);
        *offset_minutes = static_cast<int32_t>(offset);
    }
    if (is_daylight_saving != ft_nullptr)
        *is_daylight_saving = daylight_saving;
    return (FT_TRUE);
}

ft_bool time_convert_timezone(t_time time_value, int32_t source_offset_minutes,
    int32_t target_offset_minutes, t_time *converted_time)
{
    if (converted_time == ft_nullptr)
        return (FT_FALSE);
    const int64_t offset_seconds = (static_cast<int64_t>(target_offset_minutes)
        - source_offset_minutes) * 60;
    t_time converted;
    if (__builtin_add_overflow(time_value, offset_seconds, &converted))
        return (FT_FALSE);
    *converted_time = converted;
    return (FT_TRUE);
}

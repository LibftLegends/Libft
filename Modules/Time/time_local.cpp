#include "time.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <limits>

void time_local(t_time time_value, t_time_info *out)
{
    if (out == ft_nullptr || time_value < std::numeric_limits<std::time_t>::min()
        || time_value > std::numeric_limits<std::time_t>::max())
        return ;
    const std::time_t standard_time = static_cast<std::time_t>(time_value);
    std::tm temporary;
    if (cmp_localtime(&standard_time, &temporary) != FT_ERR_SUCCESS)
        return ;
    ft_bool lock_acquired = FT_FALSE;
    if (time_info_lock(out, &lock_acquired) != FT_ERR_SUCCESS)
        return ;
    out->seconds = temporary.tm_sec;
    out->minutes = temporary.tm_min;
    out->hours = temporary.tm_hour;
    out->month_day = temporary.tm_mday;
    out->month = temporary.tm_mon;
    out->year = temporary.tm_year;
    out->week_day = temporary.tm_wday;
    out->year_day = temporary.tm_yday;
    out->is_daylight_saving = temporary.tm_isdst;
    if (lock_acquired == FT_TRUE)
        time_info_unlock(out, lock_acquired);
    return ;
}

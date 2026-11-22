#include "time.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include <limits>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static t_time time_relative_clamp_int128(__int128 value)
{
    if (value > static_cast<__int128>(std::numeric_limits<t_time>::max()))
        return (std::numeric_limits<t_time>::max());
    if (value < static_cast<__int128>(std::numeric_limits<t_time>::min()))
        return (std::numeric_limits<t_time>::min());
    return (static_cast<t_time>(value));
}

static t_time time_relative_floor_seconds(t_time time_value, int64_t unit_seconds)
{
    const int64_t remainder = time_value % unit_seconds;
    const t_time whole_units = time_value - remainder;
    if (remainder >= 0)
        return (whole_units);
    if (whole_units < INT64_MIN + unit_seconds)
        return (INT64_MIN);
    return (whole_units - unit_seconds);
}

t_time  time_add_seconds(t_time time_value, int64_t seconds)
{
    __int128 adjusted_value;

    adjusted_value = static_cast<__int128>(time_value);
    adjusted_value += static_cast<__int128>(seconds);
    return (time_relative_clamp_int128(adjusted_value));
}

t_time  time_add_minutes(t_time time_value, int64_t minutes)
{
    __int128 adjusted_value;

    adjusted_value = static_cast<__int128>(time_value);
    adjusted_value += static_cast<__int128>(minutes) * static_cast<__int128>(60);
    return (time_relative_clamp_int128(adjusted_value));
}

t_time  time_add_hours(t_time time_value, int64_t hours)
{
    __int128 adjusted_value;

    adjusted_value = static_cast<__int128>(time_value);
    adjusted_value += static_cast<__int128>(hours) * static_cast<__int128>(3600);
    return (time_relative_clamp_int128(adjusted_value));
}

t_time  time_add_weeks(t_time time_value, int64_t weeks)
{
    __int128 adjusted_value;

    adjusted_value = static_cast<__int128>(time_value);
    adjusted_value += static_cast<__int128>(weeks) * static_cast<__int128>(604800);
    return (time_relative_clamp_int128(adjusted_value));
}

t_time  time_add_days(t_time time_value, int64_t days)
{
    __int128 adjusted_value;

    adjusted_value = static_cast<__int128>(time_value);
    adjusted_value += static_cast<__int128>(days) * static_cast<__int128>(86400);
    return (time_relative_clamp_int128(adjusted_value));
}

t_time  time_floor_to_second(t_time time_value)
{
    return (time_relative_floor_seconds(time_value, 1));
}

t_time  time_floor_to_minute(t_time time_value)
{
    return (time_relative_floor_seconds(time_value, 60));
}

t_time  time_floor_to_hour(t_time time_value)
{
    return (time_relative_floor_seconds(time_value, 3600));
}

t_time  time_floor_to_day(t_time time_value)
{
    return (time_relative_floor_seconds(time_value, 86400));
}

t_time  time_floor_to_week(t_time time_value)
{
    return (time_relative_floor_seconds(time_value, 604800));
}

static t_time time_relative_ceiling_seconds(t_time time_value, int64_t unit_seconds)
{
    const int64_t remainder = time_value % unit_seconds;
    const t_time whole_units = time_value - remainder;
    if (remainder <= 0)
        return (whole_units);
    if (whole_units > INT64_MAX - unit_seconds)
        return (INT64_MAX);
    return (whole_units + unit_seconds);
}

static ft_bool time_relative_is_leap_year(int32_t year)
{
    if (year % 4 != 0)
        return (FT_FALSE);
    if (year % 100 != 0)
        return (FT_TRUE);
    if (year % 400 != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t time_relative_days_in_month(int32_t year, int32_t month)
{
    if (month == 1)
        return (31);
    if (month == 2)
    {
        if (time_relative_is_leap_year(year))
            return (29);
        return (28);
    }
    if (month == 3)
        return (31);
    if (month == 4)
        return (30);
    if (month == 5)
        return (31);
    if (month == 6)
        return (30);
    if (month == 7)
        return (31);
    if (month == 8)
        return (31);
    if (month == 9)
        return (30);
    if (month == 10)
        return (31);
    if (month == 11)
        return (30);
    if (month == 12)
        return (31);
    return (0);
}

static ft_bool time_relative_months_to_year_month(__int128 total_months, int32_t *year_out, int32_t *month_out)
{
    __int128 year_value;
    __int128 month_value;

    if (!year_out || !month_out)
        return (FT_FALSE);
    year_value = total_months / static_cast<__int128>(12);
    month_value = total_months % static_cast<__int128>(12);
    if (month_value < 0)
    {
        month_value += static_cast<__int128>(12);
        year_value -= static_cast<__int128>(1);
    }
    year_value += static_cast<__int128>(1900);
    if (year_value > static_cast<__int128>(std::numeric_limits<int32_t>::max()))
        return (FT_FALSE);
    if (year_value < static_cast<__int128>(std::numeric_limits<int32_t>::min()))
        return (FT_FALSE);
    *year_out = static_cast<int32_t>(year_value);
    *month_out = static_cast<int32_t>(month_value);
    return (FT_TRUE);
}

static ft_bool time_relative_get_utc_tm(t_time time_value, std::tm *output_time)
{
    std::time_t standard_time;
    int32_t error_code;

    if (!output_time)
        return (FT_FALSE);
    standard_time = time_value;
    error_code = cmp_gmtime(&standard_time, output_time);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool time_relative_tm_to_time(std::tm *time_value, t_time *output_time)
{
    std::time_t epoch_time;

    if (!time_value || !output_time)
        return (FT_FALSE);
    time_value->tm_isdst = 0;
    errno = 0;
    epoch_time = cmp_timegm(time_value);
    if (epoch_time == static_cast<std::time_t>(-1) && errno != 0)
        return (FT_FALSE);
    *output_time = epoch_time;
    return (FT_TRUE);
}

t_time  time_ceiling_to_second(t_time time_value)
{
    return (time_relative_ceiling_seconds(time_value, 1));
}

t_time  time_ceiling_to_minute(t_time time_value)
{
    return (time_relative_ceiling_seconds(time_value, 60));
}

t_time  time_ceiling_to_hour(t_time time_value)
{
    return (time_relative_ceiling_seconds(time_value, 3600));
}

t_time  time_ceiling_to_day(t_time time_value)
{
    return (time_relative_ceiling_seconds(time_value, 86400));
}

t_time  time_ceiling_to_week(t_time time_value)
{
    return (time_relative_ceiling_seconds(time_value, 604800));
}

t_time  time_floor_to_month(t_time time_value)
{
    std::tm utc_time;
    t_time floored_time;

    if (!time_relative_get_utc_tm(time_value, &utc_time))
        return (time_value);
    utc_time.tm_mday = 1;
    utc_time.tm_hour = 0;
    utc_time.tm_min = 0;
    utc_time.tm_sec = 0;
    if (!time_relative_tm_to_time(&utc_time, &floored_time))
        return (time_value);
    return (floored_time);
}

t_time  time_ceiling_to_month(t_time time_value)
{
    std::tm utc_time;
    t_time floored_time;
    t_time ceiling_time;

    if (!time_relative_get_utc_tm(time_value, &utc_time))
        return (time_value);
    utc_time.tm_mday = 1;
    utc_time.tm_hour = 0;
    utc_time.tm_min = 0;
    utc_time.tm_sec = 0;
    if (!time_relative_tm_to_time(&utc_time, &floored_time))
        return (time_value);
    if (floored_time == time_value)
        return (floored_time);
    utc_time.tm_mon += 1;
    if (!time_relative_tm_to_time(&utc_time, &ceiling_time))
        return (time_value);
    return (ceiling_time);
}

t_time  time_floor_to_quarter(t_time time_value)
{
    std::tm utc_time;
    t_time floored_time;

    if (!time_relative_get_utc_tm(time_value, &utc_time))
        return (time_value);
    utc_time.tm_mon = (utc_time.tm_mon / 3) * 3;
    utc_time.tm_mday = 1;
    utc_time.tm_hour = 0;
    utc_time.tm_min = 0;
    utc_time.tm_sec = 0;
    if (!time_relative_tm_to_time(&utc_time, &floored_time))
        return (time_value);
    return (floored_time);
}

t_time  time_ceiling_to_quarter(t_time time_value)
{
    t_time floored_time;

    floored_time = time_floor_to_quarter(time_value);
    if (floored_time == time_value)
        return (floored_time);
    return (time_add_months(floored_time, 3));
}

t_time  time_add_months(t_time time_value, int64_t months)
{
    std::tm utc_time;
    t_time adjusted_time;
    __int128 total_months;
    int32_t target_year;
    int32_t target_month;
    int32_t days_in_target_month;
    int32_t target_day;

    if (!time_relative_get_utc_tm(time_value, &utc_time))
    {
        if (months >= 0)
            return (std::numeric_limits<t_time>::max());
        return (std::numeric_limits<t_time>::min());
    }
    total_months = static_cast<__int128>(utc_time.tm_year);
    total_months *= static_cast<__int128>(12);
    total_months += static_cast<__int128>(utc_time.tm_mon);
    total_months += static_cast<__int128>(months);
    if (!time_relative_months_to_year_month(total_months, &target_year, &target_month))
    {
        if (months >= 0)
            return (std::numeric_limits<t_time>::max());
        return (std::numeric_limits<t_time>::min());
    }
    days_in_target_month = time_relative_days_in_month(target_year, target_month + 1);
    target_day = utc_time.tm_mday;
    if (target_day > days_in_target_month)
        target_day = days_in_target_month;
    utc_time.tm_year = target_year - 1900;
    utc_time.tm_mon = target_month;
    utc_time.tm_mday = target_day;
    if (!time_relative_tm_to_time(&utc_time, &adjusted_time))
    {
        if (months >= 0)
            return (std::numeric_limits<t_time>::max());
        return (std::numeric_limits<t_time>::min());
    }
    return (adjusted_time);
}

t_time  time_add_quarters(t_time time_value, int64_t quarters)
{
    __int128 total_months;

    total_months = static_cast<__int128>(quarters);
    total_months *= static_cast<__int128>(3);
    if (total_months > static_cast<__int128>(std::numeric_limits<int64_t>::max()))
        return (std::numeric_limits<t_time>::max());
    if (total_months < static_cast<__int128>(std::numeric_limits<int64_t>::min()))
        return (std::numeric_limits<t_time>::min());
    return (time_add_months(time_value, static_cast<int64_t>(total_months)));
}

t_time  time_add_years(t_time time_value, int64_t years)
{
    __int128 total_months;

    total_months = static_cast<__int128>(years);
    total_months *= static_cast<__int128>(12);
    if (total_months > static_cast<__int128>(std::numeric_limits<int64_t>::max()))
        return (std::numeric_limits<t_time>::max());
    if (total_months < static_cast<__int128>(std::numeric_limits<int64_t>::min()))
        return (std::numeric_limits<t_time>::min());
    return (time_add_months(time_value, static_cast<int64_t>(total_months)));
}

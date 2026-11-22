#include "time_format_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <charconv>
#include <ctime>
#include <limits>
#include <new>

static ft_string *time_format_make_string(const char *buffer)
{
    ft_string *formatted = new (std::nothrow) ft_string();
    if (formatted == ft_nullptr)
        return (ft_nullptr);
    if (formatted->initialize(buffer) != FT_ERR_SUCCESS)
    {
        delete formatted;
        return (ft_nullptr);
    }
    return (formatted);
}

ft_size_t time_format_timezone_offset(char *buffer, ft_size_t size,
    int32_t offset_minutes)
{
    if (buffer == ft_nullptr || size == 0U)
        return (0U);
    int64_t magnitude = offset_minutes;
    char formatted[16];
    formatted[0] = '+';
    if (magnitude < 0)
    {
        formatted[0] = '-';
        magnitude = -magnitude;
    }
    const uint32_t hours = static_cast<uint32_t>(magnitude / 60);
    const uint32_t minutes = static_cast<uint32_t>(magnitude % 60);
    char *cursor = formatted + 1U;
    if (hours < 10U)
        *cursor++ = '0';
    cursor = std::to_chars(cursor, formatted + sizeof(formatted), hours).ptr;
    *cursor++ = ':';
    *cursor++ = static_cast<char>('0' + minutes / 10U);
    *cursor++ = static_cast<char>('0' + minutes % 10U);
    const ft_size_t length = static_cast<ft_size_t>(cursor - formatted);
    ft_size_t copied = length;
    if (copied >= size)
        copied = size - 1U;
    ft_memcpy(buffer, formatted, copied);
    buffer[copied] = '\0';
    if (copied != length)
        return (0U);
    return (length);
}

static ft_size_t time_format_calendar(char *buffer, ft_size_t size,
    t_time time_value, const char *format)
{
    if (time_value < std::numeric_limits<std::time_t>::min()
        || time_value > std::numeric_limits<std::time_t>::max())
        return (0U);
    const std::time_t standard_time = static_cast<std::time_t>(time_value);
    std::tm date;
    if (cmp_gmtime(&standard_time, &date) != FT_ERR_SUCCESS)
        return (0U);
    return (std::strftime(buffer, size, format, &date));
}

ft_size_t time_format_rfc3339_buffer(t_time time_value, char (&buffer)[21])
{
    return (time_format_calendar(buffer, sizeof(buffer), time_value,
            "%Y-%m-%dT%H:%M:%SZ"));
}

ft_string *time_format_iso8601(t_time time_value)
{
    char buffer[21];
    if (time_format_rfc3339_buffer(time_value, buffer) == 0U)
        return (ft_nullptr);
    return (time_format_make_string(buffer));
}

ft_string *time_format_iso8601_with_offset(t_time time_value,
    int32_t offset_minutes)
{
    const int64_t offset_seconds = static_cast<int64_t>(offset_minutes) * 60;
    if ((offset_seconds > 0 && time_value > INT64_MAX - offset_seconds)
        || (offset_seconds < 0 && time_value < INT64_MIN - offset_seconds))
        return (ft_nullptr);
    char buffer[26];
    const ft_size_t date_length = time_format_calendar(buffer, 20U,
            time_value + offset_seconds, "%Y-%m-%dT%H:%M:%S");
    if (date_length == 0U
        || time_format_timezone_offset(buffer + date_length, 7U,
            offset_minutes) == 0U)
        return (ft_nullptr);
    return (time_format_make_string(buffer));
}

ft_string *time_format_rfc3339(t_time time_value)
{
    return (time_format_iso8601(time_value));
}

ft_string *time_format_rfc3339_with_offset(t_time time_value, int32_t offset_minutes)
{
    if (offset_minutes == 0)
        return (time_format_rfc3339(time_value));
    return (time_format_iso8601_with_offset(time_value, offset_minutes));
}

static char *time_format_duration_component(char *cursor, char *end,
    uint64_t value, char suffix)
{
    cursor = std::to_chars(cursor, end, value).ptr;
    *cursor++ = suffix;
    return (cursor);
}

ft_string *time_format_duration_string(t_duration_milliseconds duration_value,
    ft_bool relative)
{
    constexpr uint64_t MILLISECONDS_PER_SECOND = 1000U;
    constexpr uint64_t MILLISECONDS_PER_MINUTE = 60U * MILLISECONDS_PER_SECOND;
    constexpr uint64_t MILLISECONDS_PER_HOUR = 60U * MILLISECONDS_PER_MINUTE;
    constexpr uint64_t MILLISECONDS_PER_DAY = 24U * MILLISECONDS_PER_HOUR;
    // Even an int64_t magnitude has at most twelve day digits.
    char buffer[40];
    char *cursor = buffer;
    char *end = buffer + sizeof(buffer) - 1U;
    const int64_t milliseconds = duration_value.milliseconds;
    uint64_t magnitude = static_cast<uint64_t>(milliseconds);
    if (milliseconds < 0)
    {
        *cursor++ = '-';
        magnitude = 0U - magnitude;
    }
    else if (relative == FT_TRUE)
    {
        ft_memcpy(cursor, "in ", 3U);
        cursor += 3U;
    }
    *cursor++ = 'P';
    const uint64_t days = magnitude / MILLISECONDS_PER_DAY;
    const uint64_t hours = (magnitude / MILLISECONDS_PER_HOUR) % 24U;
    const uint64_t minutes = (magnitude / MILLISECONDS_PER_MINUTE) % 60U;
    const uint64_t seconds = (magnitude / MILLISECONDS_PER_SECOND) % 60U;
    const uint64_t fraction = magnitude % MILLISECONDS_PER_SECOND;
    if (days != 0U)
        cursor = time_format_duration_component(cursor, end, days, 'D');
    if (hours != 0U || minutes != 0U || seconds != 0U || fraction != 0U || days == 0U)
    {
        *cursor++ = 'T';
        if (hours != 0U)
            cursor = time_format_duration_component(cursor, end, hours, 'H');
        if (minutes != 0U)
            cursor = time_format_duration_component(cursor, end, minutes, 'M');
        if (seconds != 0U || fraction != 0U || (days == 0U && hours == 0U && minutes == 0U))
        {
            cursor = std::to_chars(cursor, end, seconds).ptr;
            if (fraction != 0U)
            {
                *cursor++ = '.';
                *cursor++ = static_cast<char>('0' + fraction / 100U);
                *cursor++ = static_cast<char>('0' + (fraction / 10U) % 10U);
                *cursor++ = static_cast<char>('0' + fraction % 10U);
            }
            *cursor++ = 'S';
        }
    }
    *cursor = '\0';
    return (time_format_make_string(buffer));
}

ft_string *time_format_duration(t_duration_milliseconds duration_value)
{
    return (time_format_duration_string(duration_value, FT_FALSE));
}

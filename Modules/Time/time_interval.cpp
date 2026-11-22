#include "time_format_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno.hpp"
#include <limits>
#include <memory>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool time_interval_is_duration(const char *string_input)
{
    ft_size_t index;

    if (!string_input)
        return (FT_FALSE);
    index = 0;
    if (string_input[index] == '+' || string_input[index] == '-')
        index += 1;
    if (string_input[index] != 'P' && string_input[index] != 'p')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool time_interval_duration_has_whole_seconds(t_duration_milliseconds duration_value)
{
    if (duration_value.milliseconds % 1000 != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool time_interval_add_seconds(t_time base_time, int64_t seconds, t_time *out_time)
{
    int64_t result_seconds;

    if (!out_time)
        return (FT_FALSE);
    if (seconds > 0 && base_time > std::numeric_limits<t_time>::max() - seconds)
        return (FT_FALSE);
    if (seconds < 0)
    {
        if (seconds == std::numeric_limits<int64_t>::min())
        {
            if (base_time < static_cast<t_time>(0))
                return (FT_FALSE);
        }
        else if (base_time < std::numeric_limits<t_time>::min() - seconds)
            return (FT_FALSE);
    }
    result_seconds = base_time + seconds;
    *out_time = result_seconds;
    return (FT_TRUE);
}

ft_string *time_format_interval(t_time start_time, t_time end_time)
{
    char start_buffer[21];
    char end_buffer[21];
    const ft_size_t start_length = time_format_rfc3339_buffer(start_time, start_buffer);
    if (start_length == 0U)
        return (ft_nullptr);
    const ft_size_t end_length = time_format_rfc3339_buffer(end_time, end_buffer);
    if (end_length == 0U)
        return (ft_nullptr);
    char buffer[42];
    ft_memcpy(buffer, start_buffer, start_length);
    buffer[start_length] = '/';
    ft_memcpy(buffer + start_length + 1U, end_buffer, end_length + 1U);
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

ft_bool    time_parse_interval(const char *string_input, t_time *start_time, t_time *end_time, t_duration_milliseconds *duration_output)
{
    const char *separator;
    const char *left_text;
    const char *right_text;
    ft_size_t left_text_length;
    t_duration_milliseconds interval_duration;
    t_time left_timestamp;
    t_time right_timestamp;
    t_time computed_start;
    t_time computed_end;
    std::tm parsed_time;
    ft_bool left_is_duration;
    ft_bool right_is_duration;
    char left_buffer[64];
    std::unique_ptr<char[]> long_left_buffer;

    if (!string_input || !start_time || !end_time)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    separator = ft_strchr(string_input, '/');
    if (!separator)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    left_text = string_input;
    right_text = separator + 1;
    if (*left_text == '\0' || *right_text == '\0')
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    left_is_duration = time_interval_is_duration(left_text);
    right_is_duration = time_interval_is_duration(right_text);
    if (left_is_duration && right_is_duration)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    left_text_length = static_cast<ft_size_t>(separator - left_text);
    char *left_text_copy = left_buffer;
    if (left_text_length >= sizeof(left_buffer))
    {
        long_left_buffer.reset(new (std::nothrow) char[left_text_length + 1U]);
        if (!long_left_buffer)
            return (FT_FALSE);
        left_text_copy = long_left_buffer.get();
    }
    ft_memcpy(left_text_copy, left_text, left_text_length);
    left_text_copy[left_text_length] = '\0';
    if (left_is_duration)
    {
        if (!time_parse_duration(left_text_copy, &interval_duration))
        {
            return (FT_FALSE);
        }
        if (!time_interval_duration_has_whole_seconds(interval_duration))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        if (!time_parse_rfc3339(right_text, &parsed_time, &right_timestamp))
        {
            return (FT_FALSE);
        }
        if (!time_interval_add_seconds(right_timestamp,
                -(interval_duration.milliseconds / 1000),
                &computed_start))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        computed_end = right_timestamp;
        *start_time = computed_start;
        *end_time = computed_end;
        if (duration_output)
            *duration_output = interval_duration;
        (void)(FT_ERR_SUCCESS);
        return (FT_TRUE);
    }
    if (right_is_duration)
    {
        if (!time_parse_rfc3339(left_text_copy, &parsed_time, &left_timestamp))
        {
            return (FT_FALSE);
        }
        if (!time_parse_duration(right_text, &interval_duration))
        {
            return (FT_FALSE);
        }
        if (!time_interval_duration_has_whole_seconds(interval_duration))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        if (!time_interval_add_seconds(left_timestamp,
                interval_duration.milliseconds / 1000,
                &computed_end))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        computed_start = left_timestamp;
        *start_time = computed_start;
        *end_time = computed_end;
        if (duration_output)
            *duration_output = interval_duration;
        (void)(FT_ERR_SUCCESS);
        return (FT_TRUE);
    }
    if (!time_parse_rfc3339(left_text_copy, &parsed_time, &left_timestamp))
    {
        return (FT_FALSE);
    }
    if (!time_parse_rfc3339(right_text, &parsed_time, &right_timestamp))
    {
        return (FT_FALSE);
    }
    computed_start = left_timestamp;
    computed_end = right_timestamp;
    *start_time = computed_start;
    *end_time = computed_end;
    if (duration_output)
    {
        int64_t difference_seconds;
        int64_t difference_milliseconds;

        difference_seconds = computed_end - computed_start;
        if (difference_seconds > 0
            && difference_seconds > (std::numeric_limits<int64_t>::max() / 1000))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        if (difference_seconds < 0
            && difference_seconds < (std::numeric_limits<int64_t>::min() / 1000))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        difference_milliseconds = difference_seconds * 1000;
        duration_output->mutex = ft_nullptr;
        duration_output->thread_safe_enabled = FT_FALSE;
        duration_output->milliseconds = difference_milliseconds;
    }
    (void)(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

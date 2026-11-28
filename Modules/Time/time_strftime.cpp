#include "time.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <charconv>

static ft_size_t time_format_component(char *destination, ft_size_t available,
    int64_t value, ft_size_t width)
{
    if (width == 2U && value >= 0 && value < 100)
    {
        destination[0] = static_cast<char>('0' + value / 10);
        if (available == 1U)
            return (1U);
        destination[1] = static_cast<char>('0' + value % 10);
        return (2U);
    }
    char digits[20];
    const auto converted = std::to_chars(digits, digits + sizeof(digits), value);
    const ft_size_t digit_count = static_cast<ft_size_t>(converted.ptr - digits);
    ft_size_t written = 0U;
    if (digit_count < width)
    {
        const ft_size_t padding = width - digit_count;
        while (written < padding && written < available)
            destination[written++] = '0';
    }
    ft_size_t copied = digit_count;
    if (copied > available - written)
        copied = available - written;
    if (copied != 0U)
        ft_memcpy(destination + written, digits, copied);
    return (written + copied);
}

ft_size_t time_strftime(char *buffer, ft_size_t size, const char *format,
    const t_time_info *time_info)
{
    if (buffer == ft_nullptr || size == 0U || format == ft_nullptr
        || time_info == ft_nullptr)
        return (0U);
    ft_bool lock_acquired = FT_FALSE;
    if (time_info_lock(time_info, &lock_acquired) != FT_ERR_SUCCESS)
        return (0U);
    ft_size_t format_index = 0U;
    ft_size_t output_index = 0U;
    const ft_size_t capacity = size - 1U;
    while (format[format_index] != '\0' && output_index < capacity)
    {
        if (format[format_index] != '%' || format[format_index + 1U] == '\0')
        {
            buffer[output_index++] = format[format_index++];
            continue ;
        }
        const char conversion = format[++format_index];
        int64_t value;
        ft_size_t width = 2U;
        if (conversion == '%')
        {
            buffer[output_index++] = '%';
            ++format_index;
            continue ;
        }
        if (conversion == 'Y')
        {
            value = static_cast<int64_t>(time_info->year) + 1900;
            width = 4U;
        }
        else if (conversion == 'm')
            value = static_cast<int64_t>(time_info->month) + 1;
        else if (conversion == 'd')
            value = time_info->month_day;
        else if (conversion == 'H')
            value = time_info->hours;
        else if (conversion == 'M')
            value = time_info->minutes;
        else if (conversion == 'S')
            value = time_info->seconds;
        else
            continue ;
        output_index += time_format_component(buffer + output_index,
                capacity - output_index, value, width);
        ++format_index;
    }
    buffer[output_index] = '\0';
    if (lock_acquired == FT_TRUE)
        time_info_unlock(time_info, lock_acquired);
    return (output_index);
}

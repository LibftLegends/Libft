#include "printf.hpp"
#include "printf_internal.hpp"

int32_t pf_printf_fd(int32_t file_descriptor, const char *format, ...)
{
    if (format == ft_nullptr)
        return (-1);
    va_list arguments;
    va_start(arguments, format);
    int32_t result = -1;
    try
    {
        result = pf_printf_fd_v(file_descriptor, format, arguments);
    }
    catch (...)
    {
    }
    va_end(arguments);
    return (result);
}

int32_t pf_printf(const char *format, ...)
{
    if (format == ft_nullptr)
        return (-1);
    va_list arguments;
    va_start(arguments, format);
    int32_t result = -1;
    try
    {
        result = pf_printf_fd_v(1, format, arguments);
    }
    catch (...)
    {
    }
    va_end(arguments);
    return (result);
}

#include "printf.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t pf_snprintf(char *string, ft_size_t size, const char *format, ...)
{
    if (format == ft_nullptr)
    {
        if (string != ft_nullptr && size > 0)
            string[0] = '\0';
        return (-1);
    }
    if (string == ft_nullptr && size > 0)
        return (-1);
    va_list argument_list;
    va_start(argument_list, format);
    int32_t printed;
    try
    {
        printed = pf_vsnprintf(string, size, format, argument_list);
    }
    catch (...)
    {
        if (string != ft_nullptr && size > 0)
            string[0] = '\0';
        printed = -1;
    }
    va_end(argument_list);
    return (printed);
}

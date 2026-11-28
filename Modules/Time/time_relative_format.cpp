#include "time_format_internal.hpp"

ft_string *time_format_relative(t_duration_milliseconds duration_value)
{
    return (time_format_duration_string(duration_value, FT_TRUE));
}

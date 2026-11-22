#ifndef TIME_FORMAT_INTERNAL_HPP
# define TIME_FORMAT_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "time.hpp"

ft_size_t time_format_rfc3339_buffer(t_time time_value, char (&buffer)[21]);
ft_string *time_format_duration_string(t_duration_milliseconds duration_value,
                ft_bool relative);

#endif

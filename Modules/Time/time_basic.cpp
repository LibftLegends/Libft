#include "time.hpp"
#include "../Basic/class_nullptr.hpp"

int64_t ft_time_ms(void)
{
    return (time_now_ms());
}

char *ft_time_format(char *buffer, ft_size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return (ft_nullptr);
    t_time current_time = time_now();
    if (current_time == -1)
        return (ft_nullptr);
    t_time_info time_info{};
    time_local(current_time, &time_info);
    ft_size_t formatted_length = time_strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &time_info);
    if (formatted_length == 0)
        return (ft_nullptr);
    return (buffer);
}

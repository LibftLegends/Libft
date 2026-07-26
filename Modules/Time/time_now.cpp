#include "time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <chrono>
#include <ctime>
#include <cerrno>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

t_time_clock_now_hook g_time_clock_now_hook = ft_nullptr;

void    time_set_clock_now_hook(t_time_clock_now_hook hook)
{
    g_time_clock_now_hook = hook;
    return ;
}

void    time_reset_clock_now_hook(void)
{
    g_time_clock_now_hook = ft_nullptr;
    return ;
}

t_time  time_now(void)
{
    std::chrono::system_clock::time_point current_time;
    std::time_t standard_time;
    int32_t error_code;

    if (g_time_clock_now_hook != ft_nullptr)
    {
        current_time = g_time_clock_now_hook();
        standard_time = std::chrono::duration_cast<std::chrono::seconds>(
            current_time.time_since_epoch()).count();
    }
    else
        standard_time = ::time(ft_nullptr);
    if (standard_time == static_cast<std::time_t>(-1))
    {
        error_code = errno;
        if (error_code == 0)
            error_code = FT_ERR_TERMINATED;
        (void)(error_code);
        return (-1);
    }
    return (standard_time);
}

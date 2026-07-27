#include "time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <chrono>
#include <ctime>
#include <cerrno>
#include <limits>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local t_time_clock_now_hook g_time_clock_now_hook = ft_nullptr;

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
    int64_t seconds_count;
    int32_t error_code;

    if (g_time_clock_now_hook != ft_nullptr)
    {
        current_time = g_time_clock_now_hook();
        seconds_count = std::chrono::duration_cast<std::chrono::seconds>(
            current_time.time_since_epoch()).count();
        if (std::numeric_limits<std::time_t>::is_signed == FT_TRUE
            && seconds_count < static_cast<int64_t>(
                std::numeric_limits<std::time_t>::min()))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (-1);
        }
        if (std::numeric_limits<std::time_t>::is_signed == FT_FALSE
            && seconds_count < 0)
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (-1);
        }
        if (seconds_count >= 0
            && static_cast<uint64_t>(seconds_count)
                > static_cast<uint64_t>(std::numeric_limits<std::time_t>::max()))
        {
            (void)(FT_ERR_OUT_OF_RANGE);
            return (-1);
        }
        standard_time = static_cast<std::time_t>(seconds_count);
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

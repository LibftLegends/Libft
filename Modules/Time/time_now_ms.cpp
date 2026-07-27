#include "time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <chrono>
#include <climits>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

extern thread_local t_time_clock_now_hook g_time_clock_now_hook;

int64_t    time_now_ms(void)
{
    std::chrono::system_clock::time_point time_now;
    std::chrono::milliseconds milliseconds;
    int64_t milliseconds_count;

    if (g_time_clock_now_hook != ft_nullptr)
        time_now = g_time_clock_now_hook();
    else
        time_now = std::chrono::system_clock::now();
    milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    milliseconds_count = milliseconds.count();
    if (milliseconds_count > LONG_MAX)
    {
        (void)(FT_ERR_OUT_OF_RANGE);
        return (LONG_MAX);
    }
    if (milliseconds_count < LONG_MIN)
    {
        (void)(FT_ERR_OUT_OF_RANGE);
        return (LONG_MIN);
    }
    (void)(FT_ERR_SUCCESS);
    return (milliseconds_count);
}

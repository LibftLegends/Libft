#include "time.hpp"
#include "../Basic/class_nullptr.hpp"

#ifdef LIBFT_TEST_BUILD
extern thread_local t_time_clock_now_hook g_time_clock_now_hook;
#endif

int64_t time_now_ms(void)
{
    std::chrono::system_clock::time_point current_time;
#ifdef LIBFT_TEST_BUILD
    if (g_time_clock_now_hook != ft_nullptr)
        current_time = g_time_clock_now_hook();
    else
#endif
        current_time = std::chrono::system_clock::now();
    return (std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time.time_since_epoch()).count());
}

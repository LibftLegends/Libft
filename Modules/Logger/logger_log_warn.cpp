#include "logger_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

void ft_log_warn(const char *format_string, ...)
{
    va_list argument_list;
    va_start(argument_list, format_string);
    if (g_async_running.load(std::memory_order_relaxed) != FT_FALSE)
        logger_enqueue_running(LOG_LEVEL_WARN, format_string, argument_list);
    else
        ft_log_vwrite(LOG_LEVEL_WARN, format_string, argument_list);
    va_end(argument_list);
}

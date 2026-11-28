#include "logger_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

ft_bool ft_log_get_color()
{
    return (g_use_color.load(std::memory_order_relaxed));
}

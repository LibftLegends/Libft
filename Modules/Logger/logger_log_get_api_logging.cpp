#include "logger_internal.hpp"

ft_bool ft_log_get_api_logging()
{
    return (logger_get_global_logging(FT_FALSE));
}

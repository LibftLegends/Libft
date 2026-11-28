#include "logger_internal.hpp"

void ft_log_set_api_logging(ft_bool enable)
{
    logger_set_global_logging(FT_FALSE, enable);
    return ;
}

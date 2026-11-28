#include "logger_internal.hpp"
#include "../Errno/errno.hpp"

int32_t logger_configure_rotation(ft_size_t max_size, ft_size_t retention_count,
    uint32_t max_age_seconds)
{
    const int32_t lock_error = logger_lock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const ft_size_t sink_count = g_sinks.size();
    ft_size_t index = 0U;
    int32_t error = FT_ERR_NOT_FOUND;
    while (index < sink_count)
    {
        const s_log_sink &entry = g_sinks[index];
        if (entry.function == ft_file_sink && entry.user_data != ft_nullptr)
        {
            auto *sink = static_cast<s_file_sink *>(entry.user_data);
            ft_bool acquired = FT_FALSE;
            error = file_sink_lock(sink, &acquired);
            if (error != FT_ERR_SUCCESS)
                break ;
            sink->max_size = max_size;
            sink->retention_count = retention_count;
            sink->max_age_seconds = max_age_seconds;
            file_sink_unlock(sink, acquired);
        }
        index += 1U;
    }
    (void)logger_unlock_sinks();
    return (error);
}

int32_t logger_read_rotation(ft_size_t *max_size, ft_size_t *retention_count,
    uint32_t *max_age_seconds)
{
    if (max_size == ft_nullptr || retention_count == ft_nullptr
        || max_age_seconds == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t lock_error = logger_lock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const ft_size_t sink_count = g_sinks.size();
    ft_size_t index = 0U;
    int32_t error = FT_ERR_NOT_FOUND;
    while (index < sink_count)
    {
        const s_log_sink &entry = g_sinks[index];
        if (entry.function == ft_file_sink && entry.user_data != ft_nullptr)
        {
            auto *sink = static_cast<s_file_sink *>(entry.user_data);
            ft_bool acquired = FT_FALSE;
            error = file_sink_lock(sink, &acquired);
            if (error == FT_ERR_SUCCESS)
            {
                *max_size = sink->max_size;
                *retention_count = sink->retention_count;
                *max_age_seconds = sink->max_age_seconds;
                file_sink_unlock(sink, acquired);
            }
            break ;
        }
        index += 1U;
    }
    (void)logger_unlock_sinks();
    return (error);
}

int32_t ft_log_set_rotation(ft_size_t max_size, ft_size_t retention_count,
    uint32_t max_age_seconds)
{
    return (-static_cast<int32_t>(logger_configure_rotation(max_size, retention_count,
        max_age_seconds) != FT_ERR_SUCCESS));
}

int32_t ft_log_get_rotation(ft_size_t *max_size, ft_size_t *retention_count,
    uint32_t *max_age_seconds)
{
    return (-static_cast<int32_t>(logger_read_rotation(max_size, retention_count,
        max_age_seconds) != FT_ERR_SUCCESS));
}

#include "logger_internal.hpp"
#include "../Compatebility/compatebility_internal.hpp"

static uint64_t g_syslog_identity = 0U;

static s_log_sink_registration *logger_find_syslog_registration()
{
    if (g_syslog_identity == 0U)
        return (ft_nullptr);
    const ft_size_t count = g_sinks.size();
    ft_size_t index = 0U;
    while (index < count)
    {
        s_log_sink_registration *registration = g_sinks[index].registration;
        if (registration->identity == g_syslog_identity)
            return (registration);
        index += 1U;
    }
    return (ft_nullptr);
}

int32_t logger_configure_syslog(const char *identifier)
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    s_log_sink_registration *registration = logger_find_syslog_registration();
    int32_t error;
    if (registration != ft_nullptr)
        error = cmp_syslog_open(identifier);
    else
    {
        error = logger_add_sink_locked(ft_syslog_sink, ft_nullptr, &registration);
        if (error == FT_ERR_SUCCESS)
        {
            error = cmp_syslog_open(identifier);
            if (error == FT_ERR_SUCCESS)
            {
                registration->close_output = FT_TRUE;
                g_syslog_identity = registration->identity;
            }
            else
            {
                s_log_sink entry = g_sinks[g_sinks.size() - 1U];
                g_sinks.pop_back();
                logger_release_sink(entry);
            }
        }
    }
    (void)logger_unlock_sinks();
    return (error);
}

int32_t ft_log_set_syslog(const char *identifier)
{
    if (logger_configure_syslog(identifier) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

void logger_close_syslog_output(uint64_t identity)
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return ;
    if (g_syslog_identity == identity)
    {
        cmp_syslog_close();
        g_syslog_identity = 0U;
    }
    (void)logger_unlock_sinks();
    return ;
}

int32_t ft_syslog_sink(const char *message, void *user_data)
{
    (void)user_data;
    if (message == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    cmp_syslog_write(message);
    return (FT_ERR_SUCCESS);
}

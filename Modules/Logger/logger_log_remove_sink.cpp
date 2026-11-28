#include "logger_internal.hpp"

void logger_release_sink(s_log_sink &entry)
{
    s_log_sink_registration *registration = entry.registration;
    if (registration->references.fetch_sub(1U, std::memory_order_acq_rel) == 1U)
    {
        if (registration->close_output != FT_FALSE)
            logger_close_sink_output(entry);
        log_sink_teardown_thread_safety(&entry);
        delete registration;
    }
    return ;
}

void logger_retire_sink(s_log_sink &entry)
{
    logger_wait_for_sink_callback(*entry.registration);
    logger_release_sink(entry);
    return ;
}

void logger_unlist_sink(s_log_sink &entry)
{
    auto &state = entry.registration->state;
    t_log_sink_registration_state previous = state.load(std::memory_order_relaxed);
    t_log_sink_registration_state retired;
    do
    {
        retired = LOG_SINK_RETIRED;
        if (previous == LOG_SINK_RUNNING)
            retired = LOG_SINK_RETIRED_RUNNING;
    }
    while (!state.compare_exchange_weak(previous, retired, std::memory_order_acq_rel));
    return ;
}

void ft_log_remove_sink(t_log_sink sink, void *user_data)
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return ;
    const ft_size_t count = g_sinks.size();
    ft_size_t index = 0U;
    while (index < count)
    {
        s_log_sink entry = g_sinks[index];
        if (entry.function == sink && entry.user_data == user_data)
        {
            g_sinks.erase(g_sinks.begin() + index);
            const int32_t error = g_sinks.get_error();
            if (error == FT_ERR_SUCCESS)
                logger_unlist_sink(entry);
            (void)logger_unlock_sinks();
            if (error == FT_ERR_SUCCESS)
                logger_retire_sink(entry);
            return ;
        }
        index += 1U;
    }
    (void)logger_unlock_sinks();
    return ;
}

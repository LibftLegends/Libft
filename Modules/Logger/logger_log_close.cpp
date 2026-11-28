#include "logger_internal.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Networking/networking.hpp"

static std::atomic<uint32_t> g_runtime_closures{0U};

ft_bool logger_runtime_closing()
{
    return (g_runtime_closures.load(std::memory_order_acquire) != 0U);
}

int32_t logger_begin_runtime_close(uint64_t &last_sink_identity)
{
    const int32_t error = logger_lock_sinks();
    if (error != FT_ERR_SUCCESS)
        return (error);
    g_runtime_closures.fetch_add(1U, std::memory_order_release);
    const ft_size_t count = g_sinks.size();
    last_sink_identity = 0U;
    // The registry assigns increasing identities under the same lock used for insertion.
    if (count != 0U)
        last_sink_identity = g_sinks[count - 1U].registration->identity;
    (void)logger_unlock_sinks();
    return (FT_ERR_SUCCESS);
}

void logger_close_sink_output(const s_log_sink &entry)
{
    if (entry.function == ft_file_sink)
    {
        auto *sink = static_cast<s_file_sink *>(entry.user_data);
        if (sink != ft_nullptr)
        {
            (void)cmp_close(sink->file_descriptor);
            file_sink_teardown_thread_safety(sink);
            delete sink;
        }
    }
    else if (entry.function == ft_network_sink)
    {
        auto *sink = static_cast<s_network_sink *>(entry.user_data);
        if (sink != ft_nullptr)
        {
            if (sink->socket_fd >= 0)
                (void)nw_close(sink->socket_fd);
            network_sink_teardown_thread_safety(sink);
            delete sink;
        }
    }
    else if (entry.function == ft_syslog_sink)
        logger_close_syslog_output(entry.registration->identity);
    return ;
}

static void logger_retire_sinks_with_new_registrations(uint64_t last_sink_identity)
{
    while (logger_lock_sinks() == FT_ERR_SUCCESS)
    {
        if (g_sinks.empty() != FT_FALSE || g_sinks[0].registration->identity > last_sink_identity)
        {
            (void)logger_unlock_sinks();
            break ;
        }
        s_log_sink entry = g_sinks[0];
        g_sinks.erase(g_sinks.begin());
        const int32_t error = g_sinks.get_error();
        if (error == FT_ERR_SUCCESS)
        {
            entry.registration->close_output = FT_TRUE;
            logger_unlist_sink(entry);
        }
        (void)logger_unlock_sinks();
        if (error != FT_ERR_SUCCESS)
            break ;
        logger_retire_sink(entry);
    }
    return ;
}

static void logger_retire_runtime_sinks(uint64_t last_sink_identity)
{
    if (last_sink_identity == 0U)
        return ;
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return ;
    const ft_size_t count = g_sinks.size();
    if (count == 0U)
    {
        (void)logger_unlock_sinks();
        return ;
    }
    if (g_sinks[count - 1U].registration->identity > last_sink_identity)
    {
        (void)logger_unlock_sinks();
        logger_retire_sinks_with_new_registrations(last_sink_identity);
        return ;
    }
    ft_vector<s_log_sink> sinks;
    const int32_t error = sinks.move(g_sinks);
    if (error == FT_ERR_SUCCESS)
    {
        ft_size_t index = 0U;
        while (index < count)
        {
            s_log_sink &entry = sinks[index];
            entry.registration->close_output = FT_TRUE;
            logger_unlist_sink(entry);
            index += 1U;
        }
    }
    (void)logger_unlock_sinks();
    if (error != FT_ERR_SUCCESS)
        return ;
    ft_size_t index = 0U;
    while (index < count)
    {
        logger_retire_sink(sinks[index]);
        index += 1U;
    }
    return ;
}

void logger_finish_runtime_close(uint64_t last_sink_identity)
{
    ft_log_clear_redactions();
    logger_context_clear();
    g_level.store(LOG_LEVEL_INFO, std::memory_order_relaxed);
    g_use_color.store(FT_TRUE, std::memory_order_relaxed);
    ft_log_enable_async(FT_FALSE);
    ft_log_enable_remote_health(FT_FALSE);
    logger_retire_runtime_sinks(last_sink_identity);
    g_runtime_closures.fetch_sub(1U, std::memory_order_release);
    return ;
}

void ft_log_close()
{
    uint64_t last_sink_identity;
    if (logger_begin_runtime_close(last_sink_identity) == FT_ERR_SUCCESS)
        logger_finish_runtime_close(last_sink_identity);
    return ;
}

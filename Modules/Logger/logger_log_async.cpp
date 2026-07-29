#include "logger_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Sink/sink.hpp"
#include "../Template/queue.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/condition.hpp"
#include "../Printf/printf.hpp"
#include <unistd.h>
#include <cerrno>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

ft_bool g_async_running = FT_FALSE;
static ft_size_t g_async_queue_limit = 1024;
static ft_size_t g_async_pending_messages = 0;
static ft_size_t g_async_peak_pending = 0;
static ft_size_t g_async_dropped_messages = 0;
static ft_queue<ft_string *> g_log_queue;
static int32_t log_queue_initialize(void);
static int32_t g_log_queue_initializer_result = log_queue_initialize();
static int32_t log_queue_initialize(void)
{
    return (g_log_queue.initialize());
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static pthread_mutex_t g_condition_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_queue_condition = PTHREAD_COND_INITIALIZER;
#pragma GCC diagnostic pop
static pthread_t g_log_thread;

static void ft_log_delete_message(ft_string *message) noexcept
{
    if (message == ft_nullptr)
        return ;
    (void)message->destroy();
    delete message;
    return ;
}

void ft_log_process_message(const ft_string &message)
{
    ft_size_t sink_count;
    ft_vector<s_log_sink> sinks_snapshot;
    int32_t    lock_error;
    int32_t    snapshot_initialize_error;

    snapshot_initialize_error = sinks_snapshot.initialize();
    if (snapshot_initialize_error != FT_ERR_SUCCESS)
        return ;
    lock_error = logger_lock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    sink_count = g_sinks.size();
    if (g_sinks.get_error() != FT_ERR_SUCCESS)
    {
        logger_unlock_sinks();
        return ;
    }
    if (sink_count == 0)
    {
        logger_unlock_sinks();
        ssize_t write_result;
#if defined(_WIN32) || defined(_WIN64)
        write_result = write(1, message.c_str(),
                static_cast<unsigned int>(message.size()));
#else
        write_result = write(1, message.c_str(), message.size());
#endif
        (void)write_result;
        return ;
    }
    ft_size_t entry_index;
    entry_index = 0;
    while (entry_index < sink_count)
    {
        s_log_sink entry;
        entry = g_sinks[entry_index];
        if (g_sinks.get_error() != FT_ERR_SUCCESS)
        {
            logger_unlock_sinks();
            return ;
        }
        sinks_snapshot.push_back(entry);
        if (sinks_snapshot.get_error() != FT_ERR_SUCCESS)
        {
            logger_unlock_sinks();
            return ;
        }
        entry_index++;
    }
    logger_unlock_sinks();
    entry_index = 0;
    while (entry_index < sink_count)
    {
        s_log_sink entry;

        entry = sinks_snapshot[entry_index];
        if (sinks_snapshot.get_error() != FT_ERR_SUCCESS)
            return ;
        ft_bool sink_lock_acquired;
        int32_t  sink_error;
        int32_t  sink_lock_error;

        sink_lock_acquired = FT_FALSE;
        sink_lock_error = log_sink_lock(&entry, &sink_lock_acquired);
        sink_error = sink_lock_error;
        if (sink_error == FT_ERR_SUCCESS)
        {
            ft_bool        rotate_for_size_pre;
            ft_bool        rotate_for_age_pre;
            s_file_sink *file_sink;

            rotate_for_size_pre = FT_FALSE;
            rotate_for_age_pre = FT_FALSE;
            file_sink = ft_nullptr;
            if (entry.function == ft_file_sink)
            {
                file_sink = static_cast<s_file_sink *>(entry.user_data);
                if (logger_prepare_rotation(file_sink, &rotate_for_size_pre, &rotate_for_age_pre) != 0)
                    sink_error = FT_ERR_INVALID_OPERATION;
            }
            if (sink_error == FT_ERR_SUCCESS)
            {
                sink_error = entry.function(message.c_str(), entry.user_data);
                if (sink_error != FT_ERR_SUCCESS && g_logger != ft_nullptr)
                    g_logger->set_error(sink_error);
            }
            if (sink_error == FT_ERR_SUCCESS && entry.function == ft_file_sink)
            {
                ft_bool rotate_for_size_post;
                ft_bool rotate_for_age_post;

                rotate_for_size_post = FT_FALSE;
                rotate_for_age_post = FT_FALSE;
                if (logger_prepare_rotation(file_sink, &rotate_for_size_post, &rotate_for_age_post) != 0)
                    sink_error = FT_ERR_INVALID_OPERATION;
                else if (rotate_for_size_pre || rotate_for_age_pre || rotate_for_size_post || rotate_for_age_post)
                {
                    logger_execute_rotation(file_sink);
                }
            }
        }
        if (sink_lock_acquired)
            log_sink_unlock(&entry, sink_lock_acquired);
        if (sink_error != FT_ERR_SUCCESS)
            return ;
        entry_index++;
    }
    return ;
}

static void *ft_log_worker(void *argument)
{
    ft_bool queue_is_empty;
    ft_string *message;
    int32_t queue_error;

    (void)argument;
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
        return (ft_nullptr);
    while (1)
    {
        queue_is_empty = g_log_queue.empty();
        if (g_log_queue.get_error() != FT_ERR_SUCCESS)
            break ;
        if (!g_async_running && queue_is_empty)
            break ;
        if (queue_is_empty)
        {
            if (pt_cond_wait(&g_queue_condition, &g_condition_mutex) != 0)
                break ;
        }
        else
        {
            message = g_log_queue.dequeue();
            queue_error = g_log_queue.get_error();
            if (queue_error == FT_ERR_SUCCESS)
            {
                if (g_async_pending_messages > 0)
                    g_async_pending_messages -= 1;
            }
            pthread_mutex_unlock(&g_condition_mutex);
            if (queue_error == FT_ERR_SUCCESS)
            {
                ft_log_process_message(*message);
                ft_log_delete_message(message);
            }
            if (pthread_mutex_lock(&g_condition_mutex) != 0)
                return (ft_nullptr);
        }
    }
    pthread_mutex_unlock(&g_condition_mutex);
    return (ft_nullptr);
}

void ft_log_enable_async(ft_bool enable)
{
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        return ;
    }
    if (enable)
    {
        if (g_async_running)
        {
            pthread_mutex_unlock(&g_condition_mutex);
            return ;
        }
        g_async_running = FT_TRUE;
        g_async_pending_messages = 0;
        g_async_peak_pending = 0;
        g_async_dropped_messages = 0;
        pthread_mutex_unlock(&g_condition_mutex);
        if (pt_thread_create(&g_log_thread, ft_nullptr, ft_log_worker, ft_nullptr) != 0)
        {
            if (pthread_mutex_lock(&g_condition_mutex) != 0)
            {
                return ;
            }
            g_async_running = FT_FALSE;
            g_async_pending_messages = 0;
            pthread_mutex_unlock(&g_condition_mutex);
        }
    }
    else
    {
        if (!g_async_running)
        {
            pthread_mutex_unlock(&g_condition_mutex);
            return ;
        }
        g_async_running = FT_FALSE;
        pthread_mutex_unlock(&g_condition_mutex);
        if (pt_cond_broadcast(&g_queue_condition) != 0)
            return ;
        if (pt_thread_join(g_log_thread, ft_nullptr) != 0)
            return ;
        if (pthread_mutex_lock(&g_condition_mutex) == 0)
        {
            g_async_pending_messages = 0;
            pthread_mutex_unlock(&g_condition_mutex);
        }
    }
    return ;
}

void ft_log_enqueue(t_log_level level, const char *format_string, va_list argument_list)
{
    ft_vector<s_redaction_rule> redaction_snapshot;
    ft_string message_text;
    ft_string context_fragment;
    ft_string final_message;
    int32_t queue_error;
    int32_t signal_result;
    int32_t unlock_status;
    ft_string *queued_message;
    int32_t format_error;
    char message_buffer[1024];
    va_list args_copy;
    int32_t lock_error;
    int32_t redaction_error;
    int32_t redaction_snapshot_initialize_error;
    int32_t context_fragment_initialize_error;
    int32_t final_message_initialize_error;

    if (!format_string)
    {
        return ;
    }
    if (level < g_level)
    {
        return ;
    }
    va_copy(args_copy, argument_list);
    int32_t formatted_length = pf_vsnprintf(message_buffer, sizeof(message_buffer), format_string, args_copy);
    va_end(args_copy);
    format_error = FT_ERR_SUCCESS;
    if (formatted_length < 0)
    {
        if (format_error != FT_ERR_SUCCESS)
        return ;
    }
    if (message_text.initialize(message_buffer) != FT_ERR_SUCCESS)
    {
        return ;
    }
    context_fragment_initialize_error = context_fragment.initialize();
    if (context_fragment_initialize_error != FT_ERR_SUCCESS)
        return ;
    final_message_initialize_error = final_message.initialize();
    if (final_message_initialize_error != FT_ERR_SUCCESS)
        return ;
    redaction_snapshot_initialize_error = redaction_snapshot.initialize();
    if (redaction_snapshot_initialize_error != FT_ERR_SUCCESS)
    {
        return ;
    }
    lock_error = logger_lock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
    {
        return ;
    }
    redaction_error = logger_copy_redaction_rules(redaction_snapshot);
    if (redaction_error != FT_ERR_SUCCESS)
    {
        lock_error = logger_unlock_sinks();
        if (lock_error != FT_ERR_SUCCESS)
        {
            return ;
        }
        return ;
    }
    lock_error = logger_unlock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
    {
        return ;
    }
    redaction_error = logger_apply_redactions(message_text, redaction_snapshot);
    if (redaction_error != FT_ERR_SUCCESS)
    {
        return ;
    }
    if (logger_context_format_flat(context_fragment) != 0)
        return ;
    if (context_fragment.get_error() != FT_ERR_SUCCESS)
    {
        return ;
    }
    if (context_fragment.size() > 0)
    {
        if (logger_apply_redactions(context_fragment, redaction_snapshot) != 0)
            return ;
    }
    if (logger_build_standard_message(level, message_text, context_fragment, final_message) != 0)
        return ;
    if (final_message.get_error() != FT_ERR_SUCCESS)
    {
        return ;
    }
    (void)sink_record_message(static_cast<int32_t>(level), final_message.c_str());
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        return ;
    }
    if (g_async_queue_limit > 0 && g_async_pending_messages >= g_async_queue_limit)
    {
        g_async_dropped_messages += 1;
        if (pthread_mutex_unlock(&g_condition_mutex) != 0)
        {
            return ;
        }
        return ;
    }
    queued_message = new (std::nothrow) ft_string();
    if (queued_message == ft_nullptr)
    {
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return ;
    }
    if (queued_message->initialize(final_message) != FT_ERR_SUCCESS)
    {
        ft_log_delete_message(queued_message);
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return ;
    }
    g_log_queue.enqueue(queued_message);
    queue_error = g_log_queue.get_error();
    if (queue_error != FT_ERR_SUCCESS)
        ft_log_delete_message(queued_message);
    if (queue_error == FT_ERR_SUCCESS)
    {
        g_async_pending_messages += 1;
        if (g_async_pending_messages > g_async_peak_pending)
            g_async_peak_pending = g_async_pending_messages;
    }
    signal_result = 0;
    if (queue_error == FT_ERR_SUCCESS)
        signal_result = pt_cond_signal(&g_queue_condition);
    unlock_status = pthread_mutex_unlock(&g_condition_mutex);
    if (unlock_status != 0)
    {
        return ;
    }
    if (queue_error != FT_ERR_SUCCESS)
    {
        return ;
    }
    if (signal_result != 0)
        return ;
    return ;
}

void ft_log_set_async_queue_limit(ft_size_t limit)
{
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        return ;
    }
    g_async_queue_limit = limit;
    if (g_async_queue_limit > 0)
    {
        ft_bool needs_trim;

        needs_trim = g_async_pending_messages > g_async_queue_limit;
        while (needs_trim)
        {
            ft_string *dropped_message;
            int32_t drop_error;

            dropped_message = g_log_queue.dequeue();
            drop_error = g_log_queue.get_error();
            if (drop_error != FT_ERR_SUCCESS)
            {
                if (pthread_mutex_unlock(&g_condition_mutex) != 0)
                {
                    return ;
                }
                return ;
            }
            ft_log_delete_message(dropped_message);
            if (g_async_pending_messages > 0)
                g_async_pending_messages -= 1;
            g_async_dropped_messages += 1;
            needs_trim = g_async_pending_messages > g_async_queue_limit;
        }
    }
    if (pthread_mutex_unlock(&g_condition_mutex) != 0)
    {
        return ;
    }
    return ;
}

ft_size_t ft_log_get_async_queue_limit()
{
    ft_size_t limit;

    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        return (FT_ERR_SUCCESS);
    }
    limit = g_async_queue_limit;
    if (pthread_mutex_unlock(&g_condition_mutex) != 0)
    {
        return (FT_ERR_SUCCESS);
    }
    return (limit);
}

int32_t ft_log_get_async_metrics(s_log_async_metrics *metrics)
{
    ft_bool metrics_lock_acquired;

    if (!metrics)
    {
        return (FT_ERR_INTERNAL);
    }
    metrics_lock_acquired = FT_FALSE;
    if (log_async_metrics_lock(metrics, &metrics_lock_acquired) != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        log_async_metrics_unlock(metrics, metrics_lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    metrics->pending_messages = g_async_pending_messages;
    metrics->peak_pending_messages = g_async_peak_pending;
    metrics->dropped_messages = g_async_dropped_messages;
    if (pthread_mutex_unlock(&g_condition_mutex) != 0)
    {
        log_async_metrics_unlock(metrics, metrics_lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    log_async_metrics_unlock(metrics, metrics_lock_acquired);
    return (FT_ERR_SUCCESS);
}

void ft_log_reset_async_metrics()
{
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        return ;
    }
    g_async_peak_pending = g_async_pending_messages;
    g_async_dropped_messages = 0;
    if (pthread_mutex_unlock(&g_condition_mutex) != 0)
    {
        return ;
    }
    return ;
}

#ifdef LIBFT_TEST_BUILD
void ft_log_destroy_async_runtime_for_tests(void)
{
    ft_log_enable_async(FT_FALSE);
    (void)g_log_queue.destroy();
    return ;
}
#endif

#include "logger_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Sink/sink.hpp"
#include "../System_utils/system_utils.hpp"
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

std::atomic<ft_bool> g_async_running{FT_FALSE};
static thread_local ft_bool g_is_log_worker = FT_FALSE;
static ft_size_t g_async_queue_limit = 1024;
static ft_size_t g_async_pending_messages = 0;
static ft_size_t g_async_peak_pending = 0;
static ft_size_t g_async_dropped_messages = 0;
struct s_queued_log_message
{
    ft_string text;
    ft_bool structured = FT_FALSE;
};

static ft_queue<s_queued_log_message *> g_log_queue;
static int32_t log_queue_initialize(void);
static int32_t g_log_queue_initializer_result = log_queue_initialize();
static int32_t log_queue_initialize(void)
{
    return (g_log_queue.initialize());
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static pthread_mutex_t g_worker_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_condition_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_retired_sink_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_retired_sink_condition = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_queue_condition = PTHREAD_COND_INITIALIZER;
#pragma GCC diagnostic pop
static pthread_t g_log_thread;
static ft_bool g_log_thread_joinable = FT_FALSE;

static void ft_log_delete_message(s_queued_log_message *message) noexcept
{
    delete message;
    return ;
}

static pthread_once_t g_delivery_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_delivery_mutex;
static int32_t g_delivery_error = FT_ERR_INTERNAL;
static thread_local ft_bool g_in_sink_callback = FT_FALSE;

static void logger_initialize_delivery_mutex()
{
    pthread_mutexattr_t attributes;
    g_delivery_error = pthread_mutexattr_init(&attributes);
    if (g_delivery_error != 0)
        return ;
    g_delivery_error = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    if (g_delivery_error == 0)
        g_delivery_error = pthread_mutex_init(&g_delivery_mutex, &attributes);
    (void)pthread_mutexattr_destroy(&attributes);
    return ;
}

void logger_wait_for_sink_callback(s_log_sink_registration &registration)
{
    if (g_in_sink_callback != FT_FALSE
        || registration.state.load(std::memory_order_acquire) != LOG_SINK_RETIRED_RUNNING)
        return ;
    if (pthread_mutex_lock(&g_retired_sink_mutex) != 0)
        return ;
    while (registration.state.load(std::memory_order_acquire) == LOG_SINK_RETIRED_RUNNING)
    {
        if (pt_cond_wait(&g_retired_sink_condition, &g_retired_sink_mutex) != 0)
            break ;
    }
    (void)pthread_mutex_unlock(&g_retired_sink_mutex);
    return ;
}

static void logger_finish_sink_callback(s_log_sink_registration &registration)
{
    t_log_sink_registration_state state = LOG_SINK_RUNNING;
    if (registration.state.compare_exchange_strong(state, LOG_SINK_READY,
            std::memory_order_release))
        return ;
    if (pthread_mutex_lock(&g_retired_sink_mutex) != 0)
        return ;
    registration.state.store(LOG_SINK_RETIRED, std::memory_order_release);
    (void)pt_cond_broadcast(&g_retired_sink_condition);
    (void)pthread_mutex_unlock(&g_retired_sink_mutex);
    return ;
}

static int32_t logger_call_sink(const s_log_sink &entry, const char *message,
    ft_bool structured)
{
    if (entry.function == ft_json_sink)
        return (logger_write_json_sink(message, entry.user_data, structured));
    if (entry.function != ft_file_sink)
        return (entry.function(message, entry.user_data));
    return (logger_write_rotating_file(static_cast<s_file_sink *>(entry.user_data), message));
}

int32_t logger_deliver_to_sinks(const ft_string &message, ft_bool stop_on_error,
    ft_bool &has_sinks, ft_bool structured)
{
    if (pthread_once(&g_delivery_once, logger_initialize_delivery_mutex) != 0
        || g_delivery_error != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    int32_t error = pthread_mutex_lock(&g_delivery_mutex);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_lock_sinks();
    if (error != FT_ERR_SUCCESS)
    {
        (void)pthread_mutex_unlock(&g_delivery_mutex);
        return (error);
    }
    constexpr ft_size_t INLINE_SINK_COUNT = 8U;
    s_log_sink inline_entries[INLINE_SINK_COUNT];
    const ft_size_t count = g_sinks.size();
    has_sinks = count != 0U;
    s_log_sink *entries = inline_entries;
    if (count > INLINE_SINK_COUNT)
        entries = new (std::nothrow) s_log_sink[count];
    if (entries == ft_nullptr)
    {
        (void)logger_unlock_sinks();
        (void)pthread_mutex_unlock(&g_delivery_mutex);
        return (FT_ERR_NO_MEMORY);
    }
    ft_size_t index = 0U;
    while (index < count)
    {
        entries[index] = g_sinks[index];
        entries[index].registration->references.fetch_add(1U, std::memory_order_relaxed);
        index += 1U;
    }
    (void)logger_unlock_sinks();
    const ft_bool previous_callback = g_in_sink_callback;
    g_in_sink_callback = FT_TRUE;
    const char *text = message.c_str();
    index = 0U;
    int32_t first_error = FT_ERR_SUCCESS;
    while (index < count)
    {
        const s_log_sink &entry = entries[index];
        t_log_sink_registration_state state = LOG_SINK_READY;
        if (entry.registration->state.compare_exchange_strong(state, LOG_SINK_RUNNING,
                std::memory_order_acq_rel))
        {
            error = logger_call_sink(entry, text, structured);
            logger_finish_sink_callback(*entry.registration);
            if (error != FT_ERR_SUCCESS)
            {
                if (first_error == FT_ERR_SUCCESS)
                    first_error = error;
                logger_report_sink_error(error);
                if (stop_on_error != FT_FALSE)
                    break ;
            }
        }
        index += 1U;
    }
    g_in_sink_callback = previous_callback;
    index = 0U;
    while (index < count)
    {
        logger_release_sink(entries[index]);
        index += 1U;
    }
    if (entries != inline_entries)
        delete[] entries;
    (void)pthread_mutex_unlock(&g_delivery_mutex);
    return (first_error);
}

static void logger_process_message(const ft_string &message, ft_bool structured)
{
    ft_bool has_sinks = FT_FALSE;
    if (logger_deliver_to_sinks(message, FT_TRUE, has_sinks, structured) == FT_ERR_SUCCESS
        && has_sinks == FT_FALSE)
        (void)su_write(1, message.c_str(), message.size());
    return ;
}

void ft_log_process_message(const ft_string &message)
{
    logger_process_message(message, FT_FALSE);
    return ;
}

static void *ft_log_worker(void *argument)
{
    ft_bool queue_is_empty;
    s_queued_log_message *message;
    int32_t queue_error;

    (void)argument;
    g_is_log_worker = FT_TRUE;
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
        return (ft_nullptr);
    while (1)
    {
        queue_is_empty = g_log_queue.empty();
        if (g_log_queue.get_error() != FT_ERR_SUCCESS)
            break ;
        if (g_async_running.load(std::memory_order_relaxed) == FT_FALSE && queue_is_empty)
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
                logger_process_message(message->text, message->structured);
                ft_log_delete_message(message);
            }
            if (pthread_mutex_lock(&g_condition_mutex) != 0)
                return (ft_nullptr);
        }
    }
    g_async_running.store(FT_FALSE, std::memory_order_relaxed);
    pthread_mutex_unlock(&g_condition_mutex);
    return (ft_nullptr);
}

static void logger_request_worker_stop()
{
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
        return ;
    g_async_running.store(FT_FALSE, std::memory_order_relaxed);
    (void)pt_cond_broadcast(&g_queue_condition);
    (void)pthread_mutex_unlock(&g_condition_mutex);
    return ;
}

static void logger_update_worker(ft_bool enable)
{
    if (enable != FT_FALSE
        && g_async_running.load(std::memory_order_relaxed) != FT_FALSE)
        return ;
    if (g_log_thread_joinable != FT_FALSE)
    {
        logger_request_worker_stop();
        if (pt_thread_join(g_log_thread, ft_nullptr) != 0)
            return ;
        g_log_thread_joinable = FT_FALSE;
    }
    if (enable == FT_FALSE || pthread_mutex_lock(&g_condition_mutex) != 0)
        return ;
    if (pt_thread_create(&g_log_thread, ft_nullptr, ft_log_worker, ft_nullptr) == 0)
    {
        g_log_thread_joinable = FT_TRUE;
        g_async_peak_pending = g_async_pending_messages;
        g_async_dropped_messages = 0U;
        g_async_running.store(FT_TRUE, std::memory_order_relaxed);
    }
    (void)pthread_mutex_unlock(&g_condition_mutex);
    return ;
}

ft_bool logger_in_delivery_callback()
{
    return (g_is_log_worker != FT_FALSE || g_in_sink_callback != FT_FALSE);
}

void ft_log_enable_async(ft_bool enable)
{
    const ft_bool closing = logger_runtime_closing();
    if (enable != FT_FALSE && closing != FT_FALSE)
        return ;
    if (logger_in_delivery_callback() != FT_FALSE
        || (closing != FT_FALSE && logger_in_health_worker() != FT_FALSE))
    {
        if (enable == FT_FALSE)
            logger_request_worker_stop();
        return ;
    }
    if (pthread_mutex_lock(&g_worker_control_mutex) != 0)
        return ;
    if (enable == FT_FALSE || logger_runtime_closing() == FT_FALSE)
        logger_update_worker(enable);
    (void)pthread_mutex_unlock(&g_worker_control_mutex);
    return ;
}

static int32_t logger_enqueue_formatted(t_log_level level, const char *format_string,
    va_list argument_list, ft_bool require_worker)
{
    if (format_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (level < g_level.load(std::memory_order_relaxed))
        return (FT_ERR_SUCCESS);
    char message_buffer[1024];
    va_list args_copy;
    va_copy(args_copy, argument_list);
    const int32_t formatted_length = pf_vsnprintf(message_buffer, sizeof(message_buffer),
        format_string, args_copy);
    va_end(args_copy);
    if (formatted_length < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string text;
    const int32_t error = text.initialize(message_buffer);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (logger_enqueue_text(level, text, require_worker));
}

void ft_log_enqueue(t_log_level level, const char *format_string, va_list argument_list)
{
    (void)logger_enqueue_formatted(level, format_string, argument_list, FT_FALSE);
    return ;
}

int32_t logger_enqueue_running(t_log_level level, const char *format_string,
    va_list argument_list)
{
    return (logger_enqueue_formatted(level, format_string, argument_list, FT_TRUE));
}

int32_t logger_enqueue_text(t_log_level level, ft_string &text, ft_bool require_worker)
{
    ft_string final_message;
    const int32_t error = logger_prepare_message(level, text, final_message);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (logger_enqueue_prepared(level, final_message, require_worker, FT_FALSE));
}

int32_t logger_enqueue_prepared(t_log_level level, ft_string &final_message,
    ft_bool require_worker, ft_bool structured)
{
    int32_t queue_error;
    s_queued_log_message *queued_message;
    (void)sink_record_message(static_cast<int32_t>(level), final_message.c_str(), final_message.size());
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
        return (FT_ERR_INTERNAL);
    if (require_worker == FT_TRUE
        && g_async_running.load(std::memory_order_relaxed) == FT_FALSE)
    {
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return (logger_deliver_sync(level, final_message, structured));
    }
    if (g_async_queue_limit > 0 && g_async_pending_messages >= g_async_queue_limit)
    {
        g_async_dropped_messages += 1;
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return (FT_ERR_FULL);
    }
    queued_message = new (std::nothrow) s_queued_log_message();
    if (queued_message == ft_nullptr)
    {
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return (FT_ERR_NO_MEMORY);
    }
    queued_message->structured = structured;
    queue_error = queued_message->text.move(final_message);
    if (queue_error != FT_ERR_SUCCESS)
    {
        ft_log_delete_message(queued_message);
        (void)pthread_mutex_unlock(&g_condition_mutex);
        return (queue_error);
    }
    g_log_queue.enqueue(queued_message);
    queue_error = g_log_queue.get_error();
    if (queue_error != FT_ERR_SUCCESS)
    {
        (void)pthread_mutex_unlock(&g_condition_mutex);
        ft_log_delete_message(queued_message);
        return (queue_error);
    }
    g_async_pending_messages += 1U;
    if (g_async_pending_messages > g_async_peak_pending)
        g_async_peak_pending = g_async_pending_messages;
    if (g_async_pending_messages == 1U)
        (void)pt_cond_signal(&g_queue_condition);
    (void)pthread_mutex_unlock(&g_condition_mutex);
    return (FT_ERR_SUCCESS);
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
            s_queued_log_message *dropped_message;
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
    ft_bool metrics_lock_acquired = FT_FALSE;
    if (log_async_metrics_lock(metrics, &metrics_lock_acquired) != 0)
        return (FT_ERR_INTERNAL);
    if (pthread_mutex_lock(&g_condition_mutex) != 0)
    {
        log_async_metrics_unlock(metrics, metrics_lock_acquired);
        return (FT_ERR_INTERNAL);
    }
    const ft_size_t pending_messages = g_async_pending_messages;
    const ft_size_t peak_pending_messages = g_async_peak_pending;
    const ft_size_t dropped_messages = g_async_dropped_messages;
    (void)pthread_mutex_unlock(&g_condition_mutex);
    metrics->pending_messages = pending_messages;
    metrics->peak_pending_messages = peak_pending_messages;
    metrics->dropped_messages = dropped_messages;
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

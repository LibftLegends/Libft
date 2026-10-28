#include "task_scheduler_tracing.hpp"
#include "../PThread/pthread.hpp"
#include "../Errno/errno.hpp"
#include "../Template/vector.hpp"
#include <atomic>
#include <mutex>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_vector<task_scheduler_trace_sink> g_task_scheduler_trace_sinks(4);
static std::mutex g_task_scheduler_trace_mutex;
static std::atomic<unsigned long long> g_task_scheduler_trace_counter(1);
thread_local unsigned long long g_task_scheduler_trace_current = 0;
static thread_local uint32_t g_task_scheduler_trace_last_error = FT_ERR_SUCCESS;

const char *const g_ft_task_trace_label_async = "async_task";
const char *const g_ft_task_trace_label_schedule_once = "scheduled_once";
const char *const g_ft_task_trace_label_schedule_repeat = "scheduled_repeat";

static uint32_t task_scheduler_trace_set_error(uint32_t error)
{
    g_task_scheduler_trace_last_error = error;
    return (error);
}

static int32_t task_scheduler_trace_ensure_runtime(void)
{
    uint8_t state_value;
    int32_t initialize_error;

    state_value = g_task_scheduler_trace_sinks.is_initialised();
    if (state_value == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    initialize_error = g_task_scheduler_trace_sinks.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    return (FT_ERR_SUCCESS);
}

int task_scheduler_register_trace_sink(task_scheduler_trace_sink sink)
{
    bool lock_acquired;
    int32_t runtime_error;

    if (sink == ft_nullptr)
    {
        task_scheduler_trace_set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    runtime_error = task_scheduler_trace_ensure_runtime();
    if (runtime_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(runtime_error));
        return (-1);
    }
    lock_acquired = false;
    g_task_scheduler_trace_mutex.lock();
    lock_acquired = true;
    size_t index;
    size_t count;

    index = 0;
    count = g_task_scheduler_trace_sinks.size();
    while (index < count)
    {
        if (g_task_scheduler_trace_sinks[index] == sink)
        {
            if (lock_acquired)
                g_task_scheduler_trace_mutex.unlock();
            task_scheduler_trace_set_error(FT_ERR_ALREADY_EXISTS);
            return (-1);
        }
        index += 1;
    }
    g_task_scheduler_trace_sinks.push_back(sink);
    int push_error = g_task_scheduler_trace_sinks.get_error();
    if (push_error != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
            g_task_scheduler_trace_mutex.unlock();
        task_scheduler_trace_set_error(push_error);
        return (-1);
    }
    if (lock_acquired)
        g_task_scheduler_trace_mutex.unlock();
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return (0);
}

int task_scheduler_unregister_trace_sink(task_scheduler_trace_sink sink)
{
    bool lock_acquired;
    int32_t runtime_error;

    if (sink == ft_nullptr)
    {
        task_scheduler_trace_set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    runtime_error = task_scheduler_trace_ensure_runtime();
    if (runtime_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(runtime_error));
        return (-1);
    }
    lock_acquired = false;
    g_task_scheduler_trace_mutex.lock();
    lock_acquired = true;
    size_t index;
    size_t count;

    index = 0;
    count = g_task_scheduler_trace_sinks.size();
    while (index < count)
    {
        if (g_task_scheduler_trace_sinks[index] == sink)
        {
            g_task_scheduler_trace_sinks.erase(g_task_scheduler_trace_sinks.begin() + index);
            int erase_error = g_task_scheduler_trace_sinks.get_error();
            if (erase_error != FT_ERR_SUCCESS)
            {
                if (lock_acquired)
                    g_task_scheduler_trace_mutex.unlock();
                task_scheduler_trace_set_error(erase_error);
                return (-1);
            }
            if (lock_acquired)
                g_task_scheduler_trace_mutex.unlock();
            task_scheduler_trace_set_error(FT_ERR_SUCCESS);
            return (0);
        }
        index += 1;
    }
    if (lock_acquired)
        g_task_scheduler_trace_mutex.unlock();
    task_scheduler_trace_set_error(FT_ERR_NOT_FOUND);
    return (-1);
}

#ifdef LIBFT_TEST_BUILD
int32_t task_scheduler_trace_reset_for_tests(void)
{
    int32_t runtime_error;
    int32_t clear_error;
    bool lock_acquired;

    runtime_error = task_scheduler_trace_ensure_runtime();
    if (runtime_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(runtime_error));
        return (runtime_error);
    }
    lock_acquired = false;
    g_task_scheduler_trace_mutex.lock();
    lock_acquired = true;
    g_task_scheduler_trace_sinks.clear();
    clear_error = g_task_scheduler_trace_sinks.get_error();
    if (lock_acquired)
        g_task_scheduler_trace_mutex.unlock();
    g_task_scheduler_trace_current = 0;
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    if (clear_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(clear_error));
        return (clear_error);
    }
    return (FT_ERR_SUCCESS);
}
#endif

#ifdef LIBFT_TEST_BUILD
void task_scheduler_trace_destroy_for_tests(void)
{
    (void)g_task_scheduler_trace_sinks.destroy();
    g_task_scheduler_trace_current = 0;
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return ;
}
#endif

int32_t task_scheduler_trace_get_error(void)
{
    return (static_cast<int32_t>(g_task_scheduler_trace_last_error));
}

const char *task_scheduler_trace_get_error_str(void)
{
    return (ft_strerror(task_scheduler_trace_get_error()));
}

void task_scheduler_trace_emit(const ft_task_trace_event &event)
{
    ft_vector<task_scheduler_trace_sink> sinks_copy;
    bool lock_acquired;
    size_t index;
    size_t count;
    int32_t runtime_error;
    int32_t sinks_copy_initialize_error;

    runtime_error = task_scheduler_trace_ensure_runtime();
    if (runtime_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(runtime_error));
        return ;
    }
    sinks_copy_initialize_error = sinks_copy.initialize();
    if (sinks_copy_initialize_error != FT_ERR_SUCCESS)
    {
        task_scheduler_trace_set_error(static_cast<uint32_t>(sinks_copy_initialize_error));
        return ;
    }
    lock_acquired = false;
    g_task_scheduler_trace_mutex.lock();
    lock_acquired = true;
    index = 0;
    count = g_task_scheduler_trace_sinks.size();
    while (index < count)
    {
        task_scheduler_trace_sink sink_instance;

        sink_instance = g_task_scheduler_trace_sinks[index];
        sinks_copy.push_back(sink_instance);
        int push_error = sinks_copy.get_error();
        if (push_error != FT_ERR_SUCCESS)
        {
            if (lock_acquired)
                g_task_scheduler_trace_mutex.unlock();
            task_scheduler_trace_set_error(push_error);
            return ;
        }
        index += 1;
    }
    if (lock_acquired)
        g_task_scheduler_trace_mutex.unlock();
    size_t call_index;
    size_t call_count;

    call_index = 0;
    call_count = sinks_copy.size();
    while (call_index < call_count)
    {
        task_scheduler_trace_sink sink_instance;

        sink_instance = sinks_copy[call_index];
        if (sink_instance != ft_nullptr)
            sink_instance(event);
        call_index += 1;
    }
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return ;
}

unsigned long long task_scheduler_trace_generate_span_id(void)
{
    unsigned long long next_value;

    next_value = g_task_scheduler_trace_counter.fetch_add(1);
    if (next_value == 0)
        next_value = g_task_scheduler_trace_counter.fetch_add(1);
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return (next_value);
}

unsigned long long task_scheduler_trace_current_span(void)
{
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return (g_task_scheduler_trace_current);
}

unsigned long long task_scheduler_trace_push_span(unsigned long long span_id)
{
    unsigned long long previous_span;

    previous_span = g_task_scheduler_trace_current;
    g_task_scheduler_trace_current = span_id;
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return (previous_span);
}

void task_scheduler_trace_pop_span(unsigned long long previous_span)
{
    g_task_scheduler_trace_current = previous_span;
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return ;
}

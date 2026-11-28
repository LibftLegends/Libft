#include "task_scheduler_tracing.hpp"
#include "../PThread/pthread.hpp"
#include "../Errno/errno.hpp"
#include "../Template/vector.hpp"
#include "../CMA/CMA.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_vector<task_scheduler_trace_sink> g_task_scheduler_trace_sinks(4);
static std::mutex g_task_scheduler_trace_mutex;
static std::atomic<ft_size_t> g_task_scheduler_trace_sink_count(0U);
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

static ft_size_t task_scheduler_trace_sink_count(void)
{
    return (g_task_scheduler_trace_sink_count.load(std::memory_order_acquire));
}

ft_bool task_scheduler_trace_has_sinks(void)
{
    return (task_scheduler_trace_sink_count() != 0U);
}

int task_scheduler_register_trace_sink(task_scheduler_trace_sink sink)
{
    if (sink == ft_nullptr)
    {
        task_scheduler_trace_set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    g_task_scheduler_trace_mutex.lock();
    int32_t error = task_scheduler_trace_ensure_runtime();
    if (error == FT_ERR_SUCCESS)
    {
        const ft_size_t count = g_task_scheduler_trace_sinks.size();
        ft_size_t index = 0U;
        while (index < count && g_task_scheduler_trace_sinks[index] != sink)
            ++index;
        if (index < count)
            error = FT_ERR_ALREADY_EXISTS;
        else
        {
            error = g_task_scheduler_trace_sinks.push_back(sink);
            if (error == FT_ERR_SUCCESS)
                g_task_scheduler_trace_sink_count.store(count + 1U, std::memory_order_release);
        }
    }
    g_task_scheduler_trace_mutex.unlock();
    task_scheduler_trace_set_error(static_cast<uint32_t>(error));
    if (error == FT_ERR_SUCCESS)
        return (0);
    return (-1);
}

int task_scheduler_unregister_trace_sink(task_scheduler_trace_sink sink)
{
    if (sink == ft_nullptr)
    {
        task_scheduler_trace_set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    g_task_scheduler_trace_mutex.lock();
    const ft_size_t count = task_scheduler_trace_sink_count();
    ft_size_t index = 0U;
    while (index < count && g_task_scheduler_trace_sinks[index] != sink)
        ++index;
    int32_t error = FT_ERR_NOT_FOUND;
    if (index < count)
    {
        g_task_scheduler_trace_sinks.erase(g_task_scheduler_trace_sinks.begin() + index);
        error = g_task_scheduler_trace_sinks.get_error();
        if (error == FT_ERR_SUCCESS)
            g_task_scheduler_trace_sink_count.store(count - 1U, std::memory_order_release);
    }
    g_task_scheduler_trace_mutex.unlock();
    task_scheduler_trace_set_error(static_cast<uint32_t>(error));
    if (error == FT_ERR_SUCCESS)
        return (0);
    return (-1);
}

#ifdef LIBFT_TEST_BUILD
int32_t task_scheduler_trace_reset_for_tests(void)
{
    g_task_scheduler_trace_mutex.lock();
    int32_t error = task_scheduler_trace_ensure_runtime();
    if (error == FT_ERR_SUCCESS)
    {
        g_task_scheduler_trace_sinks.clear();
        error = g_task_scheduler_trace_sinks.get_error();
        if (error == FT_ERR_SUCCESS)
            g_task_scheduler_trace_sink_count.store(0U, std::memory_order_release);
    }
    g_task_scheduler_trace_mutex.unlock();
    g_task_scheduler_trace_current = 0;
    task_scheduler_trace_set_error(static_cast<uint32_t>(error));
    return (error);
}

void task_scheduler_trace_destroy_for_tests(void)
{
    g_task_scheduler_trace_mutex.lock();
    (void)g_task_scheduler_trace_sinks.destroy();
    g_task_scheduler_trace_sink_count.store(0U, std::memory_order_release);
    g_task_scheduler_trace_mutex.unlock();
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
    if (task_scheduler_trace_sink_count() == 0U)
    {
        task_scheduler_trace_set_error(FT_ERR_SUCCESS);
        return ;
    }
    constexpr ft_size_t INLINE_SINK_COUNT = 8U;
    task_scheduler_trace_sink inline_sinks[INLINE_SINK_COUNT];
    std::unique_ptr<task_scheduler_trace_sink, decltype(&cma_free)> heap_sinks(
            nullptr, &cma_free);
    task_scheduler_trace_sink *sinks = inline_sinks;
    g_task_scheduler_trace_mutex.lock();
    const ft_size_t count = task_scheduler_trace_sink_count();
    if (count > INLINE_SINK_COUNT)
    {
        heap_sinks.reset(static_cast<task_scheduler_trace_sink *>(
                cma_malloc(count * sizeof(task_scheduler_trace_sink))));
        sinks = heap_sinks.get();
        if (sinks == ft_nullptr)
        {
            g_task_scheduler_trace_mutex.unlock();
            task_scheduler_trace_set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    if (count != 0U)
        ft_memcpy(sinks, g_task_scheduler_trace_sinks.begin(),
            count * sizeof(task_scheduler_trace_sink));
    g_task_scheduler_trace_mutex.unlock();
    ft_size_t index = 0U;
    while (index < count)
        sinks[index++](event);
    task_scheduler_trace_set_error(FT_ERR_SUCCESS);
    return ;
}

unsigned long long task_scheduler_trace_generate_span_id(void)
{
    unsigned long long next_value;

    next_value = g_task_scheduler_trace_counter.fetch_add(1, std::memory_order_relaxed);
    if (next_value == 0)
        next_value = g_task_scheduler_trace_counter.fetch_add(1, std::memory_order_relaxed);
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

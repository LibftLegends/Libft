#include "observability_task_scheduler_bridge.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Template/unordered_map.hpp"
#include "../PThread/mutex.hpp"
#include <pthread.h>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

struct ft_otel_span_state
{
    ft_bool submitted;
    ft_bool started;
    ft_bool timer_thread;
    t_monotonic_time_point submit_timestamp;
    t_monotonic_time_point start_timestamp;
};

static pt_mutex g_observability_bridge_mutex;
static pthread_once_t g_observability_bridge_once = PTHREAD_ONCE_INIT;
static int32_t g_observability_bridge_once_error = FT_ERR_SUCCESS;
static ft_bool g_observability_bridge_initialised = FT_FALSE;
static ft_otel_span_exporter g_observability_bridge_exporter = ft_nullptr;
static ft_unordered_map<uint64_t, ft_otel_span_state> g_observability_span_states;

#ifdef LIBFT_TEST_BUILD
static void observability_task_scheduler_bridge_untrack_runtime_leaks(void)
{
    if (g_observability_bridge_mutex._native_mutex != ft_nullptr)
        (void)cma_untrack_leak(g_observability_bridge_mutex._native_mutex);
    if (g_observability_span_states.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_observability_span_states._data != ft_nullptr)
        (void)cma_untrack_leak(g_observability_span_states._data);
    if (g_observability_span_states.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_observability_span_states._occupied != ft_nullptr)
        (void)cma_untrack_leak(g_observability_span_states._occupied);
    if (g_observability_span_states.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_observability_span_states._mutex != ft_nullptr)
        (void)cma_untrack_leak(g_observability_span_states._mutex);
    return ;
}
#endif

static void observability_task_scheduler_bridge_initialize_once(void)
{
    int32_t initialize_error;

    initialize_error = g_observability_bridge_mutex.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        g_observability_bridge_once_error = initialize_error;
        return ;
    }
    initialize_error = g_observability_span_states.initialize();
    if (initialize_error == FT_ERR_SUCCESS)
        initialize_error = g_observability_span_states.disable_thread_safety();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        g_observability_bridge_once_error = initialize_error;
        return ;
    }
#ifdef LIBFT_TEST_BUILD
    observability_task_scheduler_bridge_untrack_runtime_leaks();
#endif
    g_observability_bridge_once_error = FT_ERR_SUCCESS;
    return ;
}

static int32_t observability_task_scheduler_bridge_ensure_runtime(void)
{
    int32_t once_status;

    once_status = pthread_once(&g_observability_bridge_once,
        observability_task_scheduler_bridge_initialize_once);
    if (once_status != 0)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (g_observability_bridge_once_error != FT_ERR_SUCCESS)
        return (g_observability_bridge_once_error);
    return (FT_ERR_SUCCESS);
}
static void observability_update_span(ft_otel_span_state &state, const ft_task_trace_event &event)
{
    if (event.timer_thread)
        state.timer_thread = FT_TRUE;
    if (event.phase == FT_TASK_TRACE_PHASE_SUBMITTED)
    {
        state.submitted = FT_TRUE;
        state.submit_timestamp = event.timestamp;
    }
    else if (event.phase == FT_TASK_TRACE_PHASE_STARTED)
    {
        state.started = FT_TRUE;
        state.start_timestamp = event.timestamp;
    }
    return ;
}

static void observability_export_span(ft_otel_span_exporter exporter,
    const ft_otel_span_state &state, const ft_task_trace_event &event)
{
    ft_otel_span_metrics metrics{};
    metrics.span_id = event.trace_id;
    metrics.parent_span_id = event.parent_id;
    metrics.label = event.label;
    metrics.submit_timestamp = state.submit_timestamp;
    metrics.start_timestamp = state.start_timestamp;
    metrics.finish_timestamp = event.timestamp;
    if (state.submitted && state.started)
        metrics.queue_duration_ms = time_monotonic_point_diff_ms(state.submit_timestamp, state.start_timestamp);
    if (state.started)
        metrics.execution_duration_ms = time_monotonic_point_diff_ms(state.start_timestamp, event.timestamp);
    if (state.submitted)
        metrics.total_duration_ms = time_monotonic_point_diff_ms(state.submit_timestamp, event.timestamp);
    metrics.thread_id = event.thread_id;
    metrics.queue_depth = event.queue_depth;
    metrics.scheduled_depth = event.scheduled_depth;
    metrics.worker_active_count = event.worker_active_count;
    metrics.worker_idle_count = event.worker_idle_count;
    metrics.cancelled = event.phase == FT_TASK_TRACE_PHASE_CANCELLED;
    metrics.timer_thread = state.timer_thread || event.timer_thread;
    exporter(metrics);
    return ;
}

static void observability_task_scheduler_bridge_trace_sink(const ft_task_trace_event &event)
{
    // The sink can be registered only after the bridge mutex and state map are initialized.
    if (g_observability_bridge_mutex.lock() != FT_ERR_SUCCESS)
        return ;
    if (!g_observability_bridge_initialised)
    {
        (void)g_observability_bridge_mutex.unlock();
        return ;
    }

    if (event.phase == FT_TASK_TRACE_PHASE_FINISHED || event.phase == FT_TASK_TRACE_PHASE_CANCELLED)
    {
        ft_otel_span_state state{};
        const int32_t result = g_observability_span_states.erase_and_copy(event.trace_id, state);
        const ft_otel_span_exporter exporter = g_observability_bridge_exporter;
        if (g_observability_bridge_mutex.unlock() == FT_ERR_SUCCESS
            && (result == FT_ERR_SUCCESS || result == FT_ERR_NOT_FOUND))
            observability_export_span(exporter, state, event);
        return ;
    }

    const auto iterator = g_observability_span_states.find(event.trace_id);
    if (g_observability_span_states.get_error() == FT_ERR_SUCCESS)
    {
        if (iterator != g_observability_span_states.end())
            observability_update_span(iterator->second, event);
        else
        {
            ft_otel_span_state state{};
            observability_update_span(state, event);
            g_observability_span_states.insert(event.trace_id, state);
#ifdef LIBFT_TEST_BUILD
            if (g_observability_span_states.get_error() == FT_ERR_SUCCESS)
                observability_task_scheduler_bridge_untrack_runtime_leaks();
#endif
        }
    }
    (void)g_observability_bridge_mutex.unlock();
    return ;
}

int32_t observability_task_scheduler_bridge_initialize(ft_otel_span_exporter exporter)
{
    if (exporter == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t result = observability_task_scheduler_bridge_ensure_runtime();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (g_observability_bridge_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!g_observability_bridge_initialised)
    {
        g_observability_span_states.clear();
        if (g_observability_span_states.get_error() != FT_ERR_SUCCESS)
            result = FT_ERR_NO_MEMORY;
        else if (task_scheduler_register_trace_sink(&observability_task_scheduler_bridge_trace_sink)
            != FT_ERR_SUCCESS)
            result = FT_ERR_INVALID_OPERATION;
        else
            g_observability_bridge_initialised = FT_TRUE;
    }
    if (result == FT_ERR_SUCCESS)
        g_observability_bridge_exporter = exporter;
    if (g_observability_bridge_mutex.unlock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    return (result);
}

int32_t observability_task_scheduler_bridge_shutdown(void)
{
    int32_t result = observability_task_scheduler_bridge_ensure_runtime();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (g_observability_bridge_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (g_observability_bridge_initialised
        && task_scheduler_unregister_trace_sink(&observability_task_scheduler_bridge_trace_sink)
            != FT_ERR_SUCCESS)
        result = FT_ERR_INVALID_OPERATION;
    else
    {
        g_observability_bridge_initialised = FT_FALSE;
        g_observability_bridge_exporter = ft_nullptr;
        g_observability_span_states.clear();
        if (g_observability_span_states.get_error() != FT_ERR_SUCCESS)
            result = FT_ERR_INTERNAL;
    }
    if (g_observability_bridge_mutex.unlock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    return (result);
}

#ifdef LIBFT_TEST_BUILD
void observability_task_scheduler_bridge_destroy_for_tests(void)
{
    (void)g_observability_span_states.destroy();
    return ;
}
#endif

#include "../test_internal.hpp"
#include "../../Modules/Observability/observability_task_scheduler_bridge.hpp"
#include "../../Modules/Threading/task_scheduler_tracing.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static std::mutex g_span_mutex;
static std::vector<ft_otel_span_metrics> g_recorded_spans;

static void observability_test_exporter(const ft_otel_span_metrics &span)
{
    std::lock_guard<std::mutex> guard(g_span_mutex);
    g_recorded_spans.push_back(span);
    return ;
}

static void observability_test_clear_spans(void)
{
    std::lock_guard<std::mutex> guard(g_span_mutex);
    std::vector<ft_otel_span_metrics>().swap(g_recorded_spans);
    return ;
}

static std::vector<ft_otel_span_metrics> observability_test_snapshot(void)
{
    std::vector<ft_otel_span_metrics> spans;
    std::lock_guard<std::mutex> guard(g_span_mutex);
    size_t index;
    size_t count;

    index = 0;
    count = g_recorded_spans.size();
    while (index < count)
    {
        spans.push_back(g_recorded_spans[index]);
        index += 1;
    }
    return (spans);
}

static void observability_test_wait_for_count(size_t expected_count)
{
    size_t attempt;

    attempt = 0;
    while (attempt < 100)
    {
        std::vector<ft_otel_span_metrics> spans = observability_test_snapshot();
        if (spans.size() >= expected_count)
            return ;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        attempt += 1;
    }
    return ;
}

FT_TEST(test_observability_bridge_invalid_exporter)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, observability_task_scheduler_bridge_initialize(ft_nullptr));
    return (1);
}

FT_TEST(test_observability_bridge_exports_terminal_event_without_prior_state)
{
    observability_test_clear_spans();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_task_scheduler_bridge_initialize(observability_test_exporter));
    ft_task_trace_event event{};
    event.phase = FT_TASK_TRACE_PHASE_CANCELLED;
    event.trace_id = task_scheduler_trace_generate_span_id();
    event.parent_id = 17;
    event.label = g_ft_task_trace_label_schedule_once;
    event.timestamp = time_monotonic_point_create(99);
    event.thread_id = 42;
    event.timer_thread = FT_TRUE;
    task_scheduler_trace_emit(event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_task_scheduler_bridge_shutdown());
    const auto spans = observability_test_snapshot();
    observability_test_clear_spans();
    FT_ASSERT_EQ(1U, spans.size());
    FT_ASSERT_EQ(event.trace_id, spans[0].span_id);
    FT_ASSERT_EQ(event.parent_id, spans[0].parent_span_id);
    FT_ASSERT_EQ(event.thread_id, spans[0].thread_id);
    FT_ASSERT_EQ(0, spans[0].queue_duration_ms);
    FT_ASSERT_EQ(0, spans[0].execution_duration_ms);
    FT_ASSERT_EQ(0, spans[0].total_duration_ms);
    FT_ASSERT_EQ(99, spans[0].finish_timestamp.milliseconds);
    FT_ASSERT_EQ(FT_TRUE, spans[0].cancelled);
    FT_ASSERT_EQ(FT_TRUE, spans[0].timer_thread);
    return (1);
}

FT_TEST(test_observability_bridge_concurrent_initialize_registers_one_sink)
{
    observability_test_clear_spans();
    std::atomic<ft_bool> ready{FT_FALSE};
    int32_t first = FT_ERR_INTERNAL;
    std::thread initializer([&]()
    {
        while (!ready.load(std::memory_order_acquire))
            std::this_thread::yield();
        first = observability_task_scheduler_bridge_initialize(observability_test_exporter);
        return ;
    });
    ready.store(FT_TRUE, std::memory_order_release);
    const int32_t second = observability_task_scheduler_bridge_initialize(observability_test_exporter);
    initializer.join();
    ft_task_trace_event event{};
    event.phase = FT_TASK_TRACE_PHASE_FINISHED;
    event.trace_id = task_scheduler_trace_generate_span_id();
    task_scheduler_trace_emit(event);
    const int32_t shutdown = observability_task_scheduler_bridge_shutdown();
    const auto spans = observability_test_snapshot();
    observability_test_clear_spans();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shutdown);
    FT_ASSERT_EQ(1U, spans.size());
    return (1);
}

FT_TEST(test_observability_bridge_async_span)
{
    ft_task_trace_event submit_event;
    ft_task_trace_event start_event;
    ft_task_trace_event finish_event;
    unsigned long long root_span;
    unsigned long long trace_id;
    t_monotonic_time_point now_point;

    observability_test_clear_spans();
    FT_ASSERT_EQ(0, observability_task_scheduler_bridge_initialize(&observability_test_exporter));
    root_span = task_scheduler_trace_generate_span_id();
    trace_id = task_scheduler_trace_generate_span_id();
    now_point = time_monotonic_point_now();

    submit_event.phase = FT_TASK_TRACE_PHASE_SUBMITTED;
    submit_event.trace_id = trace_id;
    submit_event.parent_id = root_span;
    submit_event.label = g_ft_task_trace_label_async;
    submit_event.timestamp = now_point;
    submit_event.thread_id = 0;
    submit_event.queue_depth = 1;
    submit_event.scheduled_depth = 0;
    submit_event.worker_active_count = 0;
    submit_event.worker_idle_count = 1;
    submit_event.timer_thread = false;
    task_scheduler_trace_emit(submit_event);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    start_event = submit_event;
    start_event.phase = FT_TASK_TRACE_PHASE_STARTED;
    start_event.timestamp = time_monotonic_point_now();
    start_event.thread_id = 42;
    start_event.queue_depth = 0;
    start_event.worker_active_count = 1;
    start_event.worker_idle_count = 0;
    task_scheduler_trace_emit(start_event);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    finish_event = start_event;
    finish_event.phase = FT_TASK_TRACE_PHASE_FINISHED;
    finish_event.timestamp = time_monotonic_point_now();
    task_scheduler_trace_emit(finish_event);

    observability_test_wait_for_count(1);
    std::vector<ft_otel_span_metrics> spans = observability_test_snapshot();
    FT_ASSERT_EQ(static_cast<size_t>(1), spans.size());
    const ft_otel_span_metrics &span = spans[0];
    FT_ASSERT(span.span_id != 0);
    FT_ASSERT_EQ(root_span, span.parent_span_id);
    FT_ASSERT(span.label == g_ft_task_trace_label_async);
    FT_ASSERT(span.queue_duration_ms >= 0);
    FT_ASSERT(span.execution_duration_ms >= 0);
    FT_ASSERT(span.total_duration_ms >= span.execution_duration_ms);
    FT_ASSERT(span.total_duration_ms >= span.queue_duration_ms);
    FT_ASSERT(!span.cancelled);
    FT_ASSERT(!span.timer_thread);
    FT_ASSERT(span.thread_id != 0);
    FT_ASSERT_EQ(0, observability_task_scheduler_bridge_shutdown());
    observability_test_clear_spans();
    return (1);
}

FT_TEST(test_observability_bridge_cancelled_span)
{
    ft_task_trace_event submit_event;
    ft_task_trace_event cancel_event;
    unsigned long long root_span;
    unsigned long long trace_id;

    observability_test_clear_spans();
    FT_ASSERT_EQ(0, observability_task_scheduler_bridge_initialize(&observability_test_exporter));
    root_span = task_scheduler_trace_generate_span_id();
    trace_id = task_scheduler_trace_generate_span_id();

    submit_event.phase = FT_TASK_TRACE_PHASE_SUBMITTED;
    submit_event.trace_id = trace_id;
    submit_event.parent_id = root_span;
    submit_event.label = g_ft_task_trace_label_schedule_once;
    submit_event.timestamp = time_monotonic_point_now();
    submit_event.thread_id = 0;
    submit_event.queue_depth = 0;
    submit_event.scheduled_depth = 1;
    submit_event.worker_active_count = 0;
    submit_event.worker_idle_count = 1;
    submit_event.timer_thread = false;
    task_scheduler_trace_emit(submit_event);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    cancel_event = submit_event;
    cancel_event.phase = FT_TASK_TRACE_PHASE_CANCELLED;
    cancel_event.timestamp = time_monotonic_point_now();
    task_scheduler_trace_emit(cancel_event);

    observability_test_wait_for_count(1);
    std::vector<ft_otel_span_metrics> spans = observability_test_snapshot();
    FT_ASSERT_EQ(static_cast<size_t>(1), spans.size());
    const ft_otel_span_metrics &span = spans[0];
    FT_ASSERT(span.span_id != 0);
    FT_ASSERT_EQ(root_span, span.parent_span_id);
    FT_ASSERT(span.label == g_ft_task_trace_label_schedule_once);
    FT_ASSERT(span.queue_duration_ms >= 0);
    FT_ASSERT_EQ(0, span.execution_duration_ms);
    FT_ASSERT(span.total_duration_ms >= 0);
    FT_ASSERT(span.cancelled);
    FT_ASSERT(!span.timer_thread);
    FT_ASSERT_EQ(0, observability_task_scheduler_bridge_shutdown());
    observability_test_clear_spans();
    return (1);
}

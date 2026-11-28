#include "../../../Modules/Observability/observability_task_scheduler_bridge.hpp"
#include <chrono>
#include <cstdio>

static uint64_t exported_spans;
static int64_t exported_queue_time;
static int64_t exported_execution_time;

static void count_exported_span(const ft_otel_span_metrics &span)
{
    ++exported_spans;
    exported_queue_time += span.queue_duration_ms;
    exported_execution_time += span.execution_duration_ms;
    return ;
}

int main()
{
    constexpr uint32_t BATCHES = 2048;
    constexpr uint32_t SPANS_PER_BATCH = 64;
    constexpr uint32_t SPAN_COUNT = BATCHES * SPANS_PER_BATCH;
    if (observability_task_scheduler_bridge_initialize(count_exported_span) != FT_ERR_SUCCESS)
        return (1);
    ft_task_trace_event event{};
    event.label = "benchmark";
    uint32_t batch = 0;
    const auto start = std::chrono::steady_clock::now();
    while (batch < BATCHES)
    {
        const uint64_t first_id = 1U + static_cast<uint64_t>(batch) * SPANS_PER_BATCH;
        uint32_t span_index = 0;
        event.phase = FT_TASK_TRACE_PHASE_SUBMITTED;
        event.timestamp = time_monotonic_point_create(10);
        while (span_index < SPANS_PER_BATCH)
        {
            event.trace_id = first_id + span_index++;
            task_scheduler_trace_emit(event);
        }
        span_index = 0;
        while (span_index < SPANS_PER_BATCH)
        {
            event.trace_id = first_id + span_index++;
            event.phase = FT_TASK_TRACE_PHASE_STARTED;
            event.timestamp = time_monotonic_point_create(12);
            task_scheduler_trace_emit(event);
            event.phase = FT_TASK_TRACE_PHASE_FINISHED;
            event.timestamp = time_monotonic_point_create(15);
            task_scheduler_trace_emit(event);
        }
        ++batch;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (observability_task_scheduler_bridge_shutdown() != FT_ERR_SUCCESS || exported_spans != SPAN_COUNT
        || exported_queue_time != 2 * SPAN_COUNT || exported_execution_time != 3 * SPAN_COUNT)
        return (1);
    std::printf("%.3f us/span\n", std::chrono::duration<double, std::micro>(elapsed).count() / SPAN_COUNT);
    return (0);
}

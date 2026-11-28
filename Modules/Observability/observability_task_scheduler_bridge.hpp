#ifndef OBSERVABILITY_TASK_SCHEDULER_BRIDGE_HPP
#define OBSERVABILITY_TASK_SCHEDULER_BRIDGE_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Threading/task_scheduler_tracing.hpp"
#include "../Time/time.hpp"
#include "../Errno/errno.hpp"

struct ft_otel_span_metrics
{
    uint64_t span_id;
    uint64_t parent_span_id;
    const char *label;
    t_monotonic_time_point submit_timestamp;
    t_monotonic_time_point start_timestamp;
    t_monotonic_time_point finish_timestamp;
    int64_t queue_duration_ms;
    int64_t execution_duration_ms;
    int64_t total_duration_ms;
    uint64_t thread_id;
    int64_t queue_depth;
    int64_t scheduled_depth;
    int64_t worker_active_count;
    int64_t worker_idle_count;
    ft_bool cancelled;
    ft_bool timer_thread;
};

typedef void (*ft_otel_span_exporter)(const ft_otel_span_metrics &span);

// Exporters run outside the bridge lock. Concurrent tasks may invoke them concurrently.
int32_t observability_task_scheduler_bridge_initialize(ft_otel_span_exporter exporter);
int32_t observability_task_scheduler_bridge_shutdown(void);

#ifdef LIBFT_TEST_BUILD
void observability_task_scheduler_bridge_destroy_for_tests(void);
#endif

#endif

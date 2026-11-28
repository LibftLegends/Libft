#include "game_event_scheduler_telemetry.hpp"
#include "../Observability/observability_game_metrics.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include <limits>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "game_event.hpp"

static constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000;

static void game_event_scheduler_telemetry_emit(const char *event_name,
        const char *attribute,
        int64_t delta_value,
        int64_t total_value,
        const char *unit,
        int32_t error_code,
        ft_bool success,
        const char *entity) noexcept
{
    ft_game_observability_sample sample;

    sample.labels.event_name = event_name;
    sample.labels.entity = entity;
    sample.labels.attribute = attribute;
    sample.delta_value = delta_value;
    sample.total_value = total_value;
    sample.unit = unit;
    sample.error_code = error_code;
    sample.error_tag = ft_nullptr;
    sample.success = success;
    observability_game_metrics_record(sample);
    return ;
}

static void game_event_scheduler_telemetry_update_state(game_event_scheduler_telemetry_state &state,
        const t_event_scheduler_profile &profile) noexcept
{
    state.last_update_count = profile.update_count;
    state.last_events_processed = profile.events_processed;
    state.last_events_rescheduled = profile.events_rescheduled;
    state.last_total_processing_ns = profile.total_processing_ns;
    state.last_max_queue_depth = profile.max_queue_depth;
    state.last_max_ready_batch = profile.max_ready_batch;
    state.last_last_update_ns = profile.last_update_processing_ns;
    return ;
}

void game_event_scheduler_telemetry_state_initialize(game_event_scheduler_telemetry_state &state,
        const char *scheduler_name) noexcept
{
    state.scheduler_name = scheduler_name;
    game_event_scheduler_telemetry_state_reset(state);
    return ;
}

void game_event_scheduler_telemetry_state_reset(game_event_scheduler_telemetry_state &state) noexcept
{
    state.last_update_count = 0;
    state.last_events_processed = 0;
    state.last_events_rescheduled = 0;
    state.last_total_processing_ns = 0;
    state.last_max_queue_depth = 0;
    state.last_max_ready_batch = 0;
    state.last_last_update_ns = 0;
    return ;
}

static int64_t game_event_scheduler_events_per_second(int64_t events,
        int64_t elapsed_ns) noexcept
{
    if (elapsed_ns <= 0 || events <= 0)
        return (0);
    constexpr int64_t MAXIMUM = std::numeric_limits<int64_t>::max();
    if (events <= MAXIMUM / NANOSECONDS_PER_SECOND)
        return (events * NANOSECONDS_PER_SECOND / elapsed_ns);
    const __int128 rate = static_cast<__int128>(events)
        * NANOSECONDS_PER_SECOND / elapsed_ns;
    if (rate > MAXIMUM)
        return (MAXIMUM);
    return (static_cast<int64_t>(rate));
}

void game_event_scheduler_telemetry_record(game_event_scheduler_telemetry_state &state,
        const t_event_scheduler_profile &profile) noexcept
{
    if (state.scheduler_name == ft_nullptr)
        return ;
    if (profile.update_count == state.last_update_count
        && profile.events_processed == state.last_events_processed
        && profile.last_update_processing_ns == state.last_last_update_ns
        && profile.max_queue_depth == state.last_max_queue_depth
        && profile.events_rescheduled == state.last_events_rescheduled
        && profile.total_processing_ns == state.last_total_processing_ns
        && profile.max_ready_batch == state.last_max_ready_batch)
        return ;

    if (profile.update_count < 0 || profile.events_processed < 0
        || profile.events_rescheduled < 0 || profile.total_processing_ns < 0)
        return ;
    if (profile.update_count < state.last_update_count
        || profile.events_processed < state.last_events_processed
        || profile.events_rescheduled < state.last_events_rescheduled
        || profile.total_processing_ns < state.last_total_processing_ns
        || profile.max_queue_depth < state.last_max_queue_depth
        || profile.max_ready_batch < state.last_max_ready_batch)
        game_event_scheduler_telemetry_state_reset(state);

    const int64_t delta_updates = profile.update_count - state.last_update_count;
    const int64_t delta_processed = profile.events_processed - state.last_events_processed;
    if (delta_updates == 0 && delta_processed == 0
        && profile.max_queue_depth == state.last_max_queue_depth
        && profile.last_update_processing_ns == state.last_last_update_ns)
    {
        game_event_scheduler_telemetry_update_state(state, profile);
        return ;
    }

    int64_t delta_processing_ns = profile.total_processing_ns - state.last_total_processing_ns;
    if (delta_updates > 0 && profile.last_update_processing_ns > 0
        && profile.last_update_processing_ns < state.last_last_update_ns)
    {
        constexpr int64_t MAXIMUM = std::numeric_limits<int64_t>::max();
        int64_t projected_processing_ns = MAXIMUM;
        if (delta_updates <= MAXIMUM / profile.last_update_processing_ns)
            projected_processing_ns = delta_updates * profile.last_update_processing_ns;
        if (projected_processing_ns > delta_processing_ns)
            delta_processing_ns = projected_processing_ns;
    }
    int64_t events_per_update = 0;
    if (delta_updates > 0)
        events_per_update = delta_processed / delta_updates;
    const int64_t events_per_second = game_event_scheduler_events_per_second(
        delta_processed, delta_processing_ns);
    int64_t queue_depth_value = std::numeric_limits<int64_t>::max();
    if (profile.max_queue_depth <= static_cast<ft_size_t>(queue_depth_value))
        queue_depth_value = static_cast<int64_t>(profile.max_queue_depth);
    const int64_t latency_value = profile.last_update_processing_ns;
    const ft_bool success = profile.last_error_code == FT_ERR_SUCCESS;
    game_event_scheduler_telemetry_emit("event_scheduler.throughput",
        "events_per_update",
        events_per_update,
        events_per_update,
        "events/update",
        profile.last_error_code,
        success,
        state.scheduler_name);
    game_event_scheduler_telemetry_emit("event_scheduler.throughput",
        "events_per_second",
        events_per_second,
        events_per_second,
        "events/sec",
        profile.last_error_code,
        success,
        state.scheduler_name);
    game_event_scheduler_telemetry_emit("event_scheduler.queue",
        "max_depth",
        queue_depth_value,
        queue_depth_value,
        "events",
        profile.last_error_code,
        success,
        state.scheduler_name);
    game_event_scheduler_telemetry_emit("event_scheduler.latency",
        "last_update_ns",
        latency_value,
        profile.total_processing_ns,
        "nanoseconds",
        profile.last_error_code,
        success,
        state.scheduler_name);
    game_event_scheduler_telemetry_update_state(state, profile);

    return ;
}

void game_event_scheduler_publish_telemetry(game_event_scheduler &scheduler,
        game_event_scheduler_telemetry_state &state) noexcept
{
    t_event_scheduler_profile profile;
    scheduler.snapshot_profile(profile);
    if (scheduler.get_error() != FT_ERR_SUCCESS)
    {

        return ;
    }
    game_event_scheduler_telemetry_record(state, profile);
    return ;
}

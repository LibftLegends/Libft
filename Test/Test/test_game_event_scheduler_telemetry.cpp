#include "../test_internal.hpp"
#include "../../Modules/Game/game_event_scheduler_telemetry.hpp"
#include "../../Modules/Observability/observability_game_metrics.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <limits>

static ft_game_observability_sample g_scheduler_samples[4];
static ft_size_t g_scheduler_sample_count = 0U;

static void capture_scheduler_sample(const ft_game_observability_sample &sample)
{
    g_scheduler_samples[g_scheduler_sample_count % 4U] = sample;
    ++g_scheduler_sample_count;
    return ;
}

FT_TEST(test_game_event_scheduler_telemetry_skips_duplicates_and_tracks_resets)
{
    g_scheduler_sample_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_game_metrics_initialize(capture_scheduler_sample));
    game_event_scheduler_telemetry_state state;
    game_event_scheduler_telemetry_state_initialize(state, "test");
    t_event_scheduler_profile profile{2, 6, 1, 8U, 3U, 100, 80, FT_ERR_SUCCESS};
    game_event_scheduler_telemetry_record(state, profile);
    game_event_scheduler_telemetry_record(state, profile);
    ++profile.events_rescheduled;
    profile.total_processing_ns = 120;
    game_event_scheduler_telemetry_record(state, profile);
    const ft_size_t first_count = g_scheduler_sample_count;
    const int64_t first_rate = g_scheduler_samples[1].delta_value;
    const int64_t last_total = state.last_total_processing_ns;

    profile = {3, 10, 2, 8U, 3U, 160, 20, FT_ERR_SUCCESS};
    game_event_scheduler_telemetry_record(state, profile);
    const int64_t next_rate = g_scheduler_samples[1].delta_value;
    profile = {1, 2, 0, 2U, 2U, 50, 50, FT_ERR_SUCCESS};
    game_event_scheduler_telemetry_record(state, profile);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_shutdown());
    FT_ASSERT_EQ(4U, first_count);
    FT_ASSERT_EQ(60000000, first_rate);
    FT_ASSERT_EQ(120, last_total);
    FT_ASSERT_EQ(100000000, next_rate);
    FT_ASSERT_EQ(12U, g_scheduler_sample_count);
    FT_ASSERT_EQ(2, g_scheduler_samples[0].delta_value);
    FT_ASSERT_EQ(40000000, g_scheduler_samples[1].delta_value);
    FT_ASSERT_EQ(2, g_scheduler_samples[2].delta_value);
    FT_ASSERT_EQ(50, g_scheduler_samples[3].total_value);
    return (1);
}

FT_TEST(test_game_event_scheduler_telemetry_large_counters_keep_the_throughput)
{
    constexpr int64_t MAXIMUM = std::numeric_limits<int64_t>::max();
    g_scheduler_sample_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_game_metrics_initialize(capture_scheduler_sample));
    game_event_scheduler_telemetry_state state;
    game_event_scheduler_telemetry_state_initialize(state, "test");
    t_event_scheduler_profile profile{1, MAXIMUM, 0, 1U, 1U,
        MAXIMUM, 1, FT_ERR_SUCCESS};
    game_event_scheduler_telemetry_record(state, profile);
    const int64_t scaled_rate = g_scheduler_samples[1].delta_value;

    game_event_scheduler_telemetry_state_reset(state);
    profile.total_processing_ns = 1;
    profile.max_queue_depth = std::numeric_limits<ft_size_t>::max();
    game_event_scheduler_telemetry_record(state, profile);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_game_metrics_shutdown());
    FT_ASSERT_EQ(8U, g_scheduler_sample_count);
    FT_ASSERT_EQ(1000000000, scaled_rate);
    FT_ASSERT_EQ(MAXIMUM, g_scheduler_samples[1].delta_value);
    FT_ASSERT_EQ(MAXIMUM, g_scheduler_samples[2].delta_value);
    return (1);
}

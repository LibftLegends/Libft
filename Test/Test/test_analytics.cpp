#include "../test_internal.hpp"
#include "../../Modules/Analytics/analytics.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <chrono>
#include <thread>

static uint32_t g_analytics_exported_frame = 0U;
static uint32_t g_analytics_trace_events = 0U;

static void analytics_test_export_callback(
    const analytics_frame_statistics &frame, void *user_data)
{
    uint32_t *frame_counter;

    frame_counter = static_cast<uint32_t *>(user_data);
    if (frame_counter != ft_nullptr)
        *frame_counter += 1U;
    g_analytics_exported_frame = static_cast<uint32_t>(frame.frame_number);
    return ;
}

static void analytics_test_trace_callback(const analytics_trace_event &event,
    void *user_data)
{
    uint32_t *event_counter;

    (void)event;
    event_counter = static_cast<uint32_t *>(user_data);
    if (event_counter != ft_nullptr)
        *event_counter += 1U;
    g_analytics_trace_events += 1U;
    return ;
}

FT_TEST(test_analytics_session_records_nested_scope_statistics)
{
    analytics_session session;
    analytics_region_statistics statistics;
    uint32_t outer_region;
    uint32_t inner_region;
    uint32_t exported_frames;

    outer_region = 0U;
    inner_region = 0U;
    exported_frames = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("frame.outer",
        "test", &outer_region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("frame.inner",
        "test", &inner_region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_export_callback(
        analytics_test_export_callback, &exported_frames));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, outer_region));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, inner_region));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(1U, exported_frames);
    FT_ASSERT_EQ(42U, g_analytics_exported_frame);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(inner_region,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT(statistics.inclusive_nanoseconds > 0U);
    FT_ASSERT(statistics.exclusive_nanoseconds > 0U);
    FT_ASSERT(statistics.percentile_50_nanoseconds > 0U);
    FT_ASSERT(statistics.percentile_95_nanoseconds >=
        statistics.percentile_50_nanoseconds);
    FT_ASSERT(statistics.percentile_99_nanoseconds >=
        statistics.percentile_95_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(outer_region,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT(statistics.inclusive_nanoseconds >= statistics.exclusive_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_session_can_be_disabled_without_recording)
{
    analytics_session session;
    analytics_region_statistics statistics;
    uint32_t region_id;

    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("disabled", "test",
        &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_enabled(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_begin_frame(&session, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(0U, statistics.invocation_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_cross_thread_flow_exports_trace_event)
{
    analytics_session session;
    analytics_flow_token flow_token;
    uint32_t region_id;
    uint32_t trace_events;

    region_id = 0U;
    trace_events = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("worker.flow",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(
        analytics_test_trace_callback, &trace_events));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_flow(&session, 99U,
        region_id, &flow_token));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_flow(flow_token));
    FT_ASSERT_EQ(1U, trace_events);
    FT_ASSERT_EQ(1U, g_analytics_trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

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
    analytics_frame_statistics latest_frame;

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
    FT_ASSERT_EQ(0U, exported_frames);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(1U, exported_frames);
    FT_ASSERT_EQ(42U, g_analytics_exported_frame);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&latest_frame));
    FT_ASSERT_EQ(2U, latest_frame.breakdown_count);
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
    FT_ASSERT_EQ(0U, trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(1U, trace_events);
    FT_ASSERT_EQ(1U, g_analytics_trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_trace_queue_reports_overflow_and_flushes)
{
    analytics_session session;
    analytics_trace_event event;
    uint32_t trace_events;
    uint32_t event_index;

    trace_events = 0U;
    event = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(
        analytics_test_trace_callback, &trace_events));
    event_index = 0U;
    while (event_index < FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(event));
        event_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_FULL, session.publish_trace(event));
    FT_ASSERT_EQ(1U, session.get_dropped_trace_count());
    FT_ASSERT_EQ(0U, trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS, trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_worker_frame_flushes_to_shared_session)
{
    analytics_session session;
    uint32_t region_id;
    uint64_t timestamp;
    analytics_region_statistics statistics;
    analytics_frame_statistics latest_frame;

    region_id = 0U;
    timestamp = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("worker.scope",
        "worker", &region_id));
    std::thread worker([&session, region_id, &timestamp]() -> int32_t
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 77U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_now_nanoseconds(&timestamp));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_thread_frame(&session));
        return (1);
    });
    worker.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&latest_frame));
    FT_ASSERT_EQ(1U, latest_frame.breakdown_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_session_retains_latest_frame_and_rolling_statistics)
{
    analytics_session session;
    analytics_frame_statistics first_frame;
    analytics_frame_statistics second_frame;
    analytics_frame_statistics latest_frame;

    first_frame.frame_number = 1U;
    first_frame.duration_nanoseconds = 10U;
    first_frame.mean_duration_nanoseconds = 0U;
    first_frame.percentile_95_nanoseconds = 0U;
    first_frame.percentile_99_nanoseconds = 0U;
    first_frame.uninstrumented_nanoseconds = 0U;
    first_frame.completed_scope_count = 0U;
    first_frame.dropped_scope_count = 0U;
    second_frame = first_frame;
    second_frame.frame_number = 2U;
    second_frame.duration_nanoseconds = 30U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_EMPTY, session.get_latest_frame(&latest_frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(first_frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(second_frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&latest_frame));
    FT_ASSERT_EQ(2U, latest_frame.frame_number);
    FT_ASSERT_EQ(20U, latest_frame.mean_duration_nanoseconds);
    FT_ASSERT_EQ(10U, latest_frame.percentile_95_nanoseconds);
    FT_ASSERT_EQ(10U, latest_frame.percentile_99_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_frame_reports_uninstrumented_gap)
{
    analytics_session session;
    analytics_frame_statistics latest_frame;
    uint32_t region_id;

    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("instrumented",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 88U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&latest_frame));
    FT_ASSERT(latest_frame.duration_nanoseconds > 0U);
    FT_ASSERT(latest_frame.uninstrumented_nanoseconds > 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_exporters_are_transactional_and_valid)
{
    analytics_session session;
    analytics_frame_statistics frame;
    analytics_trace_event event;
    ft_string output;
    uint32_t region_id;
    char preserved[256];

    region_id = 0U;
    frame = {};
    event = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("render", "frame",
        &region_id));
    frame.frame_number = 12U;
    frame.duration_nanoseconds = 16000000U;
    frame.breakdown_count = 1U;
    frame.breakdown[0].region_id = region_id;
    frame.breakdown[0].invocation_count = 2U;
    frame.breakdown[0].inclusive_nanoseconds = 800U;
    frame.breakdown[0].exclusive_nanoseconds = 500U;
    event.region_id = region_id;
    event.start_nanoseconds = 1000U;
    event.duration_nanoseconds = 2000U;
    event.thread_id = 7U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("preserved"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_frame_json(session, frame,
        &output));
    FT_ASSERT(ft_str_contains(output.c_str(), "\"frame\":12"));
    FT_ASSERT(ft_str_contains(output.c_str(), "\"region_id\":0"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_trace_json(session, &event,
        1U, &output));
    FT_ASSERT(ft_str_contains(output.c_str(), "\"ph\":\"X\""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_frame_csv(session, frame,
        &output));
    FT_ASSERT(ft_str_contains(output.c_str(),
        "frame,region_id,invocations,inclusive_ns,exclusive_ns"));
    ft_strlcpy(preserved, output.c_str(), sizeof(preserved));
    frame.breakdown[0].region_id = 99U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_export_frame_json(session,
        frame, &output));
    FT_ASSERT_EQ(FT_TRUE, output == preserved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}

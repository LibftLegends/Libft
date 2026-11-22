#include "../test_internal.hpp"
#define private public
#include "../../Modules/Analytics/analytics.hpp"
#undef private
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

static uint32_t g_analytics_exported_frame = 0U;
static uint32_t g_analytics_trace_events = 0U;

struct analytics_test_manual_clock
{
    uint64_t now_nanoseconds;
};

static uint64_t analytics_test_manual_clock_now(void *user_data) noexcept
{
    analytics_test_manual_clock *clock;

    clock = static_cast<analytics_test_manual_clock *>(user_data);
    return (clock->now_nanoseconds);
}

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

static void analytics_test_atomic_trace_callback(
    const analytics_trace_event &event, void *user_data)
{
    std::atomic<uint32_t> *event_count;

    (void)event;
    event_count = static_cast<std::atomic<uint32_t> *>(user_data);
    event_count->fetch_add(1U, std::memory_order_relaxed);
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

FT_TEST(test_analytics_scope_batch_validates_all_regions_before_recording)
{
    analytics_session session;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    uint32_t region = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("batch", "test", &region));
    const uint32_t ids[] = {region, FT_ANALYTICS_MAX_REGIONS};
    const uint64_t durations[] = {7U, 13U};
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, session.record_scope_batch(ids, durations, durations, 2U));
    analytics_region_statistics statistics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region, &statistics));
    FT_ASSERT_EQ(0U, statistics.invocation_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_percentiles_use_wrapped_sample_windows)
{
    analytics_session session;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    uint32_t region_id = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("descending", "test", &region_id));
    uint64_t percentile = 0;
    FT_ASSERT_EQ(FT_ERR_EMPTY, session.get_region_percentile(region_id, 95, &percentile));
    uint64_t sample = 128;
    while (sample != 0)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.record_scope(region_id, sample, sample));
        analytics_frame_statistics frame{};
        frame.frame_number = 129 - sample;
        frame.duration_nanoseconds = sample;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
        --sample;
    }
    analytics_region_statistics statistics{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id, &statistics));
    FT_ASSERT_EQ(UINT64_C(128), statistics.invocation_count);
    FT_ASSERT_EQ(UINT64_C(16), statistics.percentile_50_nanoseconds);
    FT_ASSERT_EQ(UINT64_C(30), statistics.percentile_95_nanoseconds);
    FT_ASSERT_EQ(UINT64_C(31), statistics.percentile_99_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_percentile(region_id, 99, &percentile));
    FT_ASSERT_EQ(UINT64_C(31), percentile);
    analytics_frame_statistics latest{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&latest));
    FT_ASSERT_EQ(UINT64_C(114), latest.percentile_95_nanoseconds);
    FT_ASSERT_EQ(UINT64_C(118), latest.percentile_99_nanoseconds);
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
    while (event_index < FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS
        * FT_ANALYTICS_EXPORT_BUFFER_COUNT)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(event));
        event_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_FULL, session.publish_trace(event));
    FT_ASSERT_EQ(1U, session.get_dropped_trace_count());
    FT_ASSERT_EQ(0U, trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS
        * FT_ANALYTICS_EXPORT_BUFFER_COUNT, trace_events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

struct analytics_test_trace_order
{
    uint64_t starts[8];
    uint32_t count;
};

static void analytics_test_collect_trace_order(const analytics_trace_event &event, void *context)
{
    auto *order = static_cast<analytics_test_trace_order *>(context);
    if (order->count < 8U)
        order->starts[order->count] = event.start_nanoseconds;
    ++order->count;
    return ;
}

FT_TEST(test_analytics_frame_trace_batches_preserve_order_and_drop_counts)
{
    analytics_session session;
    analytics_session_config configuration;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(&configuration));
    configuration.reserved_trace_events = 2U;
    configuration.buffer_count = 2U;
    configuration.frame_export_interval = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    uint32_t region_id = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("batch", "test", &region_id));
    analytics_test_trace_order order{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(analytics_test_collect_trace_order, &order));
    uint32_t frame = 1;
    while (frame <= 2U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, frame));
        uint32_t scope = 0;
        while (scope < 6U)
        {
            const uint64_t start = frame * 100U + scope * 2U;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope_at(&session, region_id, start));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope_at(&session, start + 1U));
            ++scope;
        }
        if (frame == 1U)
        {
            FT_ASSERT_EQ(FT_ERR_FULL, analytics_end_frame(&session));
            FT_ASSERT_EQ(2U, session.get_dropped_trace_count());
        }
        else
        {
            FT_ASSERT_EQ(FT_ERR_INVALID_STATE, analytics_end_thread_frame(&session));
            FT_ASSERT_EQ(3U, session.get_dropped_trace_count());
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
        ++frame;
    }
    FT_ASSERT_EQ(8U, order.count);
    uint32_t index = 0;
    while (index < order.count)
    {
        FT_ASSERT_EQ((index / 4U + 1U) * 100U + index % 4U * 2U, order.starts[index]);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_many_region_breakdown_keeps_order_totals_and_overflow)
{
    analytics_session session;
    analytics_session_config config;
    config.frame_export_interval = 10U;
    config.trace_frame_interval = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(config));
    constexpr uint32_t REGION_COUNT = FT_ANALYTICS_MAX_FRAME_BREAKDOWN + 3U;
    uint32_t region_ids[REGION_COUNT];
    uint32_t index = 0U;
    while (index < REGION_COUNT)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("fixture", "test", &region_ids[index]));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 1U));
    while (index != 0U)
    {
        const uint32_t region = region_ids[--index];
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope_at(&session, region, 100U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope_at(&session, 107U));
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope_at(&session, region_ids[REGION_COUNT - 1U], 120U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope_at(&session, 133U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    analytics_frame_statistics frame{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&frame));
    FT_ASSERT_EQ(FT_ANALYTICS_MAX_FRAME_BREAKDOWN, frame.breakdown_count);
    FT_ASSERT_EQ(3U, frame.dropped_breakdown_count);
    FT_ASSERT_EQ(2U, frame.breakdown[0].invocation_count);
    FT_ASSERT_EQ(20U, frame.breakdown[0].inclusive_nanoseconds);
    FT_ASSERT_EQ(20U, frame.breakdown[0].exclusive_nanoseconds);
    FT_ASSERT_EQ(0U, frame.percentile_95_nanoseconds);
    FT_ASSERT_EQ(0U, frame.percentile_99_nanoseconds);
    while (index < frame.breakdown_count)
    {
        FT_ASSERT_EQ(region_ids[REGION_COUNT - index - 1U], frame.breakdown[index].region_id);
        ++index;
    }
    analytics_region_statistics dropped_region{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_ids[0], &dropped_region));
    FT_ASSERT_EQ(1U, dropped_region.invocation_count);
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

FT_TEST(test_analytics_exporters_preserve_full_width_unsigned_values)
{
    analytics_session session;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    uint32_t region_id = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("wide", "test", &region_id));
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    analytics_frame_statistics frame{};
    frame.frame_number = UINT64_MAX;
    frame.breakdown_count = 1U;
    frame.breakdown[0].region_id = region_id;
    frame.breakdown[0].inclusive_nanoseconds = UINT64_MAX;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_frame_json(session, frame, &output));
    FT_ASSERT(ft_str_contains(output.c_str(), "\"frame\":18446744073709551615"));
    FT_ASSERT(ft_str_contains(output.c_str(), "\"inclusive_ns\":18446744073709551615"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_frame_csv(session, frame, &output));
    FT_ASSERT_EQ(FT_TRUE, output == "frame,region_id,invocations,inclusive_ns,exclusive_ns\n"
        "18446744073709551615,0,0,18446744073709551615,0\n");

    analytics_trace_event event{};
    event.region_id = region_id;
    event.start_nanoseconds = UINT64_MAX;
    event.duration_nanoseconds = UINT64_MAX;
    event.thread_id = UINT32_MAX;
    constexpr char TRACE[] = "[{\"name\":\"region-0\",\"cat\":\"analytics\",\"ph\":\"X\","
        "\"ts\":18446744073709551,\"dur\":18446744073709551,\"pid\":1,\"tid\":4294967295}]";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_export_trace_json(session, &event, 1U, &output));
    FT_ASSERT_EQ(FT_TRUE, output == TRACE);
    event.region_id = 999U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_export_trace_json(session, &event, 1U, &output));
    FT_ASSERT_EQ(FT_TRUE, output == TRACE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_trace_file_export_does_not_allocate_from_cma)
{
    constexpr char PATH[] = "analytics_allocation_free_trace.jsonl";
    analytics_session session;
    analytics_session_config configuration;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(&configuration));
    configuration.output_path = PATH;
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    uint32_t region_id = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("trace", "test", &region_id));
    analytics_trace_event event{};
    event.region_id = region_id;
    event.start_nanoseconds = 3000U;
    event.duration_nanoseconds = 2000U;
    event.thread_id = 7U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(event));
    cma_set_alloc_limit(1U);
    const int32_t result = session.flush_exports();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    std::FILE *file = std::fopen(PATH, "rb");
    FT_ASSERT(file != ft_nullptr);
    char line[160]{};
    const ft_size_t length = std::fread(line, 1U, sizeof(line) - 1U, file);
    const int32_t close_result = std::fclose(file);
    const int32_t remove_result = std::remove(PATH);
    constexpr char EXPECTED[] = "[{\"name\":\"region-0\",\"cat\":\"analytics\",\"ph\":\"X\","
        "\"ts\":3,\"dur\":2,\"pid\":1,\"tid\":7}]\n";
    FT_ASSERT_EQ(0, close_result);
    FT_ASSERT_EQ(0, remove_result);
    FT_ASSERT_EQ(sizeof(EXPECTED) - 1U, length);
    FT_ASSERT_EQ(0, ft_memcmp(line, EXPECTED, length));
    return (1);
}

FT_TEST(test_analytics_libft_owns_file_exporter_and_copies_region_names)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    const char *region_name;
    uint32_t region_id;
    std::FILE *file;
    char buffer[512];
    ft_size_t bytes_read;

    region_id = 0U;
    region_name = "analytics.region";
    configuration.output_path = "analytics_session_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region(region_name,
        "test", &region_id));
    region_name = "caller.storage.changed";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_name(region_id,
        &region_name));
    FT_ASSERT(ft_str_contains(region_name, "analytics.region"));
    frame = {};
    frame.frame_number = 7U;
    frame.duration_nanoseconds = 100U;
    frame.breakdown_count = 1U;
    frame.breakdown[0].region_id = region_id;
    frame.breakdown[0].invocation_count = 1U;
    frame.breakdown[0].inclusive_nanoseconds = 100U;
    frame.breakdown[0].exclusive_nanoseconds = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    file = std::fopen("analytics_session_test.jsonl", "rb");
    FT_ASSERT(file != ft_nullptr);
    bytes_read = std::fread(buffer, 1U, sizeof(buffer) - 1U, file);
    buffer[bytes_read] = '\0';
    FT_ASSERT(ft_str_contains(buffer, "\"frame\":7"));
    FT_ASSERT_EQ(0, std::fclose(file));
    FT_ASSERT_EQ(0, std::remove("analytics_session_test.jsonl"));
    return (1);
}

FT_TEST(test_analytics_frame_export_interval_samples_file_output)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    std::FILE *file;
    char buffer[1024];
    ft_size_t bytes_read;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.output_path = "analytics_frame_interval_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.frame_export_interval = 2U;
    configuration.start_exporter = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    frame = {};
    frame.frame_number = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    frame.frame_number = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    frame.frame_number = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    file = std::fopen(configuration.output_path, "rb");
    FT_ASSERT(file != ft_nullptr);
    bytes_read = std::fread(buffer, 1U, sizeof(buffer) - 1U, file);
    buffer[bytes_read] = '\0';
    FT_ASSERT(!ft_str_contains(buffer, "\"frame\":1"));
    FT_ASSERT(ft_str_contains(buffer, "\"frame\":2"));
    FT_ASSERT(!ft_str_contains(buffer, "\"frame\":3"));
    FT_ASSERT(ft_str_contains(buffer, "\n"));
    FT_ASSERT_EQ(0, std::fclose(file));
    FT_ASSERT_EQ(0, std::remove(configuration.output_path));
    return (1);
}

FT_TEST(test_analytics_exports_slow_frames_between_periodic_samples)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame = {};
    char buffer[1024];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.output_path = "analytics_slow_frame_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.frame_export_interval = 2U;
    configuration.slow_frame_export_threshold_nanoseconds = 200U;
    configuration.start_exporter = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    frame.frame_number = 1U;
    frame.duration_nanoseconds = 200U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    frame.frame_number = 2U;
    frame.duration_nanoseconds = 199U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    frame.frame_number = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    std::FILE *file = std::fopen(configuration.output_path, "rb");
    FT_ASSERT(file != ft_nullptr);
    const ft_size_t bytes_read = std::fread(buffer, 1U, sizeof(buffer) - 1U, file);
    buffer[bytes_read] = '\0';
    FT_ASSERT(ft_str_contains(buffer, "\"frame\":1"));
    FT_ASSERT(ft_str_contains(buffer, "\"frame\":2"));
    FT_ASSERT(!ft_str_contains(buffer, "\"frame\":3"));
    FT_ASSERT_EQ(0, std::fclose(file));
    FT_ASSERT_EQ(0, std::remove(configuration.output_path));
    return (1);
}

FT_TEST(test_analytics_oldest_export_queue_age_uses_handoff_time)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_test_manual_clock clock;
    analytics_frame_statistics frame;

    clock.now_nanoseconds = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    configuration.buffer_count = 3U;
    configuration.reserved_frame_exports = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    frame = {};
    frame.frame_number = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    clock.now_nanoseconds = 2500U;
    frame.frame_number = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(1U, session.get_export_queue_depth());
    clock.now_nanoseconds = 4000U;
    FT_ASSERT_EQ(1500U,
        session.get_oldest_export_queue_age_nanoseconds());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(0U, session.get_oldest_export_queue_age_nanoseconds());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_world_output_is_separate_from_menu_output)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    analytics_trace_event menu_trace;
    analytics_trace_event world_trace;
    uint32_t menu_region_id;
    uint32_t world_region_id;
    std::FILE *menu_file;
    std::FILE *world_file;
    char menu_buffer[512];
    char world_buffer[512];
    ft_size_t menu_size;
    ft_size_t world_size;

    configuration.output_path = "analytics_menu_test.jsonl";
    configuration.world_output_path = "analytics_world_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_TRUE;
    frame = {};
    menu_trace = {};
    world_trace = {};
    frame.duration_nanoseconds = 10U;
    menu_region_id = 0U;
    world_region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("menu", "test",
        &menu_region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("world", "test",
        &world_region_id));
    menu_trace.region_id = menu_region_id;
    menu_trace.flow_id = 11U;
    world_trace.region_id = world_region_id;
    world_trace.flow_id = 12U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(menu_trace));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_world_active(FT_TRUE));
    frame.frame_number = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(world_trace));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_world_active(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    menu_file = std::fopen("analytics_menu_test.jsonl", "rb");
    world_file = std::fopen("analytics_world_test.jsonl", "rb");
    FT_ASSERT(menu_file != ft_nullptr);
    FT_ASSERT(world_file != ft_nullptr);
    menu_size = std::fread(menu_buffer, 1U, sizeof(menu_buffer) - 1U,
        menu_file);
    world_size = std::fread(world_buffer, 1U, sizeof(world_buffer) - 1U,
        world_file);
    menu_buffer[menu_size] = '\0';
    world_buffer[world_size] = '\0';
    FT_ASSERT(ft_str_contains(menu_buffer, "\"frame\":0"));
    FT_ASSERT(!ft_str_contains(menu_buffer, "\"frame\":2"));
    FT_ASSERT(ft_str_contains(menu_buffer, "\"region-0\""));
    FT_ASSERT(!ft_str_contains(menu_buffer, "\"region-1\""));
    FT_ASSERT(ft_str_contains(world_buffer, "\"frame\":2"));
    FT_ASSERT(ft_str_contains(world_buffer, "\"region-1\""));
    FT_ASSERT(!ft_str_contains(world_buffer, "\"region-0\""));
    FT_ASSERT_EQ(0, std::fclose(menu_file));
    FT_ASSERT_EQ(0, std::fclose(world_file));
    FT_ASSERT_EQ(0, std::remove("analytics_menu_test.jsonl"));
    FT_ASSERT_EQ(0, std::remove("analytics_world_test.jsonl"));
    return (1);
}

FT_TEST(test_analytics_world_only_output_starts_exporter)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    std::FILE *world_file;
    char buffer[512];
    ft_size_t bytes_read;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.world_output_path = "analytics_world_only_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_TRUE;
    frame = {};
    frame.frame_number = 17U;
    frame.duration_nanoseconds = 25U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    world_file = std::fopen(configuration.world_output_path, "rb");
    FT_ASSERT(world_file != ft_nullptr);
    bytes_read = std::fread(buffer, 1U, sizeof(buffer) - 1U, world_file);
    buffer[bytes_read] = '\0';
    FT_ASSERT(ft_str_contains(buffer, "\"frame\":17"));
    FT_ASSERT_EQ(0, std::fclose(world_file));
    FT_ASSERT_EQ(0, std::remove(configuration.world_output_path));
    return (1);
}

FT_TEST(test_analytics_scope_rejects_clock_regression_without_popping)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_test_manual_clock clock;
    uint32_t region_id;
    analytics_region_statistics statistics;

    region_id = 0U;
    clock.now_nanoseconds = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("regression",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    clock.now_nanoseconds = 99U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(0U, statistics.invocation_count);
    clock.now_nanoseconds = 101U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT_EQ(1U, statistics.minimum_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_frame_and_flow_reject_clock_regression)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_test_manual_clock clock;
    analytics_flow_token flow_token;
    analytics_region_statistics statistics;
    uint32_t region_id;

    region_id = 0U;
    clock.now_nanoseconds = 200U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("timed",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 2U));
    clock.now_nanoseconds = 199U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_end_frame(&session));
    clock.now_nanoseconds = 201U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    clock.now_nanoseconds = 300U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_flow(&session, 4U,
        region_id, &flow_token));
    clock.now_nanoseconds = 299U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_end_flow(flow_token));
    clock.now_nanoseconds = 305U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_flow(flow_token));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT_EQ(5U, statistics.minimum_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_worker_frame_failure_releases_thread_state)
{
    analytics_session session;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 20U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope_at(&session, 999U,
        session.now_nanoseconds()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope_at(&session,
        session.now_nanoseconds()));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, analytics_end_thread_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_thread_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_world_classification_is_captured_at_scope_start)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    analytics_test_manual_clock clock;
    uint32_t menu_region_id;
    uint32_t world_region_id;
    std::FILE *menu_file;
    std::FILE *world_file;
    char menu_buffer[1024];
    char world_buffer[1024];
    ft_size_t menu_size;
    ft_size_t world_size;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.output_path = "analytics_scope_menu_test.jsonl";
    configuration.world_output_path = "analytics_scope_world_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    clock.now_nanoseconds = 100U;
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("menu_scope", "test",
        &menu_region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("world_scope", "test",
        &world_region_id));
    frame = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session,
        menu_region_id));
    clock.now_nanoseconds = 110U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_world_active(FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session,
        world_region_id));
    clock.now_nanoseconds = 120U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    clock.now_nanoseconds = 130U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_world_active(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    menu_file = std::fopen(configuration.output_path, "rb");
    world_file = std::fopen(configuration.world_output_path, "rb");
    FT_ASSERT(menu_file != ft_nullptr);
    FT_ASSERT(world_file != ft_nullptr);
    menu_size = std::fread(menu_buffer, 1U, sizeof(menu_buffer) - 1U,
        menu_file);
    world_size = std::fread(world_buffer, 1U, sizeof(world_buffer) - 1U,
        world_file);
    menu_buffer[menu_size] = '\0';
    world_buffer[world_size] = '\0';
    FT_ASSERT(ft_str_contains(menu_buffer, "\"name\":\"region-0\""));
    FT_ASSERT(!ft_str_contains(menu_buffer, "\"name\":\"region-1\""));
    FT_ASSERT(ft_str_contains(menu_buffer, "\"frame\":9"));
    FT_ASSERT(ft_str_contains(world_buffer, "\"name\":\"region-1\""));
    FT_ASSERT(!ft_str_contains(world_buffer, "\"name\":\"region-0\""));
    FT_ASSERT(!ft_str_contains(world_buffer, "\"frame\":9"));
    FT_ASSERT_EQ(0, std::fclose(menu_file));
    FT_ASSERT_EQ(0, std::fclose(world_file));
    FT_ASSERT_EQ(0, std::remove(configuration.output_path));
    FT_ASSERT_EQ(0, std::remove(configuration.world_output_path));
    return (1);
}

FT_TEST(test_analytics_configuration_capacity_and_world_transitions)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.reserved_frame_exports = 1U;
    configuration.reserved_trace_events = 1U;
    configuration.buffer_count = 4U;
    frame = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_FULL, session.publish_frame(frame));
    FT_ASSERT_EQ(1U, session.get_dropped_frame_export_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_trace_frame_interval_samples_without_losing_frames)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;
    uint32_t region_id;
    uint32_t trace_count;
    uint32_t frame_count;

    trace_count = 0U;
    frame_count = 0U;
    frame = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.trace_frame_interval = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("sampled", "test",
        &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(
        analytics_test_trace_callback, &trace_count));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_export_callback(
        analytics_test_export_callback, &frame_count));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.flush_exports());
    FT_ASSERT_EQ(3U, frame_count);
    FT_ASSERT_EQ(1U, trace_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_manual_clock_is_used_by_frame_and_scope_helpers)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_test_manual_clock clock;
    analytics_frame_statistics frame;
    analytics_region_statistics statistics;
    uint32_t region_id;

    clock.now_nanoseconds = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("manual", "test",
        &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 4U));
    clock.now_nanoseconds = 120U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    clock.now_nanoseconds = 170U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    clock.now_nanoseconds = 200U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&frame));
    FT_ASSERT_EQ(100U, frame.duration_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(1U, statistics.invocation_count);
    FT_ASSERT_EQ(50U, statistics.inclusive_nanoseconds);
    FT_ASSERT_EQ(50U, statistics.exclusive_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_recording_does_not_allocate_after_initialization)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_test_manual_clock clock;
    test_cma_failure_controller controller;
    uint32_t region_id;

    clock.now_nanoseconds = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.clock_callback = analytics_test_manual_clock_now;
    configuration.clock_user_data = &clock;
    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("allocation-free",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 1U));
    clock.now_nanoseconds = 120U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session, region_id));
    clock.now_nanoseconds = 160U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    clock.now_nanoseconds = 200U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(0U, test_cma_failure_controller_attempt_count(controller,
        TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_destroy(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_initialize_allocation_failure_leaves_destroyed_state)
{
    analytics_session session;
    analytics_session_config configuration;
    test_cma_failure_controller controller;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, session.initialize(configuration));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, session._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_destroy(controller));
    return (1);
}

FT_TEST(test_analytics_concurrent_producers_and_exporter_drain)
{
    analytics_session session;
    analytics_session_config configuration;
    analytics_trace_event first_event;
    analytics_trace_event second_event;
    std::atomic<uint32_t> exported_count(0U);
    std::atomic<int32_t> first_error(FT_ERR_SUCCESS);
    std::thread first_producer;
    std::thread second_producer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.output_path = "analytics_concurrent_test.jsonl";
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(
        analytics_test_atomic_trace_callback, &exported_count));
    first_event = {};
    first_event.flow_id = 1U;
    first_event.thread_id = 1U;
    second_event = {};
    second_event.flow_id = 2U;
    second_event.thread_id = 2U;
    first_producer = std::thread([&session, &first_event, &first_error]()
    {
        uint32_t index;
        int32_t error_code;

        index = 0U;
        while (index < 100U)
        {
            first_event.frame_number = index;
            error_code = session.publish_trace(first_event);
            if (error_code != FT_ERR_SUCCESS)
                first_error.store(error_code, std::memory_order_relaxed);
            index += 1U;
        }
    });
    second_producer = std::thread([&session, &second_event, &first_error]()
    {
        uint32_t index;
        int32_t error_code;

        index = 0U;
        while (index < 100U)
        {
            second_event.frame_number = index;
            error_code = session.publish_trace(second_event);
            if (error_code != FT_ERR_SUCCESS)
                first_error.store(error_code, std::memory_order_relaxed);
            index += 1U;
        }
    });
    first_producer.join();
    second_producer.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_error.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    FT_ASSERT_EQ(200U, exported_count.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(0, std::remove("analytics_concurrent_test.jsonl"));
    return (1);
}

struct analytics_test_flush_overlap
{
    analytics_session *session;
    std::atomic<uint32_t> callbacks{0U};
    std::atomic<int32_t> nested_result{FT_ERR_INVALID_STATE};
    std::atomic<ft_bool> first_ready{FT_FALSE};
    std::atomic<ft_bool> release_first{FT_TRUE};
};

static void analytics_test_block_first_export(const analytics_trace_event &event, void *context)
{
    auto *state = static_cast<analytics_test_flush_overlap *>(context);
    state->callbacks.fetch_add(1U);
    if (event.frame_number == 1U)
    {
        state->nested_result.store(state->session->flush_exports());
        state->first_ready.store(FT_TRUE);
        while (state->release_first.load() == FT_FALSE)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return ;
}

FT_TEST(test_analytics_manual_flush_waits_for_worker_buffer_ownership)
{
    analytics_test_flush_overlap state{};
    constexpr char PATH[] = "analytics_flush_overlap.jsonl";
    analytics_session session;
    analytics_session_config config;
    config.output_path = PATH;
    config.output_format = analytics_output_format::JSONL;
    config.reserved_trace_events = 1U;
    config.buffer_count = 3U;
    config.start_exporter = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(config));
    state.session = &session;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.set_trace_callback(analytics_test_block_first_export, &state));
    analytics_trace_event event{};
    event.frame_number = 1U;
    while (event.frame_number <= 3U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_trace(event));
        ++event.frame_number;
    }
    state.release_first.store(FT_FALSE);
    const int32_t start_result = session.start_exporter();
    if (start_result != FT_ERR_SUCCESS)
        state.release_first.store(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, start_result);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (state.first_ready.load() == FT_FALSE && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const ft_bool worker_ready = state.first_ready.load();
    if (!worker_ready)
        state.release_first.store(FT_TRUE);
    FT_ASSERT_EQ(FT_TRUE, worker_ready);
    std::atomic<ft_bool> manual_started{FT_FALSE};
    std::atomic<ft_bool> manual_finished{FT_FALSE};
    int32_t manual_result = FT_ERR_INVALID_STATE;
    std::thread caller([&]()
    {
        manual_started.store(FT_TRUE);
        manual_result = session.flush_exports();
        manual_finished.store(FT_TRUE);
    });
    while (manual_started.load() == FT_FALSE)
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const ft_bool returned_early = manual_finished.load();
    const uint32_t callbacks_while_blocked = state.callbacks.load();
    const int32_t full_queue_result = session.publish_trace(event);
    state.release_first.store(FT_TRUE);
    caller.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.nested_result.load());
    FT_ASSERT_EQ(FT_FALSE, returned_early);
    FT_ASSERT_EQ(1U, callbacks_while_blocked);
    FT_ASSERT_EQ(FT_ERR_FULL, full_queue_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manual_result);
    FT_ASSERT_EQ(3U, state.callbacks.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    FT_ASSERT_EQ(0, std::remove(PATH));
    return (1);
}

FT_TEST(test_analytics_runtime_shutdown_waits_for_active_scope)
{
    analytics_session session;
    std::atomic<ft_bool> scope_started;
    std::atomic<ft_bool> scope_finished;
    std::thread worker;

    scope_started.store(FT_FALSE, std::memory_order_release);
    scope_finished.store(FT_FALSE, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_register_regions(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_set_sample_rate(1U));
    worker = std::thread([&session, &scope_started, &scope_finished]() -> void
    {
        analytics_runtime_scope_token worker_token;
        int32_t frame_error;
        int32_t scope_error;

        frame_error = analytics_begin_frame(&session, 1U);
        scope_error = analytics_runtime_scope_begin(
            analytics_runtime_region::CMA_MALLOC, &worker_token);
        scope_started.store(FT_TRUE, std::memory_order_release);
        if (scope_error != FT_ERR_SUCCESS || worker_token.active == FT_FALSE)
            return ;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        if (analytics_runtime_scope_end(&worker_token) == FT_ERR_SUCCESS
            && frame_error == FT_ERR_SUCCESS
            && analytics_end_frame(&session) == FT_ERR_SUCCESS)
            scope_finished.store(FT_TRUE, std::memory_order_release);
        return ;
    });
    while (scope_started.load(std::memory_order_acquire) == FT_FALSE)
        std::this_thread::yield();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_shutdown());
    worker.join();
    FT_ASSERT_EQ(FT_TRUE, scope_finished.load(std::memory_order_acquire));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_runtime_burst_preserves_enclosing_application_scope)
{
    analytics_session session;
    analytics_frame_statistics frame;
    analytics_runtime_scope_token token;
    uint32_t application_region;
    uint32_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("application",
        "test", &application_region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_register_regions(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_set_sample_rate(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_scope(&session,
        application_region));
    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_EVENTS * 2U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_begin(
            analytics_runtime_region::CMA_MALLOC, &token));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_end(&token));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_scope(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&frame));
    FT_ASSERT_EQ(2U, frame.breakdown_count);
    FT_ASSERT_EQ(FT_ANALYTICS_MAX_THREAD_EVENTS / 2U,
        frame.breakdown[0].invocation_count);
    FT_ASSERT_EQ(application_region, frame.breakdown[1].region_id);
    FT_ASSERT_EQ(1U, frame.breakdown[1].invocation_count);
    FT_ASSERT_EQ(0U, frame.dropped_scope_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_begin_frame(&session, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_begin(
        analytics_runtime_region::CMA_MALLOC, &token));
    FT_ASSERT_EQ(FT_TRUE, token.active);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_end(&token));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_end_frame(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_shutdown());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_runtime_gate_closes_scope_admission)
{
    analytics_session session;
    analytics_runtime_scope_token token;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_register_regions(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_shutdown());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_begin(
        analytics_runtime_region::CMA_MALLOC, &token));
    FT_ASSERT_EQ(FT_FALSE, token.active);
    FT_ASSERT_EQ(FT_FALSE, token.runtime_scope_registered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_scope_end(&token));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_runtime_registration_rejects_duplicate)
{
    analytics_session session;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_register_regions(&session));
    FT_ASSERT_EQ(FT_ERR_ALREADY_INITIALISED,
        analytics_runtime_register_regions(&session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_runtime_shutdown());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_overflow_policies_preserve_buffer_ownership)
{
    analytics_session session;
    analytics_session failed_session;
    analytics_session_config configuration;
    analytics_frame_statistics frame;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, analytics_default_session_config(
        &configuration));
    configuration.reserved_frame_exports = 1U;
    configuration.buffer_count = 3U;
    configuration.overflow_policy = analytics_overflow_policy::
        DROP_OLDEST_COMPLETED_WITH_COUNTER;
    frame = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(1U, session.get_dropped_frame_export_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());

    configuration.overflow_policy = analytics_overflow_policy::FAIL_SESSION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, failed_session.initialize(configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, failed_session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, failed_session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, failed_session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_FULL, failed_session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_FULL, failed_session.get_export_error());
    FT_ASSERT_EQ(FT_FALSE, failed_session.is_enabled());
    FT_ASSERT_EQ(FT_ERR_FULL, failed_session.destroy());
    return (1);
}

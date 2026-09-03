#include "../test_internal.hpp"
#include "../../Modules/Analytics/analytics.hpp"
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

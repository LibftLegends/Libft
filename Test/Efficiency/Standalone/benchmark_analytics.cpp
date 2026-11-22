#include "../../../Modules/Analytics/analytics.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_statistics(analytics_session &session, uint32_t region_id)
{
    constexpr uint32_t ITERATIONS = 20000;
    uint32_t sample = 0;
    while (sample < FT_ANALYTICS_MAX_SAMPLES)
    {
        const uint64_t duration = (sample * 13U) % FT_ANALYTICS_MAX_SAMPLES + 1U;
        if (session.record_scope(region_id, duration, duration) != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    uint32_t iteration = 0;
    analytics_region_statistics statistics{};
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (session.get_region_statistics(region_id, &statistics) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (statistics.percentile_95_nanoseconds != 30U)
        return (1);
    std::printf("analytics_region_statistics ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_frame_publication(analytics_session &session)
{
    constexpr uint32_t BATCHES = 40;
    constexpr uint32_t BATCH_SIZE = 120;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t batch = 0;
    while (batch < BATCHES)
    {
        uint32_t index = 0;
        const auto start = std::chrono::steady_clock::now();
        while (index < BATCH_SIZE)
        {
            analytics_frame_statistics frame{};
            frame.frame_number = batch * BATCH_SIZE + index;
            frame.duration_nanoseconds = (index * 37U) % BATCH_SIZE + 1U;
            if (session.publish_frame(frame) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        elapsed += std::chrono::steady_clock::now() - start;
        if (session.flush_exports() != FT_ERR_SUCCESS)
            return (1);
        ++batch;
    }
    analytics_frame_statistics latest{};
    if (session.get_latest_frame(&latest) != FT_ERR_SUCCESS
        || latest.percentile_99_nanoseconds != 118U)
        return (1);
    std::printf("analytics_sampled_frame ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / (BATCHES * BATCH_SIZE));
    return (0);
}

static int32_t measure_traced_frames(analytics_session &session, const uint32_t *region_ids,
    uint32_t region_count, const char *name)
{
    constexpr uint32_t FRAME_COUNT = 512;
    constexpr uint32_t SCOPE_COUNT = FT_ANALYTICS_MAX_THREAD_EVENTS;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t frame_number = 0;
    while (frame_number < FRAME_COUNT)
    {
        if (analytics_begin_frame(&session, frame_number) != FT_ERR_SUCCESS)
            return (1);
        uint32_t scope = 0;
        while (scope < SCOPE_COUNT)
        {
            if (analytics_begin_scope(&session, region_ids[scope % region_count]) != FT_ERR_SUCCESS
                || analytics_end_scope(&session) != FT_ERR_SUCCESS)
                return (1);
            ++scope;
        }
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = analytics_end_frame(&session);
        elapsed += std::chrono::steady_clock::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++frame_number;
        if (frame_number % 8U == 0U && session.flush_exports() != FT_ERR_SUCCESS)
            return (1);
    }
    if (session.get_dropped_trace_count() != 0U)
        return (1);
    std::printf("%s ns/op=%.2f\n", name,
        std::chrono::duration<double, std::nano>(elapsed).count() / FRAME_COUNT);
    return (0);
}

static int32_t measure_many_regions(analytics_session &session)
{
    constexpr uint32_t REGION_COUNT = FT_ANALYTICS_MAX_FRAME_BREAKDOWN;
    uint32_t region_ids[REGION_COUNT];
    uint32_t index = 0;
    while (index < REGION_COUNT)
    {
        if (session.register_region("multi-region fixture", "performance", &region_ids[index]) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    return (measure_traced_frames(session, region_ids, REGION_COUNT, "analytics_many_regions"));
}

template <typename Serialize>
static int32_t measure_serializer(const char *name, Serialize serialize)
{
    constexpr uint32_t ITERATIONS = 16;
    ft_string output;
    if (output.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (serialize(&output) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (output.empty())
        return (1);
    std::printf("%s ns/op=%.2f\n", name,
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_exports(analytics_session &session)
{
    analytics_frame_statistics frame{};
    frame.frame_number = 1000000U;
    frame.duration_nanoseconds = 16666667U;
    frame.breakdown_count = 32U;
    uint32_t index = 0U;
    while (index < frame.breakdown_count)
    {
        auto &entry = frame.breakdown[index];
        if (session.register_region("export fixture", "performance", &entry.region_id) != FT_ERR_SUCCESS)
            return (1);
        entry.invocation_count = 12U;
        entry.inclusive_nanoseconds = 400000U + index;
        entry.exclusive_nanoseconds = 100000U + index;
        ++index;
    }
    analytics_trace_event events[128]{};
    index = 0U;
    while (index < 128U)
    {
        events[index].region_id = frame.breakdown[index % frame.breakdown_count].region_id;
        events[index].start_nanoseconds = 261100000000U + index * 100000U;
        events[index].duration_nanoseconds = 20000U + index * 100U;
        events[index].thread_id = 840193118U;
        ++index;
    }
    if (measure_serializer("analytics_export_frame_json", [&](ft_string *output)
        { return (analytics_export_frame_json(session, frame, output)); }) != 0
        || measure_serializer("analytics_export_frame_csv", [&](ft_string *output)
        { return (analytics_export_frame_csv(session, frame, output)); }) != 0
        || measure_serializer("analytics_export_trace_json", [&](ft_string *output)
        { return (analytics_export_trace_json(session, events, 128U, output)); }) != 0)
        return (1);
    return (0);
}

static int32_t measure_trace_file()
{
    constexpr uint32_t BATCH_COUNT = 8U;
    constexpr uint32_t BATCH_SIZE = 128U;
    ft_string path;
    if (filesystem_temp_path("libft_analytics_bench", "jsonl", &path) != FT_ERR_SUCCESS)
        return (1);
    analytics_session_config configuration;
    configuration.output_path = path.c_str();
    configuration.output_format = analytics_output_format::JSONL;
    configuration.start_exporter = FT_FALSE;
    analytics_session session;
    if (session.initialize(configuration) != FT_ERR_SUCCESS)
        return (1);
    analytics_trace_event event{};
    int32_t result = session.register_region("file fixture", "performance", &event.region_id);
    event.start_nanoseconds = 261100000000U;
    event.duration_nanoseconds = 100000U;
    event.thread_id = 840193118U;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t batch = 0U;
    while (batch < BATCH_COUNT && result == FT_ERR_SUCCESS)
    {
        uint32_t index = 0U;
        while (index < BATCH_SIZE && result == FT_ERR_SUCCESS)
        {
            result = session.publish_trace(event);
            ++index;
        }
        if (result == FT_ERR_SUCCESS)
        {
            const auto start = std::chrono::steady_clock::now();
            result = session.flush_exports();
            elapsed += std::chrono::steady_clock::now() - start;
        }
        ++batch;
    }
    const int32_t close_result = session.destroy();
    const int32_t remove_result = std::remove(path.c_str());
    if (result != FT_ERR_SUCCESS || close_result != FT_ERR_SUCCESS || remove_result != 0)
        return (1);
    std::printf("analytics_trace_file ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / (BATCH_COUNT * BATCH_SIZE));
    return (0);
}

int32_t main()
{
    analytics_session session;
    uint32_t region_id = 0;
    if (session.initialize() != FT_ERR_SUCCESS
        || session.register_region("benchmark", "performance", &region_id) != FT_ERR_SUCCESS
        || measure_statistics(session, region_id) != 0 || measure_frame_publication(session) != 0
        || measure_traced_frames(session, &region_id, 1U, "analytics_traced_frame") != 0
        || measure_many_regions(session) != 0 || measure_exports(session) != 0)
        return (1);
    if (session.destroy() != FT_ERR_SUCCESS)
        return (1);
    return (measure_trace_file());
}

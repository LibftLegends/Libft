#include "../../../Modules/Time/time.hpp"
#include "../../../Modules/Time/time_fps.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static volatile uint64_t observed;

template <typename Operation>
static void benchmark(const char *name, uint32_t iterations, Operation operation)
{
    uint64_t result = 0U;
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < iterations)
    {
        result += operation(index);
        ++index;
    }
    observed = result;
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

int main()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    constexpr uint32_t TRACE_ITERATIONS = 30000U;
    benchmark("time_floor_to_day", ITERATIONS, [](uint32_t index)
    {
        return (static_cast<uint64_t>(time_floor_to_day(1700000000 - index)));
    });
    const t_monotonic_time_point anchor = time_monotonic_point_create(42);
    benchmark("time_monotonic_to_wall_ms", ITERATIONS, [&anchor](uint32_t index)
    {
        int64_t translated = 0;
        if (!time_monotonic_to_wall_ms(time_monotonic_point_create(index), anchor,
                1700000000000, translated))
            return (uint64_t{0U});
        return (static_cast<uint64_t>(translated));
    });
    time_fps frame_pacer;
    if (frame_pacer.initialize() != FT_ERR_SUCCESS
        || frame_pacer.set_frames_per_second(1000000000) != FT_ERR_SUCCESS)
        return (1);
    benchmark("time_fps_high_rate_pacing", ITERATIONS, [&frame_pacer](uint32_t)
    {
        frame_pacer.sleep_to_next_frame();
        return (1U);
    });

#if defined(_WIN32)
    const char *trace_sink = "NUL";
#else
    const char *trace_sink = "/dev/null";
#endif
    if (!time_trace_begin_session(trace_sink))
        return (1);
    benchmark("time_trace_instant_event_null_sink", TRACE_ITERATIONS, [](uint32_t)
    {
        return (time_trace_instant_event("simulation step with a long event name",
                "game loop with a long category"));
    });
    if (observed != TRACE_ITERATIONS || !time_trace_end_session())
        return (1);
    return (0);
}

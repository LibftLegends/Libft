#include "../../../Modules/Threading/thread_pool.hpp"
#include "../../../Modules/Threading/task_scheduler_tracing.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static uint32_t trace_events = 0U;

static void print_average(const char *name, benchmark_clock::time_point started,
    uint32_t iterations)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

static void trace_sink(const ft_task_trace_event &)
{
    ++trace_events;
    return ;
}

static int32_t benchmark_trace()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    const ft_task_trace_event event{};
    uint32_t index = 0U;
    auto started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        task_scheduler_trace_emit(event);
        ++index;
    }
    print_average("scheduler_trace_no_sinks", started, ITERATIONS);
    if (task_scheduler_register_trace_sink(trace_sink) != 0)
        return (1);
    index = 0U;
    started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        task_scheduler_trace_emit(event);
        ++index;
    }
    print_average("scheduler_trace_one_sink", started, ITERATIONS);
    if (task_scheduler_unregister_trace_sink(trace_sink) != 0
        || trace_events != ITERATIONS)
        return (1);
    return (0);
}

static int32_t benchmark_pool()
{
    constexpr uint32_t ITERATIONS = 200U;
    ft_thread_pool pool(1U, 0U);
    std::atomic<uint32_t> completed{0U};
    if (pool.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        pool.submit([&completed]()
        {
            completed.fetch_add(1U, std::memory_order_relaxed);
            return ;
        });
        if (pool.get_error() != FT_ERR_SUCCESS)
            return (1);
        pool.wait();
        if (pool.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    print_average("pool_submit_and_wait", started, ITERATIONS);
    const int32_t destroy_error = pool.destroy();
    const uint32_t observed = completed.load();
    if (destroy_error != FT_ERR_SUCCESS || observed != ITERATIONS)
    {
        std::fprintf(stderr, "pool destroy status=%d, completed=%u, expected=%u\n",
            destroy_error, observed, ITERATIONS);
        return (1);
    }
    return (0);
}

int main()
{
    return (benchmark_trace() || benchmark_pool());
}

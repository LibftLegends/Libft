#include "../../../Modules/Observability/observability.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

static int32_t benchmark_counters(uint32_t worker_count, ft_size_t bytes_read,
    ft_size_t bytes_written)
{
    constexpr uint32_t MAX_WORKERS = 4;
    constexpr uint32_t ITERATIONS = 500000;
    std::thread workers[MAX_WORKERS];
    std::atomic<uint32_t> ready(0);
    std::atomic<ft_bool> start(FT_FALSE);

    if (observability_reset_counters(FT_OBSERVABILITY_MODULE_NETWORKING) != FT_ERR_SUCCESS)
        return (1);
    uint32_t worker_index = 0;
    while (worker_index < worker_count)
    {
        workers[worker_index] = std::thread([&]()
        {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            uint32_t iteration = 0;
            while (iteration < ITERATIONS)
            {
                observability_record_operation(FT_OBSERVABILITY_MODULE_NETWORKING,
                    FT_TRUE, bytes_read, bytes_written);
                ++iteration;
            }
            return ;
        });
        ++worker_index;
    }
    while (ready.load(std::memory_order_acquire) != worker_count)
        std::this_thread::yield();
    const auto start_time = std::chrono::steady_clock::now();
    start.store(FT_TRUE, std::memory_order_release);
    worker_index = 0;
    while (worker_index < worker_count)
    {
        workers[worker_index].join();
        ++worker_index;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start_time).count();
    const ft_size_t expected = worker_count * ITERATIONS;
    ft_observability_counters counters{};
    if (observability_get_counters(FT_OBSERVABILITY_MODULE_NETWORKING, &counters)
            != FT_ERR_SUCCESS
        || counters.operations != expected || counters.successes != expected
        || counters.failures != 0 || counters.bytes_read != expected * bytes_read
        || counters.bytes_written != expected * bytes_written)
        return (1);
    std::printf("%u worker(s), read=%" FT_PRIu64 ", write=%" FT_PRIu64 ": %.2f ns/op\n",
        worker_count, bytes_read, bytes_written, elapsed_ns / expected);
    return (0);
}

int main()
{
    if (benchmark_counters(1, 0, 0) != 0 || benchmark_counters(1, 0, 64) != 0
        || benchmark_counters(1, 64, 64) != 0 || benchmark_counters(4, 0, 0) != 0
        || benchmark_counters(4, 64, 0) != 0)
        return (1);
    return (0);
}

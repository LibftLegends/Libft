#include "../../../Modules/PThread/pthread.hpp"
#include "../../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using benchmark_clock = std::chrono::steady_clock;

static void print_average(const char *name, benchmark_clock::time_point started,
    uint32_t iterations)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

static int32_t benchmark_recursion()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    pt_recursive_mutex mutex;
    if (mutex.initialize() != FT_ERR_SUCCESS || mutex.lock() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        if (mutex.lock() != FT_ERR_SUCCESS || mutex.unlock() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    print_average("recursive_mutex_nested_lock_unlock", started, ITERATIONS);
    if (mutex.unlock() != FT_ERR_SUCCESS || mutex.destroy() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}

static int32_t wait_for_epoch(std::atomic<uint32_t> &epoch, uint32_t target)
{
    uint32_t value = epoch.load(std::memory_order_acquire);
    while (value != target)
    {
        if (value == UINT32_MAX
            || pt_thread_wait_uint32(&epoch, value) != FT_ERR_SUCCESS)
            return (1);
        value = epoch.load(std::memory_order_acquire);
    }
    return (0);
}

static int32_t publish_epoch(std::atomic<uint32_t> &epoch, uint32_t current)
{
    if (!epoch.compare_exchange_strong(current, current + 1U,
            std::memory_order_release, std::memory_order_relaxed))
        return (1);
    return (pt_thread_wake_one_uint32(&epoch) != FT_ERR_SUCCESS);
}

static int32_t benchmark_handoff()
{
    constexpr uint32_t ITERATIONS = 300U;
    std::atomic<uint32_t> epoch{0U};
    std::thread worker([&]()
    {
        uint32_t round = 0U;
        while (round < ITERATIONS)
        {
            if (wait_for_epoch(epoch, round * 2U + 1U)
                || publish_epoch(epoch, round * 2U + 1U))
            {
                epoch.store(UINT32_MAX, std::memory_order_release);
                pt_thread_wake_all_uint32(&epoch);
                return ;
            }
            ++round;
        }
        return ;
    });
    uint32_t round = 0U;
    const auto started = benchmark_clock::now();
    while (round < ITERATIONS)
    {
        if (publish_epoch(epoch, round * 2U)
            || wait_for_epoch(epoch, round * 2U + 2U))
        {
            epoch.store(UINT32_MAX, std::memory_order_release);
            pt_thread_wake_all_uint32(&epoch);
            break ;
        }
        ++round;
    }
    const auto finished = benchmark_clock::now();
    worker.join();
    if (round != ITERATIONS || epoch.load() != ITERATIONS * 2U)
        return (1);
    const double elapsed = std::chrono::duration<double, std::nano>(finished - started).count();
    std::printf("atomic_wait_round_trip ns/op=%.2f\n",
        elapsed / static_cast<double>(ITERATIONS));
    return (0);
}

int main()
{
    return (benchmark_recursion() || benchmark_handoff());
}

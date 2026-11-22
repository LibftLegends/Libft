#include "../../../Modules/PThread/condition.hpp"
#include "../../../Modules/PThread/pthread.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;

static void print_average(const char *name, benchmark_clock::time_point started,
    uint32_t iterations)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

static int32_t benchmark_read_locks()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    constexpr uint32_t LOCK_COUNT = 80U;
    t_pt_rwlock locks[LOCK_COUNT];
    uint32_t index = 0U;
    while (index < LOCK_COUNT)
    {
        if (pt_rwlock_strategy_init(&locks[index], PT_RWLOCK_STRATEGY_WRITER_PRIORITY))
            return (1);
        ++index;
    }
    auto started = benchmark_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        if (pt_rwlock_strategy_rdlock(&locks[0]) || pt_rwlock_strategy_rdunlock(&locks[0]))
            return (1);
        ++index;
    }
    print_average("rwlock_single_read_lock_unlock", started, ITERATIONS);
    index = 0U;
    while (index < LOCK_COUNT)
    {
        if (pt_rwlock_strategy_rdlock(&locks[index]))
            return (1);
        ++index;
    }
    started = benchmark_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        if (pt_rwlock_strategy_rdunlock(&locks[0]) || pt_rwlock_strategy_rdlock(&locks[0]))
            return (1);
        ++index;
    }
    print_average("rwlock_churn_with_80_held", started, ITERATIONS);
    index = LOCK_COUNT;
    while (index != 0U)
    {
        --index;
        if (pt_rwlock_strategy_rdunlock(&locks[index])
            || pt_rwlock_strategy_destroy(&locks[index]))
            return (1);
    }
    return (0);
}

static int32_t benchmark_condition()
{
    constexpr uint32_t ITERATIONS = 500000U;
    pt_condition_variable condition;
    if (condition.signal() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        if (condition.signal() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    print_average("condition_signal_initialized", started, ITERATIONS);
    return (0);
}

#if defined(_WIN32)
static void *join_worker(void *pointer)
{
    auto *released = static_cast<std::atomic<uint32_t> *>(pointer);
    while (released->load() == 0U)
        pt_thread_wait_uint32(released, 0U);
    return (pointer);
}

static int32_t benchmark_join_timeout()
{
    constexpr uint32_t ITERATIONS = 12U;
    std::atomic<uint32_t> released{0U};
    pthread_t thread;
    if (pt_thread_create(&thread, ft_nullptr, join_worker, &released) != 0)
        return (1);
    uint32_t index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS)
    {
        if (pt_thread_timed_join(thread, ft_nullptr, 5) != ETIMEDOUT)
            break ;
        ++index;
    }
    print_average("timed_join_5_ms", started, ITERATIONS);
    released.store(1U);
    pt_thread_wake_one_uint32(&released);
    if (pt_thread_join(thread, ft_nullptr) != 0 || index != ITERATIONS)
        return (1);
    return (0);
}
#endif

int main()
{
    if (benchmark_read_locks() || benchmark_condition())
        return (1);
#if defined(_WIN32)
    return (benchmark_join_timeout());
#else
    return (0);
#endif
}

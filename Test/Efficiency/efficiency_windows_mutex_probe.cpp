#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>

static const uint64_t PROBE_OPERATIONS = 1000000U;

template <typename t_operation>
static uint64_t probe_elapsed(t_operation operation)
{
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    uint64_t operation_index;

    start_time = std::chrono::steady_clock::now();
    operation_index = 0U;
    while (operation_index < PROBE_OPERATIONS)
    {
        operation();
        operation_index += 1U;
    }
    end_time = std::chrono::steady_clock::now();
    return (static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::microseconds>(end_time - start_time).count()));
}

void efficiency_run_windows_mutex_probe(void)
{
    std::mutex native_mutex;
    std::mutex registry_mutex;
    std::atomic<uint32_t> owner_state(0U);
    uint64_t lock_elapsed;
    uint64_t try_lock_elapsed;
    uint64_t double_lock_elapsed;
    uint64_t atomic_elapsed;

    lock_elapsed = probe_elapsed([&native_mutex]()
    {
        native_mutex.lock();
        native_mutex.unlock();
        return ;
    });
    try_lock_elapsed = probe_elapsed([&native_mutex]()
    {
        if (native_mutex.try_lock())
            native_mutex.unlock();
        return ;
    });
    double_lock_elapsed = probe_elapsed([&native_mutex, &registry_mutex]()
    {
        native_mutex.lock();
        registry_mutex.lock();
        registry_mutex.unlock();
        native_mutex.unlock();
        return ;
    });
    atomic_elapsed = probe_elapsed([&owner_state]()
    {
        owner_state.store(1U, std::memory_order_release);
        (void)owner_state.load(std::memory_order_acquire);
        owner_state.store(0U, std::memory_order_release);
        return ;
    });
    std::printf("[PERFORMANCE] Windows mutex probe: lock=%llu us "
        "try_lock=%llu us double_lock=%llu us atomics=%llu us\n",
        static_cast<unsigned long long>(lock_elapsed),
        static_cast<unsigned long long>(try_lock_elapsed),
        static_cast<unsigned long long>(double_lock_elapsed),
        static_cast<unsigned long long>(atomic_elapsed));
    return ;
}

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <pthread.h>
#include <thread>

#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread.hpp"

static const uint32_t RWLOCK_SNAPSHOT_BLOCK_COUNT = 4096U;
static const ft_size_t RWLOCK_SNAPSHOT_BYTE_COUNT =
    RWLOCK_SNAPSHOT_BLOCK_COUNT * sizeof(uint32_t);
static const uint32_t RWLOCK_SEQUENTIAL_OPERATION_COUNT = 128U;
static const uint32_t RWLOCK_CONCURRENT_OPERATION_COUNT = 256U;

static uint32_t g_rwlock_snapshot_source[RWLOCK_SNAPSHOT_BLOCK_COUNT];
static uint32_t g_rwlock_snapshot_destinations[16]
    [RWLOCK_SNAPSHOT_BLOCK_COUNT];

struct s_rwlock_benchmark_state
{
    std::mutex *standard_mutex;
    t_pt_rwlock *strategy_lock;
    pthread_rwlock_t *native_lock;
    pt_mutex *mutex;
    uint32_t *snapshot_source;
    uint32_t *snapshot_destination;
    uint64_t checksum;
    uint32_t operation_count;
    uint32_t mode;
    ft_bool mixed;
};

static void benchmark_initialize_snapshot(void)
{
    uint32_t block_index;

    block_index = 0U;
    while (block_index < RWLOCK_SNAPSHOT_BLOCK_COUNT)
    {
        g_rwlock_snapshot_source[block_index] =
            block_index * 2654435761U + 0x9e3779b9U;
        block_index += 1U;
    }
    return ;
}

static void benchmark_read(s_rwlock_benchmark_state *state)
{
    uint64_t observed_value;

    if (state->mode == 0U)
    {
        state->standard_mutex->lock();
        (void)ft_memcpy(state->snapshot_destination, state->snapshot_source,
            RWLOCK_SNAPSHOT_BYTE_COUNT);
        state->standard_mutex->unlock();
    }
    else if (state->mode == 1U)
    {
        (void)state->mutex->lock();
        (void)ft_memcpy(state->snapshot_destination, state->snapshot_source,
            RWLOCK_SNAPSHOT_BYTE_COUNT);
        (void)state->mutex->unlock();
    }
    else if (state->mode == 2U)
    {
        (void)pt_rwlock_rdlock(state->native_lock);
        (void)ft_memcpy(state->snapshot_destination, state->snapshot_source,
            RWLOCK_SNAPSHOT_BYTE_COUNT);
        (void)pt_rwlock_unlock(state->native_lock);
    }
    else
    {
        (void)pt_rwlock_strategy_rdlock(state->strategy_lock);
        (void)ft_memcpy(state->snapshot_destination, state->snapshot_source,
            RWLOCK_SNAPSHOT_BYTE_COUNT);
        (void)pt_rwlock_strategy_rdunlock(state->strategy_lock);
    }
    observed_value = static_cast<uint64_t>(state->snapshot_destination[0U])
        + static_cast<uint64_t>(state->snapshot_destination[
            RWLOCK_SNAPSHOT_BLOCK_COUNT / 2U])
        + static_cast<uint64_t>(state->snapshot_destination[
            RWLOCK_SNAPSHOT_BLOCK_COUNT - 1U]);
    state->checksum += observed_value;
    (void)observed_value;
    return ;
}

static void benchmark_write(s_rwlock_benchmark_state *state)
{
    if (state->mode == 0U)
    {
        state->standard_mutex->lock();
        state->snapshot_source[0U] += 1U;
        state->standard_mutex->unlock();
    }
    else if (state->mode == 1U)
    {
        (void)state->mutex->lock();
        state->snapshot_source[0U] += 1U;
        (void)state->mutex->unlock();
    }
    else if (state->mode == 2U)
    {
        (void)pt_rwlock_wrlock(state->native_lock);
        state->snapshot_source[0U] += 1U;
        (void)pt_rwlock_unlock(state->native_lock);
    }
    else
    {
        (void)pt_rwlock_strategy_wrlock(state->strategy_lock);
        state->snapshot_source[0U] += 1U;
        (void)pt_rwlock_strategy_wrunlock(state->strategy_lock);
    }
    return ;
}

static void benchmark_worker(s_rwlock_benchmark_state *state)
{
    uint32_t operation_index;

    operation_index = 0U;
    while (operation_index < state->operation_count)
    {
        if (state->mixed != FT_FALSE && operation_index % 20U == 0U)
            benchmark_write(state);
        else
            benchmark_read(state);
        operation_index += 1U;
    }
    return ;
}

static uint64_t benchmark_sequential(uint32_t mode, ft_bool write_mode)
{
    const uint32_t operation_count = RWLOCK_SEQUENTIAL_OPERATION_COUNT;
    t_pt_rwlock strategy_lock;
    pthread_rwlock_t native_lock;
    pt_mutex mutex;
    std::mutex standard_mutex;
    s_rwlock_benchmark_state state;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;
    uint32_t operation_index;

    state.strategy_lock = &strategy_lock;
    state.native_lock = &native_lock;
    state.mutex = &mutex;
    state.standard_mutex = &standard_mutex;
    state.snapshot_source = g_rwlock_snapshot_source;
    state.snapshot_destination = g_rwlock_snapshot_destinations[0U];
    state.checksum = 0U;
    state.operation_count = 0U;
    state.mode = mode;
    state.mixed = FT_FALSE;
    if (mutex.initialize() != FT_ERR_SUCCESS
        || pt_rwlock_init(&native_lock, ft_nullptr) != 0
        || pt_rwlock_strategy_init(&strategy_lock,
            PT_RWLOCK_STRATEGY_WRITER_PRIORITY) != FT_ERR_SUCCESS)
        return (0U);
    start_time = std::chrono::steady_clock::now();
    operation_index = 0U;
    while (operation_index < operation_count)
    {
        state.operation_count = 1U;
        if (write_mode != FT_FALSE)
            benchmark_write(&state);
        else
            benchmark_read(&state);
        operation_index += 1U;
    }
    finish_time = std::chrono::steady_clock::now();
    (void)pt_rwlock_strategy_destroy(&strategy_lock);
    (void)pt_rwlock_destroy(&native_lock);
    (void)mutex.destroy();
    return (static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::microseconds>(finish_time - start_time).count()));
}

static uint64_t benchmark_concurrent(uint32_t mode, uint32_t worker_count,
    ft_bool mixed)
{
    const uint32_t operation_count = RWLOCK_CONCURRENT_OPERATION_COUNT;
    t_pt_rwlock strategy_lock;
    pthread_rwlock_t native_lock;
    pt_mutex mutex;
    std::mutex standard_mutex;
    std::thread workers[16];
    s_rwlock_benchmark_state states[16];
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;
    uint32_t worker_index;

    if (mutex.initialize() != FT_ERR_SUCCESS
        || pt_rwlock_init(&native_lock, ft_nullptr) != 0
        || pt_rwlock_strategy_init(&strategy_lock,
            PT_RWLOCK_STRATEGY_WRITER_PRIORITY) != FT_ERR_SUCCESS)
        return (0U);
    worker_index = 0U;
    while (worker_index < worker_count)
    {
        states[worker_index].strategy_lock = &strategy_lock;
        states[worker_index].native_lock = &native_lock;
        states[worker_index].mutex = &mutex;
        states[worker_index].standard_mutex = &standard_mutex;
        states[worker_index].snapshot_source = g_rwlock_snapshot_source;
        states[worker_index].snapshot_destination =
            g_rwlock_snapshot_destinations[worker_index];
        states[worker_index].checksum = 0U;
        states[worker_index].operation_count = operation_count;
        states[worker_index].mode = mode;
        states[worker_index].mixed = mixed;
        worker_index += 1U;
    }
    start_time = std::chrono::steady_clock::now();
    worker_index = 0U;
    while (worker_index < worker_count)
    {
        workers[worker_index] = std::thread(benchmark_worker,
            &states[worker_index]);
        worker_index += 1U;
    }
    worker_index = 0U;
    while (worker_index < worker_count)
    {
        workers[worker_index].join();
        worker_index += 1U;
    }
    finish_time = std::chrono::steady_clock::now();
    (void)pt_rwlock_strategy_destroy(&strategy_lock);
    (void)pt_rwlock_destroy(&native_lock);
    (void)mutex.destroy();
    return (static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::microseconds>(finish_time - start_time).count()));
}

static uint64_t benchmark_median(uint64_t first_value, uint64_t second_value,
    uint64_t third_value)
{
    if (first_value > second_value)
    {
        uint64_t temporary_value = first_value;
        first_value = second_value;
        second_value = temporary_value;
    }
    if (second_value > third_value)
    {
        uint64_t temporary_value = second_value;
        second_value = third_value;
        third_value = temporary_value;
    }
    if (first_value > second_value)
        second_value = first_value;
    return (second_value);
}

static const char *benchmark_mode_name(uint32_t mode)
{
    if (mode == 0U)
        return ("std::mutex");
    if (mode == 1U)
        return ("pt_mutex");
    if (mode == 2U)
        return ("pthread_rwlock");
    return ("custom_rwlock");
}

void efficiency_run_pthread_rwlock_probe(void)
{
    uint32_t mode;
    uint32_t worker_count;
    uint64_t first_read;
    uint64_t second_read;
    uint64_t third_read;
    uint64_t first_write;
    uint64_t second_write;
    uint64_t third_write;
    uint64_t first_concurrent;
    uint64_t second_concurrent;
    uint64_t third_concurrent;
    uint64_t first_mixed;
    uint64_t second_mixed;
    uint64_t third_mixed;

    benchmark_initialize_snapshot();
    std::printf("[PERFORMANCE] RW-lock comparison\n");
    mode = 0U;
    while (mode < 4U)
    {
        worker_count = 1U;
        while (worker_count <= 16U)
        {
            first_read = benchmark_sequential(mode, FT_FALSE);
            second_read = benchmark_sequential(mode, FT_FALSE);
            third_read = benchmark_sequential(mode, FT_FALSE);
            first_write = benchmark_sequential(mode, FT_TRUE);
            second_write = benchmark_sequential(mode, FT_TRUE);
            third_write = benchmark_sequential(mode, FT_TRUE);
            first_concurrent = benchmark_concurrent(mode, worker_count,
                    FT_FALSE);
            second_concurrent = benchmark_concurrent(mode, worker_count,
                    FT_FALSE);
            third_concurrent = benchmark_concurrent(mode, worker_count,
                    FT_FALSE);
            first_mixed = benchmark_concurrent(mode, worker_count, FT_TRUE);
            second_mixed = benchmark_concurrent(mode, worker_count, FT_TRUE);
            third_mixed = benchmark_concurrent(mode, worker_count, FT_TRUE);
            std::printf("[PERFORMANCE] mode=%s workers=%u sequential-read=%"
                PRIu64 " us sequential-write=%" PRIu64
                " us concurrent-read=%" PRIu64 " us mixed-95-5=%" PRIu64
                " us\n", benchmark_mode_name(mode), worker_count,
                benchmark_median(first_read, second_read, third_read),
                benchmark_median(first_write, second_write, third_write),
                benchmark_median(first_concurrent, second_concurrent,
                    third_concurrent),
                benchmark_median(first_mixed, second_mixed, third_mixed));
            worker_count *= 2U;
        }
        mode += 1U;
    }
    return ;
}

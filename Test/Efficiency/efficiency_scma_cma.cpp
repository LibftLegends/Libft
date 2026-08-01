#include <cstdint>
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/SCMA/SCMA.hpp"
#include "../../Modules/Time/time.hpp"

void efficiency_run_windows_mutex_probe(void);

static const ft_size_t EFFICIENCY_BLOCK_COUNT = 512;
static const ft_size_t EFFICIENCY_BLOCK_SIZE = 128;
static const ft_size_t EFFICIENCY_ROUNDS = 8;
static const ft_size_t EFFICIENCY_MUTEX_OPERATIONS = 1000000;
static const ft_size_t EFFICIENCY_SAMPLE_COUNT = 5;
static const char *EFFICIENCY_JSON_PATH = "performance_benchmarks.json";
static const char *EFFICIENCY_FAILED_JSON_PATH =
    "performance_benchmarks_failed.json";

struct s_allocator_result
{
    uint64_t elapsed_microseconds;
    uint64_t allocation_microseconds;
    uint64_t access_microseconds;
    uint64_t release_microseconds;
    ft_bool passed;
    uint64_t samples[EFFICIENCY_SAMPLE_COUNT];
    uint64_t percentile_95_microseconds;
};

struct s_mutex_result
{
    uint64_t elapsed_microseconds;
    ft_size_t lock_operations;
    ft_bool passed;
    uint64_t samples[EFFICIENCY_SAMPLE_COUNT];
    uint64_t percentile_95_microseconds;
};

static s_allocator_result sample_allocator_benchmark(
    s_allocator_result (*benchmark)(void))
{
    s_allocator_result result;
    s_allocator_result measured[EFFICIENCY_SAMPLE_COUNT];
    uint64_t sorted[EFFICIENCY_SAMPLE_COUNT];
    ft_bool all_passed;
    ft_size_t index;

    (void)benchmark();
    all_passed = FT_TRUE;
    index = 0;
    while (index < EFFICIENCY_SAMPLE_COUNT)
    {
        measured[index] = benchmark();
        sorted[index] = measured[index].elapsed_microseconds;
        if (measured[index].passed != FT_TRUE)
            all_passed = FT_FALSE;
        index++;
    }
    std::sort(sorted, sorted + EFFICIENCY_SAMPLE_COUNT);
    index = 0;
    while (index < EFFICIENCY_SAMPLE_COUNT
        && measured[index].elapsed_microseconds
            != sorted[EFFICIENCY_SAMPLE_COUNT / 2])
        index++;
    result = measured[index];
    result.percentile_95_microseconds = sorted[EFFICIENCY_SAMPLE_COUNT - 1];
    result.passed = all_passed;
    index = 0;
    while (index < EFFICIENCY_SAMPLE_COUNT)
    {
        result.samples[index] = sorted[index];
        index++;
    }
    return (result);
}

static s_mutex_result sample_mutex_benchmark(
    s_mutex_result (*benchmark)(void))
{
    s_mutex_result result;
    s_mutex_result sample;
    uint64_t sorted[EFFICIENCY_SAMPLE_COUNT];
    ft_bool all_passed;
    ft_size_t index;

    (void)benchmark();
    all_passed = FT_TRUE;
    index = 0;
    while (index < EFFICIENCY_SAMPLE_COUNT)
    {
        sample = benchmark();
        sorted[index] = sample.elapsed_microseconds;
        if (sample.passed != FT_TRUE)
            all_passed = FT_FALSE;
        if (index == EFFICIENCY_SAMPLE_COUNT / 2)
            result = sample;
        index++;
    }
    std::sort(sorted, sorted + EFFICIENCY_SAMPLE_COUNT);
    result.elapsed_microseconds = sorted[EFFICIENCY_SAMPLE_COUNT / 2];
    result.percentile_95_microseconds = sorted[EFFICIENCY_SAMPLE_COUNT - 1];
    result.passed = all_passed;
    index = 0;
    while (index < EFFICIENCY_SAMPLE_COUNT)
    {
        result.samples[index] = sorted[index];
        index++;
    }
    return (result);
}

static s_mutex_result benchmark_std_mutex(void)
{
    std::mutex mutex;
    volatile uint64_t protected_value;
    t_active_clock clock;
    s_mutex_result result;
    ft_size_t operation_index;

    protected_value = 0;
    result.passed = FT_TRUE;
    result.lock_operations = 0;
    time_active_clock_init(&clock);
    (void)time_active_clock_start(&clock);
    operation_index = 0;
    while (operation_index < EFFICIENCY_MUTEX_OPERATIONS)
    {
        mutex.lock();
        protected_value += 1;
        mutex.unlock();
        result.lock_operations += 1;
        operation_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    if (protected_value != EFFICIENCY_MUTEX_OPERATIONS)
        result.passed = FT_FALSE;
    return (result);
}

static s_mutex_result benchmark_pt_mutex(void)
{
    pt_mutex mutex;
    volatile uint64_t protected_value;
    t_active_clock clock;
    s_mutex_result result;
    ft_size_t operation_index;

    protected_value = 0;
    result.passed = FT_TRUE;
    result.lock_operations = 0;
    if (mutex.initialize() != 0)
    {
        result.passed = FT_FALSE;
        result.elapsed_microseconds = 0;
        return (result);
    }
    time_active_clock_init(&clock);
    (void)time_active_clock_start(&clock);
    operation_index = 0;
    while (operation_index < EFFICIENCY_MUTEX_OPERATIONS)
    {
        if (mutex.lock() != 0)
        {
            result.passed = FT_FALSE;
            break ;
        }
        protected_value += 1;
        if (mutex.unlock() != 0)
        {
            result.passed = FT_FALSE;
            break ;
        }
        result.lock_operations += 1;
        operation_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    if (protected_value != result.lock_operations)
        result.passed = FT_FALSE;
    (void)mutex.destroy();
    return (result);
}

static s_mutex_result benchmark_pt_lock_tracking(void)
{
    t_active_clock clock;
    s_mutex_result result;
    ft_size_t operation_index;
    pt_thread_id_type thread_identifier;
    const void *mutex_pointer;

    result.passed = FT_TRUE;
    result.lock_operations = 0;
    thread_identifier = pt_thread_self();
    mutex_pointer = static_cast<const void *>(&result);
    time_active_clock_init(&clock);
    (void)time_active_clock_start(&clock);
    operation_index = 0;
    while (operation_index < EFFICIENCY_MUTEX_OPERATIONS)
    {
        if (pt_lock_tracking::notify_acquired(thread_identifier,
                mutex_pointer) != FT_ERR_SUCCESS)
        {
            result.passed = FT_FALSE;
            break ;
        }
        if (pt_lock_tracking::notify_released(thread_identifier,
                mutex_pointer) != FT_ERR_SUCCESS)
        {
            result.passed = FT_FALSE;
            break ;
        }
        result.lock_operations += 1;
        operation_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    return (result);
}

static s_mutex_result benchmark_std_recursive_mutex(void)
{
    std::recursive_mutex mutex;
    volatile uint64_t protected_value;
    t_active_clock clock;
    s_mutex_result result;
    ft_size_t operation_index;

    protected_value = 0;
    result.passed = FT_TRUE;
    result.lock_operations = 0;
    time_active_clock_init(&clock);
    (void)time_active_clock_start(&clock);
    operation_index = 0;
    while (operation_index < EFFICIENCY_MUTEX_OPERATIONS)
    {
        mutex.lock();
        mutex.lock();
        protected_value += 1;
        mutex.unlock();
        mutex.unlock();
        result.lock_operations += 2;
        operation_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    if (protected_value * 2 != result.lock_operations)
        result.passed = FT_FALSE;
    return (result);
}

static s_mutex_result benchmark_pt_recursive_mutex(void)
{
    pt_recursive_mutex mutex;
    volatile uint64_t protected_value;
    t_active_clock clock;
    s_mutex_result result;
    ft_size_t operation_index;

    protected_value = 0;
    result.passed = FT_TRUE;
    result.lock_operations = 0;
    if (mutex.initialize() != 0)
    {
        result.passed = FT_FALSE;
        result.elapsed_microseconds = 0;
        return (result);
    }
    time_active_clock_init(&clock);
    (void)time_active_clock_start(&clock);
    operation_index = 0;
    while (operation_index < EFFICIENCY_MUTEX_OPERATIONS)
    {
        if (mutex.lock() != 0 || mutex.lock() != 0)
        {
            result.passed = FT_FALSE;
            break ;
        }
        protected_value += 1;
        if (mutex.unlock() != 0 || mutex.unlock() != 0)
        {
            result.passed = FT_FALSE;
            break ;
        }
        result.lock_operations += 2;
        operation_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    if (protected_value * 2 != result.lock_operations)
        result.passed = FT_FALSE;
    (void)mutex.destroy();
    return (result);
}

static void print_mutex_result(const char *name, s_mutex_result result,
    uint64_t baseline_microseconds)
{
    double relative_percent;
    const char *status;

    relative_percent = 0.0;
    status = "FAIL";
    if (result.passed == FT_TRUE)
        status = "PASS";
    if (result.elapsed_microseconds != 0)
        relative_percent = static_cast<double>(baseline_microseconds)
            / static_cast<double>(result.elapsed_microseconds) * 100.0;
    std::printf("[PERFORMANCE] %s: %" PRIu64
        " us (lock_ops=%zu; %.2f%% of std baseline) status=%s\n",
        name, result.elapsed_microseconds,
        static_cast<std::size_t>(result.lock_operations), relative_percent,
        status);
}

static void efficiency_fill_payload(unsigned char *payload, ft_size_t size,
    unsigned char value)
{
    if (payload == ft_nullptr)
        return ;
    std::memset(payload, static_cast<int>(value),
        static_cast<std::size_t>(size));
    return ;
}

static ft_bool efficiency_verify_payload(const unsigned char *payload,
    ft_size_t size, unsigned char value)
{
    ft_size_t index;

    if (payload == ft_nullptr)
        return (FT_FALSE);
    index = 0;
    while (index < size)
    {
        if (payload[index] != value)
            return (FT_FALSE);
        index += 1;
    }
    return (FT_TRUE);
}

static s_allocator_result benchmark_malloc(void)
{
    void *blocks[EFFICIENCY_BLOCK_COUNT];
    t_active_clock clock;
    t_active_clock allocation_clock;
    t_active_clock access_clock;
    t_active_clock release_clock;
    s_allocator_result result;
    ft_size_t round_index;
    ft_size_t block_index;
    ft_bool passed;

    std::memset(blocks, 0, sizeof(blocks));
    passed = FT_TRUE;
    time_active_clock_init(&clock);
    time_active_clock_init(&allocation_clock);
    time_active_clock_init(&access_clock);
    time_active_clock_init(&release_clock);
    (void)time_active_clock_start(&clock);
    round_index = 0;
    while (round_index < EFFICIENCY_ROUNDS)
    {
        block_index = 0;
        (void)time_active_clock_resume(&allocation_clock);
        while (block_index < EFFICIENCY_BLOCK_COUNT)
        {
            blocks[block_index] = std::malloc(
                static_cast<std::size_t>(EFFICIENCY_BLOCK_SIZE));
            if (blocks[block_index] == ft_nullptr)
            {
                passed = FT_FALSE;
                break ;
            }
            block_index += 1;
        }
        (void)time_active_clock_stop(&allocation_clock);
        (void)time_active_clock_resume(&access_clock);
        {
            ft_size_t access_index;

            access_index = 0;
            while (access_index < block_index)
            {
                efficiency_fill_payload(
                    static_cast<unsigned char *>(blocks[access_index]),
                    EFFICIENCY_BLOCK_SIZE, static_cast<unsigned char>(0x5A));
                if (!efficiency_verify_payload(
                        static_cast<unsigned char *>(blocks[access_index]),
                        EFFICIENCY_BLOCK_SIZE,
                        static_cast<unsigned char>(0x5A)))
                    passed = FT_FALSE;
                access_index += 1;
            }
        }
        (void)time_active_clock_stop(&access_clock);
        (void)time_active_clock_resume(&release_clock);
        while (block_index > 0)
        {
            block_index -= 1;
            std::free(blocks[block_index]);
            blocks[block_index] = ft_nullptr;
        }
        (void)time_active_clock_stop(&release_clock);
        if (passed == FT_FALSE)
            break ;
        round_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    result.allocation_microseconds = time_active_clock_report(&allocation_clock);
    result.access_microseconds = time_active_clock_report(&access_clock);
    result.release_microseconds = time_active_clock_report(&release_clock);
    result.passed = passed;
    return (result);
}

static s_allocator_result benchmark_cma(void)
{
    void *blocks[EFFICIENCY_BLOCK_COUNT];
    t_active_clock clock;
    t_active_clock allocation_clock;
    t_active_clock access_clock;
    t_active_clock release_clock;
    s_allocator_result result;
    ft_size_t round_index;
    ft_size_t block_index;
    ft_bool passed;

    std::memset(blocks, 0, sizeof(blocks));
    passed = FT_TRUE;
    time_active_clock_init(&clock);
    time_active_clock_init(&allocation_clock);
    time_active_clock_init(&access_clock);
    time_active_clock_init(&release_clock);
    (void)time_active_clock_start(&clock);
    round_index = 0;
    while (round_index < EFFICIENCY_ROUNDS)
    {
        block_index = 0;
        (void)time_active_clock_resume(&allocation_clock);
        while (block_index < EFFICIENCY_BLOCK_COUNT)
        {
            blocks[block_index] = cma_malloc(EFFICIENCY_BLOCK_SIZE);
            if (blocks[block_index] == ft_nullptr)
            {
                passed = FT_FALSE;
                break ;
            }
            block_index += 1;
        }
        (void)time_active_clock_stop(&allocation_clock);
        (void)time_active_clock_resume(&access_clock);
        {
            ft_size_t access_index;

            access_index = 0;
            while (access_index < block_index)
            {
                efficiency_fill_payload(
                    static_cast<unsigned char *>(blocks[access_index]),
                    EFFICIENCY_BLOCK_SIZE, static_cast<unsigned char>(0x5A));
                if (!efficiency_verify_payload(
                        static_cast<unsigned char *>(blocks[access_index]),
                        EFFICIENCY_BLOCK_SIZE,
                        static_cast<unsigned char>(0x5A)))
                    passed = FT_FALSE;
                access_index += 1;
            }
        }
        (void)time_active_clock_stop(&access_clock);
        (void)time_active_clock_resume(&release_clock);
        while (block_index > 0)
        {
            block_index -= 1;
            cma_free(blocks[block_index]);
            blocks[block_index] = ft_nullptr;
        }
        (void)time_active_clock_stop(&release_clock);
        if (passed == FT_FALSE)
            break ;
        round_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    result.allocation_microseconds = time_active_clock_report(&allocation_clock);
    result.access_microseconds = time_active_clock_report(&access_clock);
    result.release_microseconds = time_active_clock_report(&release_clock);
    result.passed = passed;
    return (result);
}

static s_allocator_result benchmark_scma(void)
{
    scma_handle blocks[EFFICIENCY_BLOCK_COUNT];
    scma_write_request write_requests[EFFICIENCY_BLOCK_COUNT];
    scma_read_request read_requests[EFFICIENCY_BLOCK_COUNT];
    unsigned char payload[EFFICIENCY_BLOCK_SIZE];
    unsigned char readback[EFFICIENCY_BLOCK_COUNT][EFFICIENCY_BLOCK_SIZE];
    t_active_clock clock;
    t_active_clock allocation_clock;
    t_active_clock access_clock;
    t_active_clock release_clock;
    s_allocator_result result;
    ft_size_t round_index;
    ft_size_t block_index;
    ft_bool passed;

    std::memset(blocks, 0, sizeof(blocks));
    efficiency_fill_payload(payload, EFFICIENCY_BLOCK_SIZE,
        static_cast<unsigned char>(0x5A));
    passed = FT_TRUE;
    time_active_clock_init(&clock);
    time_active_clock_init(&allocation_clock);
    time_active_clock_init(&access_clock);
    time_active_clock_init(&release_clock);
    (void)time_active_clock_start(&clock);
    round_index = 0;
    while (round_index < EFFICIENCY_ROUNDS)
    {
        block_index = 0;
        (void)time_active_clock_resume(&allocation_clock);
        while (block_index < EFFICIENCY_BLOCK_COUNT)
        {
            blocks[block_index] = scma_allocate(EFFICIENCY_BLOCK_SIZE);
            if (scma_handle_is_valid(blocks[block_index]) != FT_TRUE)
            {
                passed = FT_FALSE;
                break ;
            }
            write_requests[block_index] = {blocks[block_index], 0, payload,
                EFFICIENCY_BLOCK_SIZE};
            read_requests[block_index] = {blocks[block_index], 0,
                readback[block_index], EFFICIENCY_BLOCK_SIZE};
            block_index += 1;
        }
        (void)time_active_clock_stop(&allocation_clock);
        std::memset(readback, 0, sizeof(readback));
        (void)time_active_clock_resume(&access_clock);
        if (passed == FT_TRUE
            && scma_write_batch(write_requests, block_index) != FT_ERR_SUCCESS)
            passed = FT_FALSE;
        if (passed == FT_TRUE
            && scma_read_batch(read_requests, block_index) != FT_ERR_SUCCESS)
            passed = FT_FALSE;
        {
            ft_size_t access_index;

            access_index = 0;
            while (access_index < block_index)
            {
                if (!efficiency_verify_payload(readback[access_index],
                        EFFICIENCY_BLOCK_SIZE,
                        static_cast<unsigned char>(0x5A)))
                    passed = FT_FALSE;
                access_index += 1;
            }
        }
        (void)time_active_clock_stop(&access_clock);
        (void)time_active_clock_resume(&release_clock);
        while (block_index > 0)
        {
            block_index -= 1;
            if (scma_free(blocks[block_index]) != FT_ERR_SUCCESS)
                passed = FT_FALSE;
            blocks[block_index] = scma_invalid_handle();
        }
        (void)time_active_clock_stop(&release_clock);
        if (passed == FT_FALSE)
            break ;
        round_index += 1;
    }
    (void)time_active_clock_stop(&clock);
    result.elapsed_microseconds = time_active_clock_report(&clock);
    result.allocation_microseconds = time_active_clock_report(&allocation_clock);
    result.access_microseconds = time_active_clock_report(&access_clock);
    result.release_microseconds = time_active_clock_report(&release_clock);
    result.passed = passed;
    return (result);
}

static void print_result(const char *name, s_allocator_result result,
    uint64_t malloc_microseconds)
{
    double relative_percent;
    const char *status;

    relative_percent = 0.0;
    status = "FAIL";
    if (result.passed == FT_TRUE)
        status = "PASS";
    if (result.elapsed_microseconds != 0)
        relative_percent = static_cast<double>(malloc_microseconds)
            / static_cast<double>(result.elapsed_microseconds) * 100.0;
    std::printf("[PERFORMANCE] %s: %" PRIu64
        " us (alloc=%" PRIu64 " us, access=%" PRIu64
        " us, release=%" PRIu64 " us; %.2f%% of malloc) status=%s\n",
        name, result.elapsed_microseconds, result.allocation_microseconds,
        result.access_microseconds, result.release_microseconds,
        relative_percent, status);
}

static double efficiency_average_microseconds(uint64_t elapsed_microseconds,
    ft_size_t instance_count)
{
    if (instance_count == 0)
        return (0.0);
    return (static_cast<double>(elapsed_microseconds)
        / static_cast<double>(instance_count));
}

static ft_bool efficiency_within_threshold(const char *name,
    uint64_t measured_microseconds, uint64_t baseline_microseconds)
{
    const char *threshold_text;
    char *end_pointer;
    double threshold_percent;
    double maximum_microseconds;

    threshold_text = std::getenv("LIBFT_PERFORMANCE_MAX_SLOWDOWN_PERCENT");
    if (threshold_text == NULL || threshold_text[0] == '\0')
        return (FT_TRUE);
    end_pointer = NULL;
    threshold_percent = std::strtod(threshold_text, &end_pointer);
    if (end_pointer == threshold_text || *end_pointer != '\0'
        || threshold_percent < 0.0 || baseline_microseconds == 0)
        return (FT_TRUE);
    maximum_microseconds = static_cast<double>(baseline_microseconds)
        * (1.0 + threshold_percent / 100.0);
    if (static_cast<double>(measured_microseconds) <= maximum_microseconds)
        return (FT_TRUE);
    std::fprintf(stderr, "[PERFORMANCE] %s exceeded the configured %.2f%% "
        "slowdown threshold\n", name, threshold_percent);
    return (FT_FALSE);
}

static uint64_t efficiency_baseline(const char *environment_name,
    uint64_t fallback_microseconds)
{
    const char *baseline_text;
    char *end_pointer;
    unsigned long long parsed_value;

    baseline_text = std::getenv(environment_name);
    if (baseline_text == NULL || baseline_text[0] == '\0')
        return (fallback_microseconds);
    end_pointer = NULL;
    parsed_value = std::strtoull(baseline_text, &end_pointer, 10);
    if (end_pointer == baseline_text || *end_pointer != '\0'
        || parsed_value == 0)
        return (fallback_microseconds);
    return (static_cast<uint64_t>(parsed_value));
}

static const char *efficiency_compiler_name(void)
{
#if defined(_MSC_VER)
    return ("msvc");
#elif defined(__clang__)
    return ("clang");
#elif defined(__GNUC__)
    return ("gcc");
#else
    return ("unknown");
#endif
}

static const char *efficiency_platform_name(void)
{
#if defined(_WIN32)
    return ("windows");
#elif defined(__APPLE__)
    return ("macos");
#elif defined(__linux__)
    return ("linux");
#else
    return ("unknown");
#endif
}

static const char *efficiency_cpu_architecture(void)
{
#if defined(_M_X64) || defined(__x86_64__)
    return ("x86_64");
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ("arm64");
#elif defined(_M_IX86) || defined(__i386__)
    return ("x86");
#else
    return ("unknown");
#endif
}

static void efficiency_write_allocator_json(FILE *output, const char *name,
    s_allocator_result result, ft_size_t instances, ft_bool trailing_comma)
{
    std::fprintf(output, "    \"%s\": {\"instances\": %zu, "
        "\"average_microseconds\": %.6f, "
        "\"allocation_average_microseconds\": %.6f, "
        "\"access_average_microseconds\": %.6f, "
        "\"release_average_microseconds\": %.6f, "
        "\"median_microseconds\": %" PRIu64 ", "
        "\"p95_microseconds\": %" PRIu64 ", "
        "\"samples_microseconds\": [%" PRIu64 ", %" PRIu64 ", %" PRIu64
        ", %" PRIu64 ", %" PRIu64 "], \"passed\": %s}%s\n",
        name, static_cast<std::size_t>(instances),
        efficiency_average_microseconds(result.elapsed_microseconds, instances),
        efficiency_average_microseconds(result.allocation_microseconds, instances),
        efficiency_average_microseconds(result.access_microseconds, instances),
        efficiency_average_microseconds(result.release_microseconds, instances),
        result.elapsed_microseconds, result.percentile_95_microseconds,
        result.samples[0], result.samples[1], result.samples[2],
        result.samples[3], result.samples[4],
        result.passed == FT_TRUE ? "true" : "false",
        trailing_comma == FT_TRUE ? "," : "");
    return ;
}

static void efficiency_write_mutex_json(FILE *output, const char *name,
    s_mutex_result result, ft_bool trailing_comma)
{
    std::fprintf(output, "    \"%s\": {\"lock_operations\": %zu, "
        "\"microseconds_per_lock_unlock_level\": %.9f, "
        "\"median_microseconds\": %" PRIu64 ", "
        "\"p95_microseconds\": %" PRIu64 ", "
        "\"samples_microseconds\": [%" PRIu64 ", %" PRIu64 ", %" PRIu64
        ", %" PRIu64 ", %" PRIu64 "], \"passed\": %s}%s\n",
        name, static_cast<std::size_t>(result.lock_operations),
        efficiency_average_microseconds(result.elapsed_microseconds,
            result.lock_operations), result.elapsed_microseconds,
        result.percentile_95_microseconds,
        result.samples[0], result.samples[1], result.samples[2],
        result.samples[3], result.samples[4],
        result.passed == FT_TRUE ? "true" : "false",
        trailing_comma == FT_TRUE ? "," : "");
    return ;
}

static int32_t write_performance_json(
    const char *path,
    ft_bool all_benchmarks_passed,
    s_allocator_result malloc_result,
    s_allocator_result cma_result,
    s_allocator_result scma_result,
    s_mutex_result std_mutex_result,
    s_mutex_result pt_mutex_result,
    s_mutex_result pt_lock_tracking_result,
    s_mutex_result std_recursive_mutex_result,
    s_mutex_result pt_recursive_mutex_result)
{
    const ft_size_t allocator_instances = EFFICIENCY_BLOCK_COUNT
        * EFFICIENCY_ROUNDS;
    FILE *output;

    (void)std::remove(EFFICIENCY_JSON_PATH);
    (void)std::remove(EFFICIENCY_FAILED_JSON_PATH);
    output = std::fopen(path, "w");
    if (output == NULL)
    {
        std::fprintf(stderr, "Could not write %s\n", path);
        return (1);
    }
    std::fprintf(output, "{\n  \"status\": \"%s\",\n"
        "  \"unit\": \"microseconds\",\n"
        "  \"sampling\": {\"warmup_runs\": 1, \"measured_runs\": %zu},\n"
        "  \"build\": {\"compiler\": \"%s\", \"platform\": \"%s\", "
        "\"cpu_architecture\": \"%s\"},\n"
        "  \"benchmarks\": {\n",
        all_benchmarks_passed == FT_TRUE ? "passed" : "failed",
        static_cast<std::size_t>(EFFICIENCY_SAMPLE_COUNT),
        efficiency_compiler_name(), efficiency_platform_name(),
        efficiency_cpu_architecture());
    efficiency_write_allocator_json(output, "malloc", malloc_result,
        allocator_instances, FT_TRUE);
    efficiency_write_allocator_json(output, "cma", cma_result,
        allocator_instances, FT_TRUE);
    efficiency_write_allocator_json(output, "scma", scma_result,
        allocator_instances, FT_TRUE);
    efficiency_write_mutex_json(output, "std_mutex", std_mutex_result,
        FT_TRUE);
    efficiency_write_mutex_json(output, "pt_mutex", pt_mutex_result,
        FT_TRUE);
    efficiency_write_mutex_json(output, "pt_lock_tracking",
        pt_lock_tracking_result, FT_TRUE);
    efficiency_write_mutex_json(output, "std_recursive_mutex",
        std_recursive_mutex_result, FT_TRUE);
    efficiency_write_mutex_json(output, "pt_recursive_mutex",
        pt_recursive_mutex_result, FT_FALSE);
    std::fprintf(output, "  }\n}\n");
    if (std::fclose(output) != 0)
    {
        std::fprintf(stderr, "Could not finish %s\n", path);
        return (1);
    }
    return (0);
}

int main(void)
{
    s_allocator_result malloc_result;
    s_allocator_result cma_result;
    s_allocator_result scma_result;
    s_mutex_result std_mutex_result;
    s_mutex_result pt_mutex_result;
    s_mutex_result pt_lock_tracking_result;
    s_mutex_result std_recursive_mutex_result;
    s_mutex_result pt_recursive_mutex_result;
    int32_t scma_initialization_result;
    int32_t exit_code;
    ft_bool all_benchmarks_passed;

    scma_initialization_result = scma_initialize(1024 * 1024);
    if (scma_initialization_result != FT_ERR_SUCCESS)
        return (1);
    malloc_result = sample_allocator_benchmark(benchmark_malloc);
    cma_result = sample_allocator_benchmark(benchmark_cma);
    scma_result = sample_allocator_benchmark(benchmark_scma);
    std_mutex_result = sample_mutex_benchmark(benchmark_std_mutex);
    pt_mutex_result = sample_mutex_benchmark(benchmark_pt_mutex);
    pt_lock_tracking_result = sample_mutex_benchmark(
            benchmark_pt_lock_tracking);
    std_recursive_mutex_result = sample_mutex_benchmark(
            benchmark_std_recursive_mutex);
    pt_recursive_mutex_result = sample_mutex_benchmark(
            benchmark_pt_recursive_mutex);
    if (efficiency_within_threshold("CMA", cma_result.elapsed_microseconds,
            efficiency_baseline("LIBFT_PERFORMANCE_BASELINE_CMA_US",
                malloc_result.elapsed_microseconds)) != FT_TRUE)
        cma_result.passed = FT_FALSE;
    if (efficiency_within_threshold("SCMA", scma_result.elapsed_microseconds,
            efficiency_baseline("LIBFT_PERFORMANCE_BASELINE_SCMA_US",
                malloc_result.elapsed_microseconds)) != FT_TRUE)
        scma_result.passed = FT_FALSE;
    if (efficiency_within_threshold("pt_mutex",
            pt_mutex_result.elapsed_microseconds,
            efficiency_baseline("LIBFT_PERFORMANCE_BASELINE_PT_MUTEX_US",
                std_mutex_result.elapsed_microseconds)) != FT_TRUE)
        pt_mutex_result.passed = FT_FALSE;
    if (efficiency_within_threshold("pt_recursive_mutex",
            pt_recursive_mutex_result.elapsed_microseconds,
            efficiency_baseline(
                "LIBFT_PERFORMANCE_BASELINE_PT_RECURSIVE_MUTEX_US",
                std_recursive_mutex_result.elapsed_microseconds)) != FT_TRUE)
        pt_recursive_mutex_result.passed = FT_FALSE;
    efficiency_run_windows_mutex_probe();
    scma_shutdown();
    print_result("malloc", malloc_result, malloc_result.elapsed_microseconds);
    print_result("CMA", cma_result, malloc_result.elapsed_microseconds);
    print_result("SCMA", scma_result, malloc_result.elapsed_microseconds);
    print_mutex_result("std::mutex", std_mutex_result,
        std_mutex_result.elapsed_microseconds);
    print_mutex_result("pt_mutex", pt_mutex_result,
        std_mutex_result.elapsed_microseconds);
    print_mutex_result("pt_lock_tracking", pt_lock_tracking_result,
        std_mutex_result.elapsed_microseconds);
    print_mutex_result("std::recursive_mutex", std_recursive_mutex_result,
        std_recursive_mutex_result.elapsed_microseconds);
    print_mutex_result("pt_recursive_mutex", pt_recursive_mutex_result,
        std_recursive_mutex_result.elapsed_microseconds);
    exit_code = FT_ERR_SUCCESS;
    all_benchmarks_passed = FT_TRUE;
    if (malloc_result.passed != FT_TRUE || cma_result.passed != FT_TRUE
        || scma_result.passed != FT_TRUE
        || std_mutex_result.passed != FT_TRUE
        || pt_mutex_result.passed != FT_TRUE
        || pt_lock_tracking_result.passed != FT_TRUE
        || std_recursive_mutex_result.passed != FT_TRUE
        || pt_recursive_mutex_result.passed != FT_TRUE)
    {
        all_benchmarks_passed = FT_FALSE;
        exit_code = 1;
    }
    if (write_performance_json(all_benchmarks_passed == FT_TRUE
            ? EFFICIENCY_JSON_PATH : EFFICIENCY_FAILED_JSON_PATH,
            all_benchmarks_passed, malloc_result, cma_result, scma_result,
            std_mutex_result, pt_mutex_result, pt_lock_tracking_result,
            std_recursive_mutex_result, pt_recursive_mutex_result) != 0)
        exit_code = 1;
    return (exit_code);
}

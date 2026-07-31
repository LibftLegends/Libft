#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/SCMA/SCMA.hpp"
#include "../../Modules/Time/time.hpp"

static const ft_size_t EFFICIENCY_BLOCK_COUNT = 512;
static const ft_size_t EFFICIENCY_BLOCK_SIZE = 128;
static const ft_size_t EFFICIENCY_ROUNDS = 8;
static const ft_size_t EFFICIENCY_MUTEX_OPERATIONS = 1000000;
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
};

struct s_mutex_result
{
    uint64_t elapsed_microseconds;
    ft_size_t lock_operations;
    ft_bool passed;
};

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

static void efficiency_write_allocator_json(FILE *output, const char *name,
    s_allocator_result result, ft_size_t instances, ft_bool trailing_comma)
{
    std::fprintf(output, "    \"%s\": {\"instances\": %zu, "
        "\"average_microseconds\": %.6f, "
        "\"allocation_average_microseconds\": %.6f, "
        "\"access_average_microseconds\": %.6f, "
        "\"release_average_microseconds\": %.6f, \"passed\": %s}%s\n",
        name, static_cast<std::size_t>(instances),
        efficiency_average_microseconds(result.elapsed_microseconds, instances),
        efficiency_average_microseconds(result.allocation_microseconds, instances),
        efficiency_average_microseconds(result.access_microseconds, instances),
        efficiency_average_microseconds(result.release_microseconds, instances),
        result.passed == FT_TRUE ? "true" : "false",
        trailing_comma == FT_TRUE ? "," : "");
    return ;
}

static void efficiency_write_mutex_json(FILE *output, const char *name,
    s_mutex_result result, ft_size_t instances, ft_bool trailing_comma)
{
    std::fprintf(output, "    \"%s\": {\"instances\": %zu, "
        "\"average_microseconds\": %.6f, \"passed\": %s}%s\n",
        name, static_cast<std::size_t>(instances),
        efficiency_average_microseconds(result.elapsed_microseconds, instances),
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
    s_mutex_result std_recursive_mutex_result,
    s_mutex_result pt_recursive_mutex_result)
{
    const ft_size_t allocator_instances = EFFICIENCY_BLOCK_COUNT
        * EFFICIENCY_ROUNDS;
    const ft_size_t mutex_instances = EFFICIENCY_MUTEX_OPERATIONS;
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
        "  \"unit\": \"microseconds\",\n  \"benchmarks\": {\n",
        all_benchmarks_passed == FT_TRUE ? "passed" : "failed");
    efficiency_write_allocator_json(output, "malloc", malloc_result,
        allocator_instances, FT_TRUE);
    efficiency_write_allocator_json(output, "cma", cma_result,
        allocator_instances, FT_TRUE);
    efficiency_write_allocator_json(output, "scma", scma_result,
        allocator_instances, FT_TRUE);
    efficiency_write_mutex_json(output, "std_mutex", std_mutex_result,
        mutex_instances, FT_TRUE);
    efficiency_write_mutex_json(output, "pt_mutex", pt_mutex_result,
        mutex_instances, FT_TRUE);
    efficiency_write_mutex_json(output, "std_recursive_mutex",
        std_recursive_mutex_result, mutex_instances, FT_TRUE);
    efficiency_write_mutex_json(output, "pt_recursive_mutex",
        pt_recursive_mutex_result, mutex_instances, FT_FALSE);
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
    s_mutex_result std_recursive_mutex_result;
    s_mutex_result pt_recursive_mutex_result;
    int32_t scma_initialization_result;
    int32_t exit_code;
    ft_bool all_benchmarks_passed;

    scma_initialization_result = scma_initialize(1024 * 1024);
    if (scma_initialization_result != FT_ERR_SUCCESS)
        return (1);
    malloc_result = benchmark_malloc();
    cma_result = benchmark_cma();
    scma_result = benchmark_scma();
    std_mutex_result = benchmark_std_mutex();
    pt_mutex_result = benchmark_pt_mutex();
    std_recursive_mutex_result = benchmark_std_recursive_mutex();
    pt_recursive_mutex_result = benchmark_pt_recursive_mutex();
    scma_shutdown();
    print_result("malloc", malloc_result, malloc_result.elapsed_microseconds);
    print_result("CMA", cma_result, malloc_result.elapsed_microseconds);
    print_result("SCMA", scma_result, malloc_result.elapsed_microseconds);
    print_mutex_result("std::mutex", std_mutex_result,
        std_mutex_result.elapsed_microseconds);
    print_mutex_result("pt_mutex", pt_mutex_result,
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
        || std_recursive_mutex_result.passed != FT_TRUE
        || pt_recursive_mutex_result.passed != FT_TRUE)
    {
        all_benchmarks_passed = FT_FALSE;
        exit_code = 1;
    }
    if (write_performance_json(all_benchmarks_passed == FT_TRUE
            ? EFFICIENCY_JSON_PATH : EFFICIENCY_FAILED_JSON_PATH,
            all_benchmarks_passed, malloc_result, cma_result, scma_result,
            std_mutex_result, pt_mutex_result, std_recursive_mutex_result,
            pt_recursive_mutex_result) != 0)
        exit_code = 1;
    return (exit_code);
}

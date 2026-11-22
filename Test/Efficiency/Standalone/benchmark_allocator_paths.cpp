#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/SCMA/SCMA.hpp"
#include "../../../Modules/SCMA/scma_internal.hpp"
#include "../../../Modules/Errno/errno.hpp"
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

static int32_t benchmark_cma()
{
    constexpr uint32_t ITERATIONS = 24000U;
    void *pointer = cma_malloc(32U);
    if (pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    static_cast<uint8_t *>(pointer)[0U] = 0xA5U;
    auto started = benchmark_clock::now();
    uint32_t iteration = 0U;

    while (iteration < ITERATIONS)
    {
        void *replacement = cma_realloc(pointer, 32U);
        if (replacement == ft_nullptr)
        {
            cma_free(pointer);
            return (FT_ERR_NO_MEMORY);
        }
        pointer = replacement;
        ++iteration;
    }
    print_average("cma_realloc_same_32", started, ITERATIONS);
    iteration = 0U;
    started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        const ft_size_t size = 32U + (iteration & 1U) * 32U;
        void *replacement = cma_realloc(pointer, size);
        if (replacement == ft_nullptr)
        {
            cma_free(pointer);
            return (FT_ERR_NO_MEMORY);
        }
        pointer = replacement;
        ++iteration;
    }
    print_average("cma_realloc_alternate_32_64", started, ITERATIONS);
    const uint8_t first_byte = static_cast<uint8_t *>(pointer)[0U];
    cma_free(pointer);
    if (first_byte != 0xA5U)
        return (FT_ERR_INVALID_STATE);
    iteration = 0U;
    started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        pointer = cma_malloc(64U);
        if (pointer == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        cma_free(pointer);
        ++iteration;
    }
    print_average("cma_malloc_free_64", started, ITERATIONS);
    return (FT_ERR_SUCCESS);
}

static int32_t benchmark_cma_resize_with_live_blocks()
{
    constexpr uint32_t BLOCK_COUNT = 96U;
    constexpr uint32_t ITERATIONS = 120000U;
    constexpr ft_size_t BLOCK_SIZE = 1024U;
    void *blocks[BLOCK_COUNT];
    uint32_t allocated = 0U;
    int32_t error_code = FT_ERR_SUCCESS;
    while (allocated < BLOCK_COUNT)
    {
        blocks[allocated] = cma_malloc(BLOCK_SIZE);
        if (blocks[allocated] == ft_nullptr)
        {
            error_code = FT_ERR_NO_MEMORY;
            break ;
        }
        static_cast<uint8_t *>(blocks[allocated])[0U] = 42U;
        ++allocated;
    }
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        const uint32_t index = 64U + iteration % 32U;
        void *replacement = cma_realloc(blocks[index], BLOCK_SIZE);
        if (replacement == ft_nullptr)
            error_code = FT_ERR_NO_MEMORY;
        else
            blocks[index] = replacement;
        ++iteration;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("cma_realloc_same_1024_with_live_blocks", started, ITERATIONS);
    uint32_t index = 0U;
    while (index < allocated)
    {
        if (error_code == FT_ERR_SUCCESS
            && static_cast<uint8_t *>(blocks[index])[0U] != 42U)
            error_code = FT_ERR_INVALID_STATE;
        cma_free(blocks[index]);
        ++index;
    }
    return (error_code);
}

static int32_t benchmark_scma()
{
    constexpr uint32_t ITERATIONS = 250000U;
    int32_t error_code = scma_initialize(4096U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const scma_handle handle = scma_allocate(sizeof(uint32_t));
    uint32_t iteration = 0U;
    uint32_t observed = 0U;
    const auto started = benchmark_clock::now();

    while (iteration < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        error_code = scma_write(handle, 0U, &iteration, sizeof(iteration));
        if (error_code == FT_ERR_SUCCESS)
            error_code = scma_read(handle, 0U, &observed, sizeof(observed));
        if (error_code == FT_ERR_SUCCESS && observed != iteration)
            error_code = FT_ERR_INVALID_STATE;
        ++iteration;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("scma_single_write_read", started, ITERATIONS);
    scma_shutdown();
    return (error_code);
}

static int32_t benchmark_scma_fragmented()
{
    constexpr uint32_t BLOCK_COUNT = 512U;
    constexpr uint32_t ITERATIONS = 3000U;
    constexpr ft_size_t BLOCK_SIZE = 64U;
    scma_handle handles[BLOCK_COUNT];
    int32_t error_code = scma_initialize(BLOCK_COUNT * BLOCK_SIZE);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint32_t index = 0U;
    const auto appended_at = benchmark_clock::now();
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        handles[index] = scma_allocate(BLOCK_SIZE);
        if (!scma_handle_is_valid(handles[index]))
            error_code = FT_ERR_NO_MEMORY;
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("scma_append_block", appended_at, BLOCK_COUNT);
    index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        error_code = scma_free(handles[index]);
        index += 2U;
    }
    index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        const scma_handle handle = scma_allocate(BLOCK_SIZE);
        if (!scma_handle_is_valid(handle))
            error_code = FT_ERR_NO_MEMORY;
        else
            error_code = scma_free(handle);
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("scma_fragmented_allocate_free", started, ITERATIONS);
    scma_shutdown();
    return (error_code);
}

static int32_t benchmark_scma_compaction()
{
    constexpr uint32_t BLOCK_COUNT = 2048U;
    constexpr ft_size_t BLOCK_SIZE = 64U;
    scma_handle handles[BLOCK_COUNT];
    int32_t error_code = scma_initialize(BLOCK_COUNT * BLOCK_SIZE);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint32_t index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        handles[index] = scma_allocate(BLOCK_SIZE);
        error_code = scma_write(handles[index], 0U, &index, sizeof(index));
        ++index;
    }
    index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        error_code = scma_free(handles[index]);
        index += 2U;
    }
    index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        handles[index] = scma_allocate(BLOCK_SIZE / 2U);
        error_code = scma_write(handles[index], 0U, &index, sizeof(index));
        index += 2U;
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        const auto started = benchmark_clock::now();
        error_code = scma_compact();
        if (error_code == FT_ERR_SUCCESS)
            print_average("scma_out_of_order_compaction", started, 1U);
    }
    index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        uint32_t payload = 0U;
        error_code = scma_read(handles[index], 0U, &payload, sizeof(payload));
        if (error_code == FT_ERR_SUCCESS && payload != index)
            error_code = FT_ERR_INVALID_STATE;
        ++index;
    }
    scma_shutdown();
    return (error_code);
}

static int32_t benchmark_scma_tail_resize()
{
    constexpr uint32_t BLOCK_COUNT = 8192U;
    constexpr uint32_t ITERATIONS = 10000U;
    constexpr ft_size_t BLOCK_SIZE = 64U;
    constexpr uint32_t PAYLOAD = 42U;
    int32_t error_code = scma_initialize((BLOCK_COUNT + 1U) * BLOCK_SIZE);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    scma_handle tail{};
    uint32_t index = 0U;
    while (index < BLOCK_COUNT && error_code == FT_ERR_SUCCESS)
    {
        tail = scma_allocate(BLOCK_SIZE);
        if (!scma_handle_is_valid(tail))
            error_code = FT_ERR_NO_MEMORY;
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = scma_write(tail, 0U, &PAYLOAD, sizeof(PAYLOAD));
    index = 0U;
    const auto started = benchmark_clock::now();
    while (index < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        error_code = scma_resize(tail, BLOCK_SIZE + (index & 1U) * 16U);
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        print_average("scma_tail_resize_64_80", started, ITERATIONS);
        uint32_t observed = 0U;
        error_code = scma_read(tail, 0U, &observed, sizeof(observed));
        if (error_code == FT_ERR_SUCCESS
            && (observed != PAYLOAD || scma_get_size(tail) != 80U))
            error_code = FT_ERR_INVALID_STATE;
    }
    scma_shutdown();
    return (error_code);
}

int main()
{
    if (benchmark_cma() != FT_ERR_SUCCESS
        || benchmark_cma_resize_with_live_blocks() != FT_ERR_SUCCESS
        || benchmark_scma() != FT_ERR_SUCCESS
        || benchmark_scma_fragmented() != FT_ERR_SUCCESS
        || benchmark_scma_compaction() != FT_ERR_SUCCESS
        || benchmark_scma_tail_resize() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}

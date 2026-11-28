#include "../../../Modules/Buffer/byte_buffer.hpp"
#include "../../../Modules/SCMA/SCMA.hpp"
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

static int32_t benchmark_self_append()
{
    constexpr uint32_t ITERATIONS = 40000U;
    constexpr ft_size_t INPUT_SIZE = 4096U;
    uint8_t payload[INPUT_SIZE] = {};
    ft_byte_buffer buffer;
    int32_t error_code = buffer.initialize(INPUT_SIZE * 2U, FT_FALSE);
    uint32_t iteration = 0U;
    payload[INPUT_SIZE - 1U] = 0xA5U;
    const auto started = benchmark_clock::now();

    while (iteration < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        error_code = buffer.clear();
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.append(payload, INPUT_SIZE);
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.append_buffer(buffer);
        if (error_code == FT_ERR_SUCCESS && (buffer.size() != INPUT_SIZE * 2U
                || buffer.data()[INPUT_SIZE * 2U - 1U] != 0xA5U))
            error_code = FT_ERR_INVALID_STATE;
        ++iteration;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("buffer_self_append_4_kib", started, ITERATIONS);
    return (error_code);
}

static int32_t benchmark_scma_accessor()
{
    constexpr uint32_t ITERATIONS = 500000U;
    int32_t error_code = scma_initialize(4096U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const scma_handle handle = scma_allocate(sizeof(uint32_t));
    scma_handle_accessor<uint32_t> accessor;
    error_code = accessor.initialize();
    if (error_code == FT_ERR_SUCCESS && !accessor.bind(handle))
        error_code = accessor.get_error();
    uint32_t iteration = 0U;
    uint32_t observed = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        error_code = accessor.write_at(iteration, 0U);
        if (error_code == FT_ERR_SUCCESS)
            error_code = accessor.read_at(observed, 0U);
        if (error_code == FT_ERR_SUCCESS && observed != iteration)
            error_code = FT_ERR_INVALID_STATE;
        ++iteration;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("scma_typed_write_read", started, ITERATIONS);
    scma_shutdown();
    return (error_code);
}

static int32_t benchmark_buffer_slice()
{
    constexpr uint32_t ITERATIONS = 20000U;
    constexpr ft_size_t INPUT_SIZE = 4096U;
    uint8_t payload[INPUT_SIZE] = {};
    ft_byte_buffer source;
    ft_byte_buffer output;
    int32_t error_code = source.initialize(INPUT_SIZE, FT_FALSE);
    payload[INPUT_SIZE - 1U] = 0xA5U;
    if (error_code == FT_ERR_SUCCESS)
        error_code = source.append(payload, INPUT_SIZE);
    const auto started = benchmark_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS && error_code == FT_ERR_SUCCESS)
    {
        error_code = source.slice(0U, INPUT_SIZE, output);
        if (error_code == FT_ERR_SUCCESS && (output.size() != INPUT_SIZE
                || output.data()[INPUT_SIZE - 1U] != 0xA5U))
            error_code = FT_ERR_INVALID_STATE;
        ++iteration;
    }
    if (error_code == FT_ERR_SUCCESS)
        print_average("buffer_slice_4_kib", started, ITERATIONS);
    return (error_code);
}

int main()
{
    if (benchmark_self_append() != FT_ERR_SUCCESS
        || benchmark_buffer_slice() != FT_ERR_SUCCESS
        || benchmark_scma_accessor() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}

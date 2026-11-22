#include "../../../Modules/Compression/compression.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <string>

using benchmark_clock = std::chrono::steady_clock;
static constexpr ft_size_t INPUT_SIZE = 65536U;
static constexpr uint32_t ITERATIONS = 400U;

static void print_average(const char *name, benchmark_clock::time_point started)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(ITERATIONS));
    return ;
}

static int32_t benchmark_base64_decode(const char *name, const uint8_t *input,
    const uint8_t *encoded, ft_size_t encoded_size)
{
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_size_t size = 0U;
        uint8_t *result = ft_base64_decode(encoded, encoded_size, &size);
        const ft_bool valid = result != ft_nullptr && size == INPUT_SIZE
            && (iteration != ITERATIONS - 1U
                || ft_memcmp(result, input, INPUT_SIZE) == 0);
        cma_free(result);
        if (valid == FT_FALSE)
            return (1);
        ++iteration;
    }
    print_average(name, started);
    return (0);
}

static int32_t benchmark_base64()
{
    uint8_t input[INPUT_SIZE];
    ft_size_t index = 0U;
    while (index < INPUT_SIZE)
    {
        input[index] = static_cast<uint8_t>(index);
        ++index;
    }
    ft_size_t encoded_size = 0U;
    uint8_t *encoded = ft_base64_encode(input, INPUT_SIZE, &encoded_size);
    if (encoded == ft_nullptr)
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_size_t size = 0U;
        uint8_t *result = ft_base64_encode(input, INPUT_SIZE, &size);
        const ft_bool valid = result != ft_nullptr && size == encoded_size;
        cma_free(result);
        if (valid == FT_FALSE)
        {
            cma_free(encoded);
            return (1);
        }
        ++iteration;
    }
    print_average("compression_base64_encode_64_kib", started);
    std::string wrapped;
    index = 0U;
    while (index < encoded_size)
    {
        const ft_size_t remaining = encoded_size - index;
        ft_size_t line_length = 76U;
        if (remaining < line_length)
            line_length = remaining;
        wrapped.append(reinterpret_cast<const char *>(encoded + index), line_length);
        wrapped.append("\r\n");
        index += line_length;
    }
    const ft_bool failed = benchmark_base64_decode("compression_base64_decode_64_kib",
            input, encoded, encoded_size)
        || benchmark_base64_decode("compression_base64_decode_wrapped_64_kib",
            input, reinterpret_cast<const uint8_t *>(wrapped.data()), wrapped.size());
    cma_free(encoded);
    return (failed);
}

static int32_t benchmark_zlib_vector()
{
    ft_string input;
    ft_vector<uint8_t> output;
    ft_string roundtrip;
    if (input.initialize("Small payload for repeated compression.")
        || output.initialize() || roundtrip.initialize())
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        if (ft_compress_string_to_vector(input, output) != 0)
            return (1);
        ++iteration;
    }
    print_average("compression_zlib_small_string_to_vector", started);
    if (ft_decompress_vector_to_string(output, roundtrip) != 0
        || input.size() != roundtrip.size()
        || ft_memcmp(input.c_str(), roundtrip.c_str(), input.size()) != 0)
        return (1);
    return (0);
}

int main()
{
    return (benchmark_base64() || benchmark_zlib_vector());
}

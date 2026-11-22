#include "../../../Modules/Encoding/encoding.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
using encoder = char *(*)(const uint8_t *, ft_size_t);
using decoder = uint8_t *(*)(const char *, ft_size_t, ft_size_t *);

static constexpr ft_size_t INPUT_SIZE = 65536U;
static constexpr uint32_t ITERATIONS = 300U;

static void print_average(const char *name, const char *operation,
    benchmark_clock::time_point started)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s_%s_64_kib ns/op=%.2f\n", name, operation,
        elapsed / static_cast<double>(ITERATIONS));
    return ;
}

static char *encode_hex(const uint8_t *input, ft_size_t size)
{
    return (encoding_hex_encode(input, size, FT_FALSE));
}

static int32_t benchmark_codec(const char *name, const uint8_t *input,
    encoder encode, decoder decode)
{
    char *encoded = encode(input, INPUT_SIZE);
    if (encoded == ft_nullptr)
        return (1);
    const ft_size_t encoded_size = ft_strlen_size_t(encoded);
    uint32_t iteration = 0U;
    auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        char *result = encode(input, INPUT_SIZE);
        if (result == ft_nullptr)
        {
            cma_free(encoded);
            return (1);
        }
        cma_free(result);
        ++iteration;
    }
    print_average(name, "encode", started);
    iteration = 0U;
    started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_size_t decoded_size = 0U;
        uint8_t *result = decode(encoded, encoded_size, &decoded_size);
        if (result == ft_nullptr || decoded_size != INPUT_SIZE)
        {
            cma_free(result);
            cma_free(encoded);
            return (1);
        }
        const ft_bool valid = iteration != ITERATIONS - 1U
            || ft_memcmp(result, input, INPUT_SIZE) == 0;
        cma_free(result);
        if (valid == FT_FALSE)
        {
            cma_free(encoded);
            return (1);
        }
        ++iteration;
    }
    print_average(name, "decode", started);
    cma_free(encoded);
    return (0);
}

int main()
{
    uint8_t binary[INPUT_SIZE];
    uint8_t ascii[INPUT_SIZE];
    ft_size_t index = 0U;
    while (index < INPUT_SIZE)
    {
        binary[index] = static_cast<uint8_t>(index);
        ascii[index] = static_cast<uint8_t>('a' + (index % 26U));
        ++index;
    }
    if (benchmark_codec("base64", binary, encoding_base64_encode,
            encoding_base64_decode)
        || benchmark_codec("base32", binary, encoding_base32_encode,
            encoding_base32_decode)
        || benchmark_codec("hex", binary, encode_hex, encoding_hex_decode)
        || benchmark_codec("percent_binary", binary, encoding_percent_encode,
            encoding_percent_decode)
        || benchmark_codec("percent_ascii", ascii, encoding_percent_encode,
            encoding_percent_decode))
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        if (encoding_utf8_validate(reinterpret_cast<const char *>(ascii),
                INPUT_SIZE) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    print_average("utf8_ascii", "validate", started);
    return (0);
}

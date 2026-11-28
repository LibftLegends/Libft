#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static void append_individual_digits(ft_big_number &number, const char *digits)
{
    while (*digits != '\0')
    {
        number.append_digit(*digits);
        digits += 1;
    }
    return ;
}

static void append_text(ft_big_number &number, const char *digits)
{
    number.append(digits);
    return ;
}

static void append_unsigned_blocks(ft_big_number &number, const char *)
{
    uint32_t index = 0U;
    while (index < 128U)
    {
        number.append_unsigned(UINT64_MAX);
        index += 1U;
    }
    return ;
}

static int32_t measure_append(const char *label, const char *digits, ft_size_t expected_length,
    void (*append)(ft_big_number &, const char *))
{
    constexpr uint32_t ITERATIONS = 48U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    double elapsed = 0.0;
    while (index < ITERATIONS)
    {
        ft_big_number number;
        if (number.initialize() != FT_ERR_SUCCESS)
            return (1);
        const auto started = std::chrono::steady_clock::now();
        append(number, digits);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - started).count();
        if (ft_big_number::get_error() != FT_ERR_SUCCESS || number.size() != expected_length)
            return (1);
        const char *output = number.c_str();
        while (*output != '\0')
        {
            checksum = checksum * 31U + static_cast<uint8_t>(*output);
            output += 1;
        }
        index += 1U;
    }
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/batch, %.2f CMA allocations/batch, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    constexpr uint32_t DIGIT_COUNT = 2048U;
    char digits[DIGIT_COUNT + 1U];
    uint32_t index = 0U;
    while (index < DIGIT_COUNT)
    {
        digits[index] = static_cast<char>('0' + ((index + 1U) % 10U));
        index += 1U;
    }
    digits[index] = '\0';
    if (measure_append("individual digits", digits, DIGIT_COUNT, append_individual_digits)
        || measure_append("text append", digits, DIGIT_COUNT, append_text))
        return (1);
    return (measure_append("unsigned appends", ft_nullptr, 128U * 20U, append_unsigned_blocks));
}

#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_multiplication(const char *label, uint32_t left_size,
    uint32_t right_size, uint32_t iterations)
{
    ft_big_number left;
    ft_big_number right;
    ft_big_number product;
    char left_digits[161];
    char right_digits[161];
    uint32_t index = 0U;
    while (index < left_size)
    {
        left_digits[index] = static_cast<char>('0' + ((index * 7U + 8U) % 10U));
        index += 1U;
    }
    left_digits[left_size] = '\0';
    index = 0U;
    while (index < right_size)
    {
        right_digits[index] = static_cast<char>('0' + ((index * 3U + 9U) % 10U));
        index += 1U;
    }
    right_digits[right_size] = '\0';
    if (left.initialize() != FT_ERR_SUCCESS || right.initialize() != FT_ERR_SUCCESS
        || product.initialize() != FT_ERR_SUCCESS)
        return (1);
    left.assign(left_digits);
    right.assign(right_digits);
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    const auto started = std::chrono::steady_clock::now();
    index = 0U;
    while (index < iterations)
    {
        product = left * right;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    const char *digit = product.c_str();
    while (*digit != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(*digit);
        digit += 1;
    }
    std::printf("%s: %.2f ns/product, %.2f CMA allocations/product, checksum %" FT_PRIu64 "\n",
        label, elapsed / iterations,
        static_cast<double>(allocations_after - allocations_before) / iterations, checksum);
    return (0);
}

int32_t main()
{
    if (measure_multiplication("single-digit multiplier", 80U, 1U, 4000U)
        || measure_multiplication("short product", 20U, 20U, 4000U)
        || measure_multiplication("grouped product", 64U, 64U, 1600U))
        return (1);
    return (measure_multiplication("heap-scratch product", 160U, 160U, 640U));
}

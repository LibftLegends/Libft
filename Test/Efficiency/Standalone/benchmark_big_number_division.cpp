#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static uint64_t hash_result(const ft_big_number &number, uint64_t checksum)
{
    checksum = checksum * 31U + number.is_negative();
    const char *digits = number.c_str();
    while (*digits != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(*digits);
        digits += 1;
    }
    return (checksum);
}

static int32_t measure_division(const char *label, const char *divisor_digits)
{
    constexpr uint32_t ITERATIONS = 80U;
    ft_big_number numerator;
    ft_big_number divisor;
    ft_big_number result;
    if (numerator.initialize() != FT_ERR_SUCCESS || divisor.initialize() != FT_ERR_SUCCESS
        || result.initialize() != FT_ERR_SUCCESS)
        return (1);
    numerator.assign("-98765432101234567890987654321012345678909876543210123456789098765432101234567890");
    if (ft_big_number::get_error() != FT_ERR_SUCCESS)
        return (1);
    divisor.assign(divisor_digits);
    if (ft_big_number::get_error() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        result = numerator / divisor;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum = hash_result(result, checksum);
        result = numerator % divisor;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum = hash_result(result, checksum);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/pair, %.2f CMA allocations/pair, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_division("single-digit divisor", "7") != 0)
        return (1);
    return (measure_division("wide divisor", "-1000000000000000123"));
}

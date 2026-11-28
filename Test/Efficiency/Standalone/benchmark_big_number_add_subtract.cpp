#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_signed_arithmetic(const char *label, const char *left_digits,
    const char *right_digits)
{
    constexpr uint32_t ITERATIONS = 30000U;
    ft_big_number left;
    ft_big_number right;
    ft_big_number result;
    if (left.initialize() != FT_ERR_SUCCESS || right.initialize() != FT_ERR_SUCCESS
        || result.initialize() != FT_ERR_SUCCESS)
        return (1);
    left.assign(left_digits);
    if (ft_big_number::get_error() != FT_ERR_SUCCESS)
        return (1);
    right.assign(right_digits);
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
        result = left + right;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + static_cast<uint8_t>(result.c_str()[0]) + result.size();
        result = right - left;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + static_cast<uint8_t>(result.c_str()[0]) + result.is_negative();
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/operation, %.2f CMA allocations/operation, checksum %" FT_PRIu64 "\n",
        label, elapsed / (2.0 * ITERATIONS),
        static_cast<double>(allocations_after - allocations_before) / (2.0 * ITERATIONS),
        checksum);
    return (0);
}

int32_t main()
{
    if (measure_signed_arithmetic("20-digit signed arithmetic", "99991234567890123456",
            "92345678901234567890") != 0)
        return (1);
    return (measure_signed_arithmetic("wide signed arithmetic",
        "9223372036854775808123456789092233720368547758081234567890"
        "9223372036854775808123456789092233720368547758081234567890",
        "-18446744073709551616123456781844674407370955161612345678"
        "18446744073709551616123456781844674407370955161612345678"));
}

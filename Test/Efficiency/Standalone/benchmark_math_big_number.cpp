#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_big_math(const char *label, const char *first_digits,
    const char *second_digits, ft_big_number *(*calculate)(const ft_big_number &, const ft_big_number &))
{
    constexpr uint32_t ITERATIONS = 80U;
    ft_big_number first;
    ft_big_number second;
    if (first.initialize() != FT_ERR_SUCCESS || second.initialize() != FT_ERR_SUCCESS)
        return (1);
    first.assign(first_digits);
    second.assign(second_digits);
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_big_number *result = calculate(first, second);
        if (result == ft_nullptr)
            return (1);
        checksum = checksum * 31U + result->is_negative();
        const char *digits = result->c_str();
        while (*digits != '\0')
        {
            checksum = checksum * 31U + static_cast<uint8_t>(*digits);
            digits += 1;
        }
        delete result;
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/operation, %.2f CMA allocations/operation, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_big_math("coprime GCD", "-3928413764606871165730",
            "6356306993006846248183", math_big_gcd))
        return (1);
    return (measure_big_math("large common factor LCM",
        "-3647175416613920351978753101033344378685538538950467392342133666030",
        "5901253787014381058843742121224782001416869458843457462882945365313",
        math_big_lcm));
}

#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_conversion(const char *label, const char *input, int32_t base)
{
    constexpr uint32_t ITERATIONS = 100U;
    ft_big_number number;
    if (number.initialize() != FT_ERR_SUCCESS)
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
        if (number.assign_base(input, base) != FT_ERR_SUCCESS)
            return (1);
        ft_string *converted = number.to_string_base(base);
        if (converted == ft_nullptr)
            return (1);
        const int32_t error_code = converted->get_error();
        const char *output = converted->c_str();
        const char *expected = input;
        while (*output != '\0' && *output == *expected)
        {
            checksum = checksum * 31U + static_cast<uint8_t>(*output);
            output += 1;
            expected += 1;
        }
        const ft_bool matches = *output == '\0' && *expected == '\0';
        delete converted;
        if (error_code != FT_ERR_SUCCESS || matches == FT_FALSE)
            return (1);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/round-trip, %.2f CMA allocations/round-trip, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_conversion("short hexadecimal", "DEADBEEF", 16)
        || measure_conversion("wide hexadecimal",
            "-F000ABCD000512345678900000012EF0020D4F000ABCD000512345678900000012EF0020D4", 16)
        || measure_conversion("ternary", "-1021020021200100002120102100012001200212212010200122012020001", 3))
        return (1);
    return (measure_conversion("decimal", "-9223372036854775808123456789092233720368547758081234567890", 10));
}

#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_power(const char *label, const char *exponent_digits)
{
    constexpr uint32_t ITERATIONS = 32U;
    ft_big_number base;
    ft_big_number exponent;
    ft_big_number modulus;
    if (base.initialize() != FT_ERR_SUCCESS || exponent.initialize() != FT_ERR_SUCCESS
        || modulus.initialize() != FT_ERR_SUCCESS)
        return (1);
    base.assign("-123456789012345678901234567890");
    exponent.assign(exponent_digits);
    modulus.assign("987654321098765432109876543211");
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_big_number *result = base.mod_pow(exponent, modulus);
        if (result == ft_nullptr)
            return (1);
        const int32_t error_code = ft_big_number::get_error();
        checksum = checksum * 31U + result->is_negative();
        const char *digits = result->c_str();
        while (*digits != '\0')
        {
            checksum = checksum * 31U + static_cast<uint8_t>(*digits);
            digits += 1;
        }
        delete result;
        if (error_code != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/power, %.2f CMA allocations/power, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS, static_cast<double>(after - before) / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_power("exponent 17", "17") != 0)
        return (1);
    return (measure_power("exponent 257", "257"));
}

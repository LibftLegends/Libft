#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_big_number_copy(const char *label, ft_bool reinitialize)
{
    constexpr uint32_t ITERATIONS = 30000U;
    ft_big_number source;
    ft_big_number destination;
    if (source.initialize() != FT_ERR_SUCCESS || destination.initialize() != FT_ERR_SUCCESS)
        return (1);
    source.assign("-9223372036854775808123456789092233720368547758081234567890"
        "9223372036854775808123456789092233720368547758081234567890");
    destination = source;
    if (ft_big_number::get_error() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (reinitialize == FT_TRUE)
        {
            if (destination.initialize(source) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            destination = source;
            if (ft_big_number::get_error() != FT_ERR_SUCCESS)
                return (1);
        }
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = destination.is_negative();
    const char *digit = destination.c_str();
    while (*digit != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(*digit);
        digit += 1;
    }
    std::printf("%s: %.2f ns/copy, %.2f CMA allocations/copy, checksum %" FT_PRIu64 "\n",
        label, elapsed / ITERATIONS,
        static_cast<double>(allocations_after - allocations_before) / ITERATIONS,
        checksum);
    return (0);
}

static int32_t measure_big_number_move()
{
    constexpr uint32_t ITERATIONS = 30000U;
    ft_big_number first;
    ft_big_number second;
    if (first.initialize() != FT_ERR_SUCCESS)
        return (1);
    first.assign("9223372036854775808123456789092233720368547758081234567890");
    const char *original_digits = first.c_str();
    uint32_t index = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (second.initialize() != FT_ERR_SUCCESS || second.move(first) != FT_ERR_SUCCESS
            || first.initialize() != FT_ERR_SUCCESS || first.move(second) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (first.c_str() != original_digits)
        return (1);
    std::printf("move to initialized empty destination: %.2f ns/move\n",
        elapsed / (2.0 * ITERATIONS));
    return (0);
}

int32_t main()
{
    if (measure_big_number_copy("copy assignment", FT_FALSE) != 0)
        return (1);
    if (measure_big_number_copy("copy reinitialization", FT_TRUE) != 0)
        return (1);
    return (measure_big_number_move());
}

#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 30000U;
    ft_big_number left;
    ft_big_number right;
    if (left.initialize() != FT_ERR_SUCCESS || right.initialize() != FT_ERR_SUCCESS)
        return (1);
    left.assign("123456789012345678901234567890");
    right.assign("1");
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    const auto started = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        ft_big_number destination;
        if (destination.initialize() != FT_ERR_SUCCESS)
            return (1);
        destination = left + right;
        if (ft_big_number::get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + destination.size();
        checksum = checksum * 31U + static_cast<uint8_t>(destination.c_str()[29]);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("fresh proxy assignment: %.2f ns/assignment, %.2f CMA allocations/assignment, checksum %" FT_PRIu64 "\n",
        elapsed / ITERATIONS, static_cast<double>(after - before) / ITERATIONS, checksum);
    return (0);
}

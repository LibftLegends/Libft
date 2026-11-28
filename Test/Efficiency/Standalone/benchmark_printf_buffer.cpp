#include "../../../Modules/Printf/printf.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int32_t main(int32_t argument_count, char **)
{
    if (argument_count > 1 && pf_enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    constexpr uint32_t ITERATIONS = 2048U;
    char buffer[48];
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t free_count = 0U;
    if (cma_get_stats(&before, &free_count) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const int32_t length = pf_snprintf(buffer, sizeof(buffer),
            "item=%u status=%s", iteration, "queued");
        if (length < 0 || buffer[0] != 'i')
            return (1);
        checksum += static_cast<uint32_t>(length) + static_cast<uint8_t>(buffer[5]);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &free_count) != FT_ERR_SUCCESS)
        return (1);
    if (argument_count > 1 && pf_disable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    std::printf("format fixed buffer %.2f ns/op, allocations %" PRIu64 ", checksum %" PRIu64 "\n",
        elapsed / ITERATIONS, after - before, checksum);
    return (0);
}

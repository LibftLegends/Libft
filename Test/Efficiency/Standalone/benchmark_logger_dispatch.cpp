#include "../../../Modules/Logger/logger_internal.hpp"
#include "../../../Modules/CPP_class/class_string.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t count_message(const char *, void *user_data)
{
    auto *count = static_cast<uint32_t *>(user_data);
    *count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 4096U;
    uint32_t count = 0U;
    ft_string message;
    if (message.initialize("dispatch benchmark\n") != FT_ERR_SUCCESS
        || ft_log_add_sink(count_message, &count) != FT_ERR_SUCCESS)
        return (1);
    ft_log_process_message(message);
    count = 0U;
    ft_size_t allocations_before = 0U;
    ft_size_t allocations_after = 0U;
    ft_size_t frees = 0U;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        ft_log_process_message(message);
        index += 1U;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS)
        return (1);
    ft_log_remove_sink(count_message, &count);
    std::printf("one sink dispatch: %.2f ns/op, checksum %" PRIu32 "\n",
        nanoseconds / ITERATIONS, count);
    std::printf("CMA allocations: %" PRIu64 "\n", allocations_after - allocations_before);
    return (count != ITERATIONS);
}

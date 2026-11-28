#include "../../../Modules/Logger/logger_internal.hpp"
#include <chrono>
#include <cstdio>

static int32_t count_message(const char *, void *user_data)
{
    auto *count = static_cast<uint32_t *>(user_data);
    *count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 8192U;
    uint32_t delivered = 0U;
    ft_string message;
    if (message.initialize("async staging benchmark") != FT_ERR_SUCCESS
        || ft_log_add_sink(count_message, &delivered) != FT_ERR_SUCCESS)
        return (1);
    ft_log_enable_async(FT_FALSE);
    ft_log_set_async_queue_limit(0U);
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        logger_enqueue_text(LOG_LEVEL_INFO, message, FT_FALSE);
        index += 1U;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    s_log_async_metrics metrics;
    const int32_t error = ft_log_get_async_metrics(&metrics);
    ft_log_enable_async(FT_TRUE);
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(count_message, &delivered);
    if (error != FT_ERR_SUCCESS || metrics.pending_messages != ITERATIONS || delivered != ITERATIONS)
        return (1);
    std::printf("stage async backlog: %.2f ns/op, checksum %u\n", nanoseconds / ITERATIONS, delivered);
    return (0);
}

#include "../../../Modules/Logger/logger_internal.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t benchmark_messages(const char *label)
{
    constexpr uint32_t ITERATIONS = 4096U;
    ft_string text;
    if (text.initialize("ordinary log message") != FT_ERR_SUCCESS)
        return (1);
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t free_count = 0U;
    if (cma_get_stats(&before, &free_count) != FT_ERR_SUCCESS)
        return (1);

    ft_size_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        ft_string message;
        if (logger_prepare_message(LOG_LEVEL_INFO, text, message) != FT_ERR_SUCCESS)
            return (1);
        checksum += message.size();
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &free_count) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%s: %.2f ns/op, allocations %" PRIu64 ", checksum %" PRIu64 "\n",
        label, elapsed / ITERATIONS, after - before, checksum);
    return (0);
}

int main()
{
    if (benchmark_messages("no redaction rules") != 0)
        return (1);
    constexpr const char *PATTERNS[] = {"credential", "signature", "password", "private-key",
        "bearer-token", "access-key", "auth-secret", "account-id"};
    ft_size_t index = 0U;
    while (index < sizeof(PATTERNS) / sizeof(*PATTERNS))
    {
        if (ft_log_add_redaction(PATTERNS[index]) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const int32_t result = benchmark_messages("eight unmatched rules");
    ft_log_clear_redactions();
    return (result);
}

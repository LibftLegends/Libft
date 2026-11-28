#include "../../../Modules/Logger/logger.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t count_message(const char *message, void *user_data)
{
    uint32_t &checksum = *static_cast<uint32_t *>(user_data);
    checksum += static_cast<uint32_t>(ft_strlen_size_t(message));
    return (FT_ERR_SUCCESS);
}

template <typename Operation>
static int32_t benchmark_message(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 4096U;
    uint32_t checksum = 0U;
    if (ft_log_add_sink(count_message, &checksum) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        operation();
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_log_remove_sink(count_message, &checksum);
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    const std::string message = std::string(256U, 'a') + "\n\"quote\"\\path";
    s_log_field field;
    field.key = "request";
    field.value = "request ID";
    ft_log_set_level(LOG_LEVEL_INFO);
    const int32_t status =
        benchmark_message("plain message", [&message]()
        {
            ft_log_info("%s", message.c_str());
            return ;
        })
        || benchmark_message("structured message", [&message, &field]()
        {
            ft_log_info_structured(message.c_str(), &field, 1U);
            return ;
        })
        || benchmark_message("filtered structured message", [&message, &field]()
        {
            ft_log_debug_structured(message.c_str(), &field, 1U);
            return ;
        });
    ft_log_close();
    return (status);
}

#include "../../../Modules/Logger/logger.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>

static int32_t count_context_message(const char *message, void *user_data)
{
    auto *checksum = static_cast<uint32_t *>(user_data);
    *checksum += static_cast<uint32_t>(ft_strlen_size_t(message));
    return (FT_ERR_SUCCESS);
}

int main()
{
    constexpr uint32_t FIELD_COUNT = 24U;
    constexpr uint32_t ITERATIONS = 512U;
    s_log_field fields[FIELD_COUNT];
    uint32_t index = 0U;
    while (index < FIELD_COUNT)
    {
        fields[index].key = "request";
        fields[index].value = "context fields have owned key and value text";
        index += 1U;
    }
    uint32_t checksum = 0U;
    auto start = std::chrono::steady_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        if (ft_log_context_push(fields, FIELD_COUNT) != FT_ERR_SUCCESS)
            return (1);
        ft_log_context_pop(FIELD_COUNT);
        index += 1U;
    }
    double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("push and pop 24 fields: %.2f ns/op, checksum %u\n",
        elapsed / ITERATIONS, index);
    if (ft_log_context_push(fields, FIELD_COUNT) != FT_ERR_SUCCESS
        || ft_log_add_sink(count_context_message, &checksum) != FT_ERR_SUCCESS)
        return (1);
    start = std::chrono::steady_clock::now();
    index = 0U;
    while (index < ITERATIONS)
    {
        ft_log_info_structured("context benchmark", ft_nullptr, 0U);
        index += 1U;
    }
    elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    ft_log_remove_sink(count_context_message, &checksum);
    ft_log_context_clear();
    std::printf("structured log with context: %.2f ns/op, checksum %u\n",
        elapsed / ITERATIONS, checksum);
    return (checksum == 0U);
}

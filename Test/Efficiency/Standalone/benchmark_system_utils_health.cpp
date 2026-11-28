#include "../../../Modules/System_utils/system_utils.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

struct health_benchmark_context
{
    uint32_t check_index;
    uint64_t *checksum;
};

static int32_t record_health_check(void *context_pointer, ft_string &detail)
{
    (void)detail;
    auto *context = static_cast<health_benchmark_context *>(context_pointer);
    *context->checksum = *context->checksum * 31U + context->check_index;
    return (0);
}

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t CHECK_COUNT = 128;
    constexpr uint32_t ITERATIONS = 1000;
    const ft_bool single_check = argument_count == 2 && ft_strcmp(arguments[1], "--single") == 0;
    uint32_t registered_count = CHECK_COUNT;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--few") == 0)
        registered_count = 8;
    uint64_t checksum = 0;
    health_benchmark_context contexts[CHECK_COUNT];
    uint32_t index = 0;
    while (index < registered_count)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "registered-health-check-%03" PRIu32, index);
        contexts[index] = {index + 1, &checksum};
        if (su_health_register_check(name, record_health_check, &contexts[index]) != 0)
            return (1);
        ++index;
    }

    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    index = 0;
    while (index < ITERATIONS)
    {
        ft_size_t count = 0;
        if (single_check)
        {
            if (su_health_run_check("registered-health-check-000", ft_nullptr) != 0)
                return (1);
        }
        else if (su_health_run_checks(ft_nullptr, 0, &count) != 0 || count != registered_count)
            return (1);
        ++index;
    }
    const double microseconds = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("health %.3f us/run allocations=%" PRIu64 " checksum=%" PRIu64 "\n",
        microseconds / ITERATIONS, ft_platform_cast<uint64_t>(after - before), checksum);
    su_health_clear_checks();
    return (0);
}

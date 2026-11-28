#include "../../../Modules/System_utils/system_utils.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_unchanged_capture()
{
    constexpr uint32_t ITERATIONS = 1024U;
    t_su_environment_snapshot snapshot;
    if (su_environment_snapshot_capture(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t count = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (su_environment_snapshot_capture(&snapshot) != FT_ERR_SUCCESS)
            return (1);
        count += snapshot.entries.size();
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("unchanged environment capture %.2f us/op, total entries %" PRIu64 "\n",
        elapsed / ITERATIONS, count);
    return (0);
}

static int32_t measure_restore_one_change()
{
    constexpr uint32_t ITERATIONS = 128U;
    const char *name = "LIBFT_STANDALONE_SNAPSHOT_BENCHMARK";
    t_su_environment_snapshot snapshot;
    if (su_setenv(name, "saved", 1) != FT_ERR_SUCCESS
        || su_environment_snapshot_capture(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (su_setenv(name, "changed", 1) != FT_ERR_SUCCESS
            || su_environment_snapshot_restore(&snapshot) != FT_ERR_SUCCESS)
            return (1);
        const char *value = su_getenv(name);
        if (value == ft_nullptr || ft_strcmp(value, "saved") != 0)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    (void)su_unsetenv(name);
    std::printf("one-variable environment restore %.2f us/op, restores %u\n",
        elapsed / ITERATIONS, iteration);
    return (0);
}

int main()
{
    if (measure_unchanged_capture() != 0)
        return (1);
    return (measure_restore_one_change());
}

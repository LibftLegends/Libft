#include "../../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>

static int32_t runner_probe_success()
{
    return (1);
}

static int32_t runner_probe_failure()
{
    ft_test_fail("intentional failure", __FILE__, __LINE__);
    return (0);
}

int main(int32_t argument_count, char **arguments)
{
    constexpr int32_t TEST_COUNT = 12000;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--failure") == 0)
    {
        if (ft_register_test(runner_probe_failure, "intentional failure",
                "RunnerBenchmark", "failure") != FT_ERR_SUCCESS)
            return (2);
        return (ft_run_registered_tests());
    }
    int32_t index = 0;
    while (index < TEST_COUNT)
    {
        if (ft_register_test(runner_probe_success, "empty test",
                "RunnerBenchmark", "empty") != FT_ERR_SUCCESS)
            return (2);
        ++index;
    }
    const auto start = std::chrono::steady_clock::now();
    const int32_t result = ft_run_registered_tests();
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("runner %.3f ms for %d tests\n", milliseconds, TEST_COUNT);
    return (result);
}

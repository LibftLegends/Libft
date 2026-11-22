#include "../../../Modules/API/api.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr int32_t ITERATIONS = 1000000;
    api_retry_policy policy;
    if (policy.initialize() != FT_ERR_SUCCESS)
        return (1);
    policy.set_initial_delay_ms(10);
    policy.set_max_delay_ms(100);
    policy.set_backoff_multiplier(2);
    policy.set_circuit_breaker_threshold(6);
    const auto start = std::chrono::steady_clock::now();
    int64_t total = 0;
    int32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        policy.set_max_attempts(iteration);
        total += policy.get_max_attempts();
        total += policy.get_initial_delay_ms();
        total += policy.get_max_delay_ms();
        total += policy.get_backoff_multiplier();
        total += policy.get_circuit_breaker_threshold();
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (total != static_cast<int64_t>(ITERATIONS) * (ITERATIONS - 1) / 2
        + static_cast<int64_t>(ITERATIONS) * 118)
        return (1);
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_retry_policy us/op=%.5f\n", microseconds / ITERATIONS);
    return (0);
}

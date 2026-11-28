#include "../../../Modules/API/api_http_internal.hpp"
#include <charconv>
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t DESTINATIONS = 1024;
    constexpr uint32_t ITERATIONS = 30000;
    api_connection_pool_handle handle;
    api_retry_policy policy;
    if (handle.initialize() != FT_ERR_SUCCESS || policy.initialize() != FT_ERR_SUCCESS)
        return (1);
    policy.set_circuit_breaker_threshold(2);
    policy.set_circuit_breaker_cooldown_ms(60000);
    uint32_t index = 0;
    while (index < DESTINATIONS)
    {
        char key[16] = {'k'};
        const auto encoded = std::to_chars(key + 1, key + sizeof(key), index);
        if (encoded.ec != std::errc{}
            || handle.key.assign(key, static_cast<ft_size_t>(encoded.ptr - key)) != FT_ERR_SUCCESS)
            return (1);
        api_retry_circuit_record_failure(handle, &policy);
        ++index;
    }
    api_retry_circuit_record_failure(handle, &policy);
    int32_t error_code = FT_ERR_SUCCESS;
    if (api_retry_circuit_allow(handle, &policy, error_code)
        || error_code != FT_ERR_API_CIRCUIT_OPEN)
        return (1);
    if (handle.key.assign("healthy", 7U) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (!api_retry_circuit_allow(handle, &policy, error_code))
            return (1);
        api_retry_circuit_record_success(handle, &policy);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    api_retry_circuit_reset();
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_retry_healthy us/op=%.4f\n", microseconds / ITERATIONS);
    return (0);
}

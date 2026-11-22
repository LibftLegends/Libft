#include "../../../Modules/API/api_internal.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 300000;
    api_connection_pool_handle handle;
    if (handle.initialize() != FT_ERR_SUCCESS)
        return (1);
    api_connection_pool_set_enabled(FT_FALSE);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (api_connection_pool_acquire(handle, "2001:db8::1", 65535U,
                api_connection_security_mode::TLS, "test-identity"))
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (!(handle.key == "2001:db8::1:65535:tls:test-identity"))
        return (1);
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_connection_pool_key us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}

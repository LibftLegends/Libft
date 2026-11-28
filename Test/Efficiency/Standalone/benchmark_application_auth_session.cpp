#include "../../../Modules/Application/application_auth_service.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t ITERATIONS = 25000;
    constexpr int64_t EXPECTED_TIMESTAMP = 1700000000;
    application_auth_service service;
    ft_string address;
    int64_t timestamp = 0;
    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.parse_login_session_value(
                "fixture-session|192.0.2.7|1700000000", &address,
                timestamp) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    if (timestamp != EXPECTED_TIMESTAMP || address != "192.0.2.7")
        return (2);
    std::printf("%.3f ns/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

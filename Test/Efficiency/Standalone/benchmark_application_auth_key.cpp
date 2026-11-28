#include "../../../Modules/Application/application_auth_service.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t ITERATIONS = 25000;
    const char username[] = "fixture-user-with-a-long-identifier-for-the-authentication-settings-benchmark";
    application_auth_service service;
    ft_string output;
    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.build_user_key(username, output) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    if (output != "application/auth/users/fixture-user-with-a-long-identifier-for-the-authentication-settings-benchmark")
        return (2);
    std::printf("%.3f ns/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

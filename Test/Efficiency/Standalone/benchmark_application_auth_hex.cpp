#include "../../../Modules/Application/application_auth_service.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t ITERATIONS = 50000;
    application_auth_service service;
    ft_string output;
    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.build_login_signal_one_time_password_hash("benchmark-placeholder",
                output) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    if (output.size() != 64)
        return (2);
    std::printf("%.3f ns/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

#include "../../../Modules/Application/application_auth_service.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t ITERATIONS = 50000;
    application_auth_service service;
    char output[65];
    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.build_login_signal_one_time_password_hash("benchmark-placeholder",
                output, sizeof(output)) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    const char expected[] = "671622f5431ff6bc03195b6d6f8f241fd314f6d32e8e399d9dae7610cd85960a";
    if (ft_memcmp(output, expected, sizeof(expected)) != 0)
        return (2);
    std::printf("%.3f ns/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

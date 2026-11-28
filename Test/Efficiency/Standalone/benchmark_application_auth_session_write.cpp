#include "../../../Modules/Application/application_auth_service.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr ft_size_t ITERATIONS = 25000;
    const char token[] = "0123456789abcdef0123456789abcdef";
    const char address[] = "2001:db8:abcd:1234:5678:90ab:cdef:1234";
    application_auth_service service;
    ft_string output;
    ft_size_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (service.build_login_session_value(token, address, 1700000000, output)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const std::chrono::duration<double, std::nano> elapsed =
        std::chrono::steady_clock::now() - start;
    if (output != "0123456789abcdef0123456789abcdef|2001:db8:abcd:1234:5678:90ab:cdef:1234|1700000000")
        return (2);
    std::printf("%.3f ns/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

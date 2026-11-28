#include "../../../Modules/Logger/logger.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    if (ft_log_set_remote_sink("127.0.0.1", 65535U, FT_FALSE) != FT_ERR_SUCCESS)
        return (1);
    ft_log_close();

    constexpr uint32_t ITERATIONS = 1000U;
    uint32_t rejected = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (ft_log_set_remote_sink("invalid-host", 65535U, FT_FALSE) == FT_ERR_INTERNAL)
            rejected += 1U;
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("reject invalid remote address: %.2f us/op, checksum %u\n",
        elapsed / ITERATIONS, rejected);
    if (rejected != ITERATIONS)
        return (1);
    return (0);
}

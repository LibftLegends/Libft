#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/RNG/rng.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_random_bytes(uint32_t length, uint32_t iterations)
{
    uint8_t buffer[4096];
    if (rng_secure_bytes(buffer, length) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (rng_secure_bytes(buffer, length) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("OS RNG, %u bytes: %.3f us per call\n", length, elapsed_us / iterations);
    return (0);
}

int32_t main()
{
    if (measure_random_bytes(32U, 200000U) != 0
        || measure_random_bytes(4096U, 15000U) != 0)
        return (1);
    return (0);
}

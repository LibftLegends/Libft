#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t SCALAR_ITERATIONS = 150000U;
    constexpr uint32_t BATCH_ITERATIONS = 24000U;
    constexpr uint32_t BATCH_SIZE = 257U;
    ft_seed_random_engine(0x4137U);
    uint64_t integer_checksum = 0U;
    float float_checksum = 0.0f;
    uint32_t iteration = 0U;
    const auto scalar_start = std::chrono::steady_clock::now();
    while (iteration < SCALAR_ITERATIONS)
    {
        integer_checksum += static_cast<uint32_t>(ft_random_int());
        iteration += 1U;
    }
    const double scalar_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - scalar_start).count();
    int32_t integers[BATCH_SIZE];
    iteration = 0U;
    const auto integer_start = std::chrono::steady_clock::now();
    while (iteration < BATCH_ITERATIONS)
    {
        if (ft_random_int_vector(0, INT32_MAX, integers, BATCH_SIZE) != FT_ERR_SUCCESS)
            return (1);
        integer_checksum += static_cast<uint32_t>(integers[iteration % BATCH_SIZE]);
        iteration += 1U;
    }
    const double integer_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - integer_start).count();
    float floats[BATCH_SIZE];
    iteration = 0U;
    const auto float_start = std::chrono::steady_clock::now();
    while (iteration < BATCH_ITERATIONS)
    {
        if (ft_random_float_vector(floats, BATCH_SIZE) != FT_ERR_SUCCESS)
            return (1);
        float_checksum += floats[iteration % BATCH_SIZE];
        iteration += 1U;
    }
    const double float_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - float_start).count();
    std::printf("Scalar integer: %.2f ns per value\n", scalar_ns / SCALAR_ITERATIONS);
    std::printf("Batch integer: %.2f ns per value, checksum %" FT_PRIu64 "\n",
        integer_ns / (BATCH_ITERATIONS * BATCH_SIZE), integer_checksum);
    std::printf("Batch float: %.2f ns per value, checksum %.2f\n",
        float_ns / (BATCH_ITERATIONS * BATCH_SIZE), static_cast<double>(float_checksum));
    return (0);
}

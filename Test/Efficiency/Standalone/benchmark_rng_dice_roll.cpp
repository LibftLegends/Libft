#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_rolls(int32_t dice_count)
{
    constexpr uint32_t ITERATIONS = 120000U;
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    ft_seed_random_engine(830173U);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const int32_t total = ft_dice_roll(dice_count, 6);
        if (total < dice_count || total > dice_count * 6)
            return (1);
        checksum += static_cast<uint32_t>(total);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%d dice per call: %.2f ns per call, checksum %" FT_PRIu64 "\n",
        dice_count, elapsed_ns / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (rng_enable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const int32_t single_result = measure_rolls(1);
    const int32_t batch_result = measure_rolls(50);
    if (rng_disable_random_engine_thread_safety() != FT_ERR_SUCCESS
        || single_result != 0 || batch_result != 0)
        return (1);
    return (0);
}

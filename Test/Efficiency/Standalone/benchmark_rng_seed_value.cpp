#include "../../../Modules/CPP_class/class_string.hpp"
#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 500000U;
    constexpr char TEXT[] = "voxel-world-2026";
    ft_string seed_text;
    if (seed_text.initialize() != FT_ERR_SUCCESS
        || seed_text.assign(TEXT, sizeof(TEXT) - 1U) != FT_ERR_SUCCESS
        || seed_text.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const uint64_t expected = static_cast<uint64_t>(rng_seed_value(TEXT)) * ITERATIONS;
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        checksum += static_cast<uint64_t>(rng_seed_value(seed_text));
        index += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("seed string: %.2f ns per hash, checksum %" FT_PRIu64 "\n",
        elapsed_ns / ITERATIONS, checksum);
    if (checksum != expected)
        return (1);
    return (0);
}

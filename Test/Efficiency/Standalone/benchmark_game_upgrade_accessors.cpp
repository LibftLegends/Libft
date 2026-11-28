#include "../../../Modules/Game/game_upgrade.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_accessors(ft_bool thread_safe)
{
    constexpr uint32_t ITERATIONS = 200000U;
    game_upgrade upgrade;
    if (upgrade.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (thread_safe && upgrade.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        upgrade.add_modifier1(1);
        checksum += static_cast<uint64_t>(upgrade.get_modifier1());
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != static_cast<uint64_t>(ITERATIONS) * (ITERATIONS + 1U) / 2U)
        return (1);
    std::printf("access_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(thread_safe),
        elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_accessors(FT_FALSE) != 0)
        return (1);
    return (measure_accessors(FT_TRUE));
}

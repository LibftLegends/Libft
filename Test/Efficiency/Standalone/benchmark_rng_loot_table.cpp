#include "../../../Modules/RNG/loot_table.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr ft_size_t ENTRY_COUNT = 4096U;
    constexpr uint32_t ITERATIONS = 22000U;
    uint32_t items[ENTRY_COUNT];
    ft_loot_table<uint32_t> table;
    if (table.initialize() != FT_ERR_SUCCESS || table.enable_thread_safety() != FT_ERR_SUCCESS
        || rng_enable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    const auto add_start = std::chrono::steady_clock::now();
    while (index < ENTRY_COUNT)
    {
        items[index] = static_cast<uint32_t>(index);
        const int32_t weight = static_cast<int32_t>(index % 17U) + 1;
        if (table.add_element(&items[index], weight, static_cast<int32_t>(index % 4U)) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    const double add_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - add_start).count();
    std::printf("loot add: %.2f ns\n", add_ns / ENTRY_COUNT);
    ft_seed_random_engine(137599U);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const Pair<int32_t, uint32_t *> result = table.get_random_loot();
        if (result.key != FT_ERR_SUCCESS || result.value == ft_nullptr)
            return (1);
        checksum += *result.value;
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("loot draw: %.2f ns, checksum %" FT_PRIu64 ", next draw %d\n",
        elapsed_ns / ITERATIONS, checksum, ft_random_int());
    if (rng_disable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}

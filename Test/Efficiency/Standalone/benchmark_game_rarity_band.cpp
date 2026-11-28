#include "../../../Modules/Game/game_economy_table.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_fetch(ft_bool locked)
{
    constexpr uint32_t ITERATIONS = 250000U;
    game_economy_table table;
    game_rarity_band source;
    game_rarity_band destination;
    if (table.initialize() != FT_ERR_SUCCESS || source.initialize(7, 1.25) != FT_ERR_SUCCESS
        || destination.initialize() != FT_ERR_SUCCESS
        || table.register_rarity_band(source) != FT_ERR_SUCCESS)
        return (1);
    if (locked == FT_TRUE)
    {
        auto &bands = table.get_rarity_bands();
        auto *entry = bands.find(7);
        if (entry == bands.end() || entry->value.enable_thread_safety() != FT_ERR_SUCCESS
            || destination.enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (table.fetch_rarity_band(7, destination) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t rarity = destination.get_rarity();
    const double multiplier = destination.get_value_multiplier();
    if (rarity != 7 || multiplier != 1.25)
        return (1);
    std::printf("fetch_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(locked),
        elapsed / ITERATIONS, rarity + static_cast<int32_t>(multiplier * 100.0));
    return (0);
}

int32_t main()
{
    if (measure_fetch(FT_FALSE) != 0)
        return (1);
    return (measure_fetch(FT_TRUE));
}

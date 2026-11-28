#include "../../../Modules/Game/game_economy_table.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_fetch(ft_bool lock_source)
{
    constexpr uint32_t ITERATIONS = 250000U;
    game_economy_table table;
    game_vendor_profile source;
    game_vendor_profile destination;
    if (table.initialize() != FT_ERR_SUCCESS
        || source.initialize(7, 1.25, 0.75, 0.125) != FT_ERR_SUCCESS
        || destination.initialize() != FT_ERR_SUCCESS
        || table.register_vendor_profile(source) != FT_ERR_SUCCESS)
        return (1);
    if (lock_source == FT_TRUE)
    {
        auto &profiles = table.get_vendor_profiles();
        auto *entry = profiles.find(7);
        if (entry == profiles.end() || entry->value.enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (table.fetch_vendor_profile(7, destination) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t vendor_id = destination.get_vendor_id();
    const double markup = destination.get_buy_markup();
    const double multiplier = destination.get_sell_multiplier();
    const double tax = destination.get_tax_rate();
    if (vendor_id != 7 || markup != 1.25 || multiplier != 0.75 || tax != 0.125)
        return (1);
    const int32_t checksum = vendor_id + static_cast<int32_t>((markup + multiplier + tax) * 1000.0);
    std::printf("fetch_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(lock_source),
        elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_fetch(FT_FALSE) != 0)
        return (1);
    return (measure_fetch(FT_TRUE));
}

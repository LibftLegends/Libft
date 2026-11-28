#include "../../../Modules/CardGame/card_game_resources.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    const ft_bool small = argument_count == 2 && ft_strcmp(arguments[1], "--small") == 0;
    card_game_resource_pool pools[FT_CARD_GAME_MAX_RESOURCE_POOLS]{};
    card_game_resource_unit units[FT_CARD_GAME_MAX_RESOURCE_UNITS]{};
    uint32_t pool_count = FT_CARD_GAME_MAX_RESOURCE_POOLS;
    uint32_t unit_count = FT_CARD_GAME_MAX_RESOURCE_UNITS;
    uint32_t iterations = 20000U;
    if (small)
    {
        pool_count = 1U;
        unit_count = 4U;
        iterations = 200000U;
    }
    uint32_t index = 0U;
    while (index < pool_count)
    {
        pools[index] = {index + 1U, pool_count - index, 7U, 4U, 4U, 0U, 0U};
        ++index;
    }
    index = 0U;
    while (index < unit_count)
    {
        units[index].unit_id = index + 1U;
        units[index].owner_id = index % pool_count + 1U;
        units[index].resource_type_id = 7U;
        units[index].amount = 1U;
        ++index;
    }
    card_game_resource_ledger ledger;
    const card_game_resource_snapshot snapshot{pool_count, unit_count,
        unit_count + 1U, pools, units};
    if (ledger.initialize() != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (ledger.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    card_game_resource_pool last{};
    if (ledger.get_pool(1U, 7U, &last) != FT_ERR_SUCCESS)
        return (1);
    std::printf("resource snapshot checksum %" PRIu32 " %.3f ns/op\n",
        last.current_amount + last.pool_id + ledger.unit_count(), elapsed / iterations);
    return (0);
}

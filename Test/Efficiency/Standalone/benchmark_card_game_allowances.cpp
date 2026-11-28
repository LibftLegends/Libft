#include "../../../Modules/CardGame/card_game_resources.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t COUNT = FT_CARD_GAME_MAX_ALLOWANCES;
    const ft_bool restore = argument_count == 2 && ft_strcmp(arguments[1], "--restore") == 0;
    card_game_action_allowance allowances[COUNT]{};
    card_game_allowance_ledger ledger;
    uint32_t index = 0U;
    while (index < COUNT)
    {
        allowances[index].allowance_id = index + 1U;
        allowances[index].owner_id = 1U;
        allowances[index].action_id = 1U;
        allowances[index].maximum_uses = UINT32_MAX;
        allowances[index].remaining_uses = UINT32_MAX;
        ++index;
    }
    const card_game_allowance_snapshot snapshot{COUNT, COUNT + 1U, allowances};
    if (ledger.initialize() != FT_ERR_SUCCESS || ledger.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iterations = 1000000U;
    if (restore)
        iterations = 20000U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (restore)
        {
            if (ledger.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
                return (1);
        }
        else if (ledger.consume(index % COUNT + 1U, 1U, 1U, 0U, 0U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    card_game_action_allowance last{};
    if (ledger.get(COUNT, &last) != FT_ERR_SUCCESS)
        return (1);
    std::printf("allowances checksum %" PRIu32 " %.3f ns/op\n",
        last.remaining_uses, elapsed / iterations);
    return (0);
}

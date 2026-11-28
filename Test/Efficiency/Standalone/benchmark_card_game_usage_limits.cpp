#include "../../../Modules/CardGame/card_game_usage_limits.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t COUNT = FT_CARD_GAME_MAX_USAGE_LIMITS;
    const ft_bool restore = argument_count == 2 && ft_strcmp(arguments[1], "--restore") == 0;
    static card_game_usage_limit entries[COUNT];
    card_game_usage_limit_ledger ledger;
    uint32_t index = 0U;
    while (index < COUNT)
    {
        entries[index] = {index + 1U, index + 1U, 1U, CARD_GAME_USAGE_MATCH,
            0U, UINT32_MAX, 0U, CARD_GAME_USAGE_ON_ATTEMPT, 0U};
        ++index;
    }
    const card_game_usage_limit_snapshot snapshot{COUNT, COUNT + 1U, COUNT, entries};
    if (ledger.initialize() != FT_ERR_SUCCESS || ledger.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iterations = 100000U;
    if (restore)
        iterations = 64U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (restore)
        {
            if (ledger.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
                return (1);
        }
        else if (ledger.consume(index % COUNT + 1U, 1U, 0U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    card_game_usage_limit last{};
    if (ledger.get(COUNT, &last) != FT_ERR_SUCCESS)
        return (1);
    std::printf("usage limits checksum %" PRIu32 " %.3f us/op\n",
        ledger.size() + last.used_uses, elapsed / iterations);
    return (0);
}

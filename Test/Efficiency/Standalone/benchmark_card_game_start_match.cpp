#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int argc, char **argv)
{
    const ft_bool no_overrides = argc == 2
        && ft_strcmp(argv[1], "--no-overrides") == 0;
    if (argc != 1 && no_overrides == FT_FALSE)
        return (1);
    constexpr uint32_t ITERATIONS = 12000U;
    const card_game_rules rules{4U, 3U, 20U, 5U, 512U, 10U};
    const card_game_match_start_config config{
        0U, 20U, 5U, 0U, FT_TRUE, FT_FALSE};
    card_game_engine engine;
    if (engine.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (no_overrides == FT_FALSE && index < FT_CARD_GAME_MAX_START_OVERRIDES)
    {
        card_game_start_override rule{
            index + 1U, index % FT_CARD_GAME_MAX_PLAYERS,
            CARD_GAME_START_HEALTH, CARD_GAME_START_OVERRIDE_ADD, 1U, 0U};
        if ((index & 1U) != 0U)
        {
            rule.player_id = CARD_GAME_START_ALL_PLAYERS;
            rule.field = CARD_GAME_START_MANA;
        }
        if (engine.register_start_override(rule) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (engine.start_match(FT_CARD_GAME_MAX_PLAYERS, config) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("match setup checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2)
        return (1);
    const ft_bool expiry = ft_strcmp(arguments[1], "--expiry") == 0;
    if (!expiry && ft_strcmp(arguments[1], "--combat") != 0)
        return (1);
    constexpr uint32_t ITERATIONS = 128U;
    card_game_engine engine;
    card_game_snapshot state;
    const card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        8, 6, CARD_GAME_NO_EFFECT};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(card) != FT_ERR_SUCCESS
        || engine.start_match(2U) != FT_ERR_SUCCESS
        || engine.play_card(0U, 1U, 0U, ft_nullptr) != FT_ERR_SUCCESS
        || engine.end_turn() != FT_ERR_SUCCESS
        || engine.play_card(1U, 1U, 0U, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < FT_CARD_GAME_MAX_MODIFIERS)
    {
        uint32_t player = 1U;
        if ((index & 3U) == 0U)
            player = 0U;
        card_game_modifier_duration duration = CARD_GAME_MODIFIER_PERMANENT;
        if (player == 1U)
            duration = CARD_GAME_MODIFIER_UNTIL_END_TURN;
        uint32_t id;
        if (engine.add_card_modifier(player, 0U, 0, 0, duration, index, &id)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (engine.get_snapshot(&state) != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    double elapsed = 0.0;
    while (index < ITERATIONS)
    {
        if (engine.apply_snapshot(state) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        int32_t result;
        if (expiry)
            result = engine.end_turn();
        else
            result = engine.resolve_combat(0U, 0U, 1U, 0U, CARD_GAME_COMBAT_ORDERED);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("combat checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

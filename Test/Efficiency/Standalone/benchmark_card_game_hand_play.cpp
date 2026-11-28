#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 240U;
    constexpr uint32_t HAND_SIZE = 96U;
    card_game_engine engine;
    const card_game_rules rules{4U, HAND_SIZE, 20U, 2U, 2U, 10U};
    const card_game_card_definition definition{
        1U, CARD_GAME_CREATURE, 1U, 2, 3, CARD_GAME_NO_EFFECT};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(definition) != FT_ERR_SUCCESS)
        return (1);
    card_game_command command{};
    command.command_sequence = 1U;
    command.type = CARD_GAME_INTENT_PLAY_CARD;
    command.card_id = definition.card_id;
    double elapsed = 0.0;
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        if (engine.start_match(1U) != FT_ERR_SUCCESS
            || engine.emit_event(1U, 0U, 0U) != FT_ERR_SUCCESS)
            return (1);
        card_game_deck_card card;
        uint32_t hand_index = 0U;
        while (hand_index < HAND_SIZE)
        {
            if (engine.deck_push_top(0U, definition.card_id) != FT_ERR_SUCCESS
                || engine.draw_to_hand(0U, &card) != FT_ERR_SUCCESS)
                return (1);
            ++hand_index;
        }
        command.source_instance_id = card.instance_id;
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = engine.submit_command(command, ft_nullptr);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("hand play checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 4000U;
    constexpr uint32_t HAND_SIZE = FT_CARD_GAME_MAX_CARDS;
    constexpr uint32_t REPLACEMENTS = HAND_SIZE / 2U;
    card_game_engine engine;
    const card_game_rules rules{4U, HAND_SIZE, 20U, 2U, 2U, 10U};
    const card_game_card_definition card{
        1U, CARD_GAME_CREATURE, 0U, 1, 1, CARD_GAME_NO_EFFECT};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(card) != FT_ERR_SUCCESS
        || engine.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < HAND_SIZE + REPLACEMENTS)
    {
        card_game_deck_card drawn;
        if (engine.deck_push_bottom(0U, card.card_id) != FT_ERR_SUCCESS)
            return (1);
        if (index < HAND_SIZE && engine.draw_to_hand(0U, &drawn) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint32_t instance_ids[REPLACEMENTS];
    uint64_t random_state = 0x123456789abcdefULL;
    double elapsed = 0.0;
    index = 0U;
    while (index < ITERATIONS)
    {
        uint32_t request_index = 0U;
        while (request_index < REPLACEMENTS)
        {
            card_game_deck_card drawn;
            if (engine.hand_inspect(0U, HAND_SIZE - 1U - 2U * request_index,
                    &drawn) != FT_ERR_SUCCESS)
                return (1);
            instance_ids[request_index++] = drawn.instance_id;
        }
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = engine.mulligan_hand(0U, instance_ids,
            REPLACEMENTS, &random_state);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("mulligan checksum %" PRIu64 " %.6f us/operation\n",
        checksum ^ random_state, elapsed / ITERATIONS);
    return (0);
}

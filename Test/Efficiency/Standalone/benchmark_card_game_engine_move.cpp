#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 24000U;
    card_game_engine source;
    card_game_engine destination;
    const card_game_rules rules{4U, 8U, 20U, 2U, 2U, 10U};
    const card_game_card_definition definition{
        1U, CARD_GAME_CREATURE, 0U, 2, 3, CARD_GAME_NO_EFFECT};
    const card_game_zone_definition zone{
        21U, 8U, 1U << CARD_GAME_CREATURE, FT_TRUE};
    card_game_deck_card drawn;
    uint32_t instance_id;
    if (source.initialize(rules) != FT_ERR_SUCCESS
        || source.register_card(definition) != FT_ERR_SUCCESS
        || source.register_zone(zone) != FT_ERR_SUCCESS
        || source.start_match(2U) != FT_ERR_SUCCESS
        || source.play_card(0U, definition.card_id, 0U, ft_nullptr) != FT_ERR_SUCCESS
        || source.deck_push_top(1U, definition.card_id) != FT_ERR_SUCCESS
        || source.draw_to_hand(1U, &drawn) != FT_ERR_SUCCESS
        || source.zone_push_top(1U, zone.zone_id, definition.card_id,
            &instance_id) != FT_ERR_SUCCESS)
        return (1);
    uint64_t before;
    if (source.get_state_hash(&before) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination.move(source) != FT_ERR_SUCCESS
            || source.move(destination) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum;
    if (source.get_state_hash(&checksum) != FT_ERR_SUCCESS || checksum != before)
        return (1);
    std::printf("engine move checksum %" PRIu64 " %.6f ns/operation\n",
        checksum, elapsed / (ITERATIONS * 2U));
    return (0);
}

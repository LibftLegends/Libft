#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 160U;
    constexpr uint32_t DRAW_COUNT = 24U;
    card_game_engine engine;
    const card_game_rules rules{4U, 32U, 20U, 5U, 10U, 10U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};
    const card_game_zone_definition discard{41U, 8U,
        1U << CARD_GAME_CREATURE, FT_TRUE};
    const card_game_zone_definition burned{42U, 8U,
        1U << CARD_GAME_CREATURE, FT_TRUE};
    card_game_host_intent_definition empty_event{};
    empty_event.type_id = 601U;
    empty_event.version = 1U;
    empty_event.field_count = 2U;
    empty_event.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    empty_event.field_types[1] = CARD_GAME_HOST_INTENT_U32;
    card_game_host_intent_definition overflow_event = empty_event;
    overflow_event.type_id = 602U;
    const card_game_draw_policy policy{discard.zone_id, burned.zone_id, FT_FALSE,
        empty_event.type_id, empty_event.version, overflow_event.type_id,
        overflow_event.version};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(card) != FT_ERR_SUCCESS
        || engine.register_zone(discard) != FT_ERR_SUCCESS
        || engine.register_zone(burned) != FT_ERR_SUCCESS
        || engine.register_host_intent_type(empty_event) != FT_ERR_SUCCESS
        || engine.register_host_intent_type(overflow_event) != FT_ERR_SUCCESS
        || engine.configure_draw_policy(policy) != FT_ERR_SUCCESS
        || engine.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < 64U)
    {
        if (engine.deck_push_top(0U, card.card_id) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    card_game_command command{};
    command.command_sequence = 1U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = DRAW_COUNT;
    double elapsed = 0.0;
    index = 0U;
    while (index < ITERATIONS)
    {
        if (engine.start_match(1U) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = engine.submit_command(command, ft_nullptr);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS)
            return (1);
        uint32_t draw_index = 0U;
        while (draw_index < DRAW_COUNT)
        {
            if (engine.deck_push_top(0U, card.card_id) != FT_ERR_SUCCESS)
                return (1);
            ++draw_index;
        }
        ++index;
    }
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("draw command checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

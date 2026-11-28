#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 1000000U;
    const ft_bool insert = argument_count == 2 && ft_strcmp(arguments[1], "--insert") == 0;
    card_game_engine engine;
    const card_game_rules rules{4U, 8U, 20U, 5U, 10U, 20U};
    card_game_card_definition definition{};
    definition.card_id = 1U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 1;
    definition.health = 2;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(definition) != FT_ERR_SUCCESS)
        return (1);
    uint32_t deck_size = FT_CARD_GAME_MAX_CARDS;
    uint32_t iterations = ITERATIONS;
    if (insert)
    {
        uint32_t zone_id = 1U;
        while (zone_id <= FT_CARD_GAME_MAX_ZONES)
        {
            if (engine.register_zone({zone_id, FT_CARD_GAME_MAX_CARDS,
                    1U << CARD_GAME_CREATURE, FT_TRUE}) != FT_ERR_SUCCESS)
                return (1);
            ++zone_id;
        }
        --deck_size;
        iterations /= 10U;
    }
    if (engine.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 1U;
    while (index <= deck_size)
    {
        if (engine.deck_push_bottom_instance(0U, index, definition.card_id) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    index = 0U;
    uint32_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    if (insert)
    {
        while (index < iterations)
        {
            uint32_t card_id = 0U;
            if (engine.deck_push_bottom(0U, definition.card_id) != FT_ERR_SUCCESS
                || engine.deck_draw_bottom(0U, &card_id) != FT_ERR_SUCCESS
                || card_id != definition.card_id)
                return (1);
            checksum += card_id;
            ++index;
        }
    }
    else
    {
        while (index < iterations)
        {
            card_game_deck_card card{};
            const uint32_t instance = (index % deck_size) + 1U;
            if (engine.deck_get_instance(0U, instance, &card) != FT_ERR_SUCCESS)
                return (1);
            checksum += card.instance_id;
            ++index;
        }
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("deck %" PRIu32 " cards, checksum %" PRIu32 " %.3f ns/op\n",
        deck_size, checksum, elapsed / iterations);
    return (0);
}

#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    uint32_t count = 8192U;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--small") == 0)
        count = 16U;
    else if (argument_count == 2 && ft_strcmp(arguments[1], "--full") == 0)
        count = FT_CARD_GAME_MAX_CARD_DEFINITIONS;
    else if (argument_count != 1)
        return (1);
    constexpr uint32_t ID_MULTIPLIER = 0x9E3779B9U;
    constexpr uint32_t LOOKUPS = 32768U;
    card_game_engine engine;
    const card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    card_game_card_definition definition{1U, CARD_GAME_CREATURE, 1U,
        3, 7, CARD_GAME_NO_EFFECT};
    if (engine.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto registration_start = std::chrono::steady_clock::now();
    while (index < count)
    {
        definition.card_id = (index + 1U) * ID_MULTIPLIER;
        if (engine.register_card(definition) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double registration_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - registration_start).count();
    if (count == FT_CARD_GAME_MAX_CARD_DEFINITIONS)
    {
        if (engine.register_card(definition) != FT_ERR_ALREADY_EXISTS)
            return (1);
        definition.card_id = (count + 1U) * ID_MULTIPLIER;
        if (engine.register_card(definition) != FT_ERR_FULL)
            return (1);
    }

    uint64_t checksum;
    if (engine.get_rules_hash(&checksum) != FT_ERR_SUCCESS
        || engine.deck_push_bottom_instance(0U, 1U, ID_MULTIPLIER) != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto lookup_start = std::chrono::steady_clock::now();
    while (index < LOOKUPS)
    {
        const uint32_t entry = (index * 65521U) & (count * 2U - 1U);
        const uint32_t card_id = (entry + 1U) * ID_MULTIPLIER;
        const int32_t status = engine.deck_push_bottom_instance(0U, 1U, card_id);
        if ((entry < count && status != FT_ERR_ALREADY_EXISTS)
            || (entry >= count && status != FT_ERR_NOT_FOUND))
            return (1);
        checksum = checksum * 31U + static_cast<uint32_t>(status);
        ++index;
    }
    const double lookup_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - lookup_start).count();
    std::printf("card catalog checksum %" PRIu64 " %.6f us/register-all %.6f ns/lookup\n",
        checksum, registration_elapsed, lookup_elapsed / LOOKUPS);
    return (0);
}

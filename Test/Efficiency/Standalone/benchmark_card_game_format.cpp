#include "../../../Modules/CardGame/card_game_format.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t RULE_COUNT = 8192U;
    const ft_bool lookup = argument_count == 2 && ft_strcmp(arguments[1], "--lookup") == 0;
    const ft_bool validate = argument_count == 2 && ft_strcmp(arguments[1], "--deck") == 0;
    const ft_bool restrictions = argument_count == 2
        && ft_strcmp(arguments[1], "--restrictions") == 0;
    const ft_bool dense = argument_count == 2 && ft_strcmp(arguments[1], "--dense") == 0;
    const ft_bool sparse = argument_count == 2 && ft_strcmp(arguments[1], "--sparse") == 0;
    const ft_bool invalid = argument_count == 2 && ft_strcmp(arguments[1], "--invalid") == 0;
    static card_game_format_config config{};
    config.format_id = 1U;
    config.revision = 1U;
    config.profile_id = 1U;
    config.corpus_version = 1U;
    config.global_copy_limit = 4U;
    config.maximum_main_cards = FT_CARD_GAME_DECK_CODE_MAX_ENTRIES;
    config.maximum_extra_cards = FT_CARD_GAME_DECK_CODE_MAX_ENTRIES;
    config.maximum_side_cards = FT_CARD_GAME_DECK_CODE_MAX_ENTRIES;
    config.legal_card_count = RULE_COUNT;
    uint32_t index = 0U;
    while (index < RULE_COUNT)
    {
        uint32_t card_id = RULE_COUNT - index;
        if (dense || sparse || invalid)
            card_id = (index * 127U) % RULE_COUNT + 1U;
        if (sparse)
            card_id *= 2654435761U;
        config.legal_cards[index] = {card_id, index % 4U + 1U, 0U,
            CARD_GAME_FORMAT_CARD_LEGAL};
        ++index;
    }
    if (invalid)
        config.legal_cards[RULE_COUNT - 1U].card_id = config.legal_cards[0].card_id;
    if (restrictions)
    {
        config.ban_count = FT_CARD_GAME_FORMAT_MAX_BAN_ENTRIES;
        config.exception_count = FT_CARD_GAME_FORMAT_MAX_EXCEPTIONS;
        index = 0U;
        while (index < config.ban_count)
        {
            config.bans[index] = {RULE_COUNT - index / 2U, 1U, (index % 2U) + 1U, 1U};
            ++index;
        }
        index = 0U;
        while (index < config.exception_count)
        {
            config.exceptions[index] = {
                index + 1U, RULE_COUNT - index / 2U, (index % 2U) + 1U, 1U, 1U
            };
            ++index;
        }
    }
    card_game_format format;
    if ((lookup || validate) && format.initialize(config) != FT_ERR_SUCCESS)
        return (1);
    card_game_deck deck{};
    if (validate)
    {
        deck.format_id = config.format_id;
        deck.profile_id = config.profile_id;
        deck.corpus_version = config.corpus_version;
        deck.zone_count = FT_CARD_GAME_DECK_CODE_MAX_ZONES;
        uint32_t zone_index = 0U;
        while (zone_index < deck.zone_count)
        {
            card_game_deck_zone &zone = deck.zones[zone_index];
            zone.zone_id = zone_index + 1U;
            zone.entry_count = FT_CARD_GAME_DECK_CODE_MAX_ENTRIES;
            uint32_t entry_index = 0U;
            while (entry_index < zone.entry_count)
            {
                const uint32_t card_id = zone_index * zone.entry_count + zone.entry_count - entry_index;
                zone.entries[entry_index] = {card_id, 0U, 1U};
                ++entry_index;
            }
            ++zone_index;
        }
    }
    uint32_t iterations = 12U;
    if (lookup)
        iterations = 250000U;
    if (validate)
        iterations = 512U;
    uint32_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (validate)
        {
            card_game_format_diagnostic diagnostic{};
            if (format.validate_deck(deck, &diagnostic) != FT_ERR_SUCCESS)
                return (1);
            checksum += deck.zone_count;
        }
        else if (lookup)
        {
            uint32_t limit = 0U;
            if (format.is_card_legal(index % RULE_COUNT + 1U, &limit) != FT_ERR_SUCCESS)
                return (1);
            checksum += limit;
        }
        else if (invalid)
        {
            if (format.initialize(config) != FT_ERR_INVALID_ARGUMENT)
                return (1);
            ++checksum;
        }
        else
        {
            card_game_format_hash hash{};
            if (format.initialize(config) != FT_ERR_SUCCESS
                || format.get_hash(&hash) != FT_ERR_SUCCESS
                || format.destroy() != FT_ERR_SUCCESS)
                return (1);
            uint32_t byte_index = 0U;
            while (byte_index < sizeof(hash.bytes))
            {
                checksum = checksum * 31U + hash.bytes[byte_index];
                ++byte_index;
            }
        }
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("format %" PRIu32 " rules checksum %" PRIu32 " %.3f us/op\n",
        RULE_COUNT, checksum, elapsed / iterations);
    return (0);
}

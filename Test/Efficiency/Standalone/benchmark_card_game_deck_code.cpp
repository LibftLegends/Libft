#include "../../../Modules/CardGame/card_game_deck_code.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 10000U;
    const ft_bool decode = argument_count == 2 && ft_strcmp(arguments[1], "--decode") == 0;
    card_game_deck deck{};
    deck.profile_id = 1U;
    deck.format_id = 2U;
    deck.corpus_version = 3U;
    deck.zone_count = 4U;
    uint32_t zone_index = 0U;
    while (zone_index < deck.zone_count)
    {
        card_game_deck_zone &zone = deck.zones[zone_index];
        zone.zone_id = deck.zone_count - zone_index;
        zone.entry_count = 125U;
        uint32_t entry_index = 0U;
        while (entry_index < zone.entry_count)
        {
            zone.entries[entry_index] = {500U - zone_index * zone.entry_count - entry_index, 0U, 1U};
            ++entry_index;
        }
        ++zone_index;
    }
    ft_string encoded;
    if (encoded.initialize() != FT_ERR_SUCCESS
        || card_game_deck_encode(deck, &encoded) != FT_ERR_SUCCESS)
        return (1);
    card_game_deck decoded{};
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (decode)
        {
            if (card_game_deck_decode(encoded, &decoded) != FT_ERR_SUCCESS)
                return (1);
        }
        else if (card_game_deck_encode(deck, &encoded) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint32_t checksum = 0U;
    const char *bytes = encoded.c_str();
    while (*bytes != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(*bytes);
        ++bytes;
    }
    if (decode)
        checksum ^= decoded.zone_count + decoded.zones[0].entries[0].definition_id;
    std::printf("deck codec checksum %" PRIu32 " %.3f us/op\n", checksum, elapsed / ITERATIONS);
    return (0);
}

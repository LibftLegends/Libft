#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_deck_code.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void card_game_deck_code_fill(card_game_deck *deck) noexcept
{
    ft_bzero(deck, sizeof(*deck));
    deck->profile_id = 2U;
    deck->format_id = 4U;
    deck->corpus_version = 9U;
    deck->zone_count = 2U;
    deck->zones[0].zone_id = CARD_GAME_DECK_ZONE_MAIN;
    deck->zones[0].entry_count = 2U;
    deck->zones[0].entries[0].definition_id = 500U;
    deck->zones[0].entries[0].quantity = 3U;
    deck->zones[0].entries[1].definition_id = 20U;
    deck->zones[0].entries[1].quantity = 1U;
    deck->zones[1].zone_id = CARD_GAME_DECK_ZONE_SIDE;
    deck->zones[1].entry_count = 1U;
    deck->zones[1].entries[0].definition_id = 100U;
    deck->zones[1].entries[0].quantity = 2U;
}

FT_TEST(test_card_game_deck_code_round_trips_canonically)
{
    card_game_deck first;
    card_game_deck second;
    ft_string encoded;
    ft_string reordered;

    card_game_deck_code_fill(&first);
    first.zones[0].entries[0].printing_id = 55U;
    first.zones[0].entries[1].printing_id = 11U;
    first.flags = CARD_GAME_DECK_FLAG_PRINTINGS;
    first.zones[0].entries[0].definition_id = 500U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reordered.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_encode(first, &encoded));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_decode(encoded, &second));
    FT_ASSERT_EQ(first.profile_id, second.profile_id);
    FT_ASSERT_EQ(first.zone_count, second.zone_count);
    FT_ASSERT_EQ(20U, second.zones[0].entries[0].definition_id);
    FT_ASSERT_EQ(11U, second.zones[0].entries[0].printing_id);
    FT_ASSERT_EQ(500U, second.zones[0].entries[1].definition_id);
    FT_ASSERT_EQ(55U, second.zones[0].entries[1].printing_id);
    first.zones[0].entries[0].definition_id = 20U;
    first.zones[0].entries[0].printing_id = 11U;
    first.zones[0].entries[1].definition_id = 500U;
    first.zones[0].entries[1].printing_id = 55U;
    first.zones[0].entries[0].quantity = 1U;
    first.zones[0].entries[1].quantity = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_encode(first, &reordered));
    FT_ASSERT(encoded == reordered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reordered.destroy());
    return (1);
}

FT_TEST(test_card_game_deck_code_rejects_checksum_and_truncation)
{
    card_game_deck deck;
    card_game_deck output;
    ft_string encoded;
    ft_string truncated;

    card_game_deck_code_fill(&deck);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_encode(deck, &encoded));
    FT_ASSERT(encoded.size() > 2U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.assign(encoded.c_str(),
        encoded.size()));
    truncated.data()[truncated.size() - 1U] = 'A';
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        card_game_deck_decode(truncated, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.assign(encoded.c_str(),
        encoded.size() - 2U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        card_game_deck_decode(truncated, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.destroy());
    return (1);
}

FT_TEST(test_card_game_deck_code_hash_is_canonical_and_changes)
{
    card_game_deck first;
    card_game_deck second;
    card_game_deck_hash_value first_hash;
    card_game_deck_hash_value second_hash;

    card_game_deck_code_fill(&first);
    second = first;
    second.zones[0].entries[0] = first.zones[0].entries[1];
    second.zones[0].entries[1] = first.zones[0].entries[0];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_hash(first, &first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_hash(second, &second_hash));
    FT_ASSERT_EQ(0, ft_memcmp(first_hash.bytes, second_hash.bytes,
        sizeof(first_hash.bytes)));
    second.zones[0].entries[0].quantity += 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_hash(second, &second_hash));
    FT_ASSERT(ft_memcmp(first_hash.bytes, second_hash.bytes,
        sizeof(first_hash.bytes)) != 0);
    return (1);
}

FT_TEST(test_card_game_deck_code_supports_five_hundred_cards)
{
    card_game_deck deck;
    card_game_deck decoded;
    ft_string encoded;

    card_game_deck_code_fill(&deck);
    deck.zone_count = 1U;
    deck.zones[0].entry_count = 1U;
    deck.zones[0].entries[0].definition_id = 900U;
    deck.zones[0].entries[0].quantity = 500U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_encode(deck, &encoded));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_deck_decode(encoded, &decoded));
    FT_ASSERT_EQ(500U, decoded.zones[0].entries[0].quantity);
    deck.zones[0].entries[0].quantity = 501U;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        card_game_deck_encode(deck, &encoded));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.destroy());
    return (1);
}

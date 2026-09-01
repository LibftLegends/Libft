#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_format.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void card_game_format_fill(card_game_format_config *config) noexcept
{
    ft_bzero(config, sizeof(*config));
    config->format_id = 7U;
    config->revision = 2U;
    config->profile_id = 3U;
    config->corpus_version = 11U;
    config->global_copy_limit = 4U;
    config->minimum_main_cards = 2U;
    config->maximum_main_cards = 4U;
    config->legal_card_count = 2U;
    config->legal_cards[0].card_id = 10U;
    config->legal_cards[0].copy_limit = 4U;
    config->legal_cards[0].policy = CARD_GAME_FORMAT_CARD_LEGAL;
    config->legal_cards[1].card_id = 20U;
    config->legal_cards[1].copy_limit = 1U;
    config->legal_cards[1].policy = CARD_GAME_FORMAT_CARD_RESTRICTED;
    config->ban_count = 1U;
    config->bans[0].card_id = 20U;
    config->bans[0].copy_limit = 1U;
    config->bans[0].source_revision = 4U;
    config->exception_count = 1U;
    config->exceptions[0].exception_id = 90U;
    config->exceptions[0].card_id = 20U;
    config->exceptions[0].rule_id = 5U;
    config->exceptions[0].replacement_program_id = 12U;
    config->exceptions[0].priority = 3U;
    return ;
}

FT_TEST(test_card_game_format_resolves_legality_hash_and_exception)
{
    card_game_format format;
    static card_game_format_config config;
    card_game_format_hash first_hash;
    card_game_format_hash second_hash;
    card_game_format_exception exception;
    uint32_t copy_limit;

    card_game_format_fill(&config);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.is_card_legal(10U, &copy_limit));
    FT_ASSERT_EQ(4U, copy_limit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.is_card_legal(20U, &copy_limit));
    FT_ASSERT_EQ(1U, copy_limit);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, format.is_card_legal(99U, &copy_limit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_exception(20U, 5U, &exception));
    FT_ASSERT_EQ(12U, exception.replacement_program_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_hash(&second_hash));
    FT_ASSERT(std::memcmp(first_hash.bytes, second_hash.bytes, 32U) == 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    return (1);
}

FT_TEST(test_card_game_format_validates_deck_transactionally)
{
    card_game_format format;
    static card_game_format_config config;
    card_game_format_diagnostic diagnostic;
    card_game_deck deck;

    card_game_format_fill(&config);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    ft_bzero(&deck, sizeof(deck));
    deck.profile_id = 3U;
    deck.format_id = 7U;
    deck.corpus_version = 11U;
    deck.zone_count = 1U;
    deck.zones[0].zone_id = CARD_GAME_DECK_ZONE_MAIN;
    deck.zones[0].entry_count = 1U;
    deck.zones[0].entries[0].definition_id = 10U;
    deck.zones[0].entries[0].quantity = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.validate_deck(deck, &diagnostic));
    deck.zones[0].entries[0].quantity = 5U;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(10U, diagnostic.card_id);
    FT_ASSERT_EQ(5U, diagnostic.observed_count);
    FT_ASSERT_EQ(4U, diagnostic.permitted_count);
    deck.format_id = 8U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    return (1);
}

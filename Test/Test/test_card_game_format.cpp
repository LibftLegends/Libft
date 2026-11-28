#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_format.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"

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
    config->bans[0].zone_mask = 1U;
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
    const uint8_t expected_hash[] = {
        0x80U, 0x76U, 0xf8U, 0xd7U, 0xc2U, 0x0bU, 0xf0U, 0x3bU,
        0xb8U, 0x59U, 0x05U, 0x7aU, 0xc1U, 0xa9U, 0x85U, 0x15U,
        0x87U, 0x42U, 0xadU, 0xdaU, 0x1aU, 0x25U, 0x31U, 0xc0U,
        0x0fU, 0x5eU, 0xa2U, 0x39U, 0xfeU, 0x55U, 0x46U, 0x39U
    };
    card_game_format_exception exception;
    uint32_t copy_limit;

    card_game_format_fill(&config);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_hash(&first_hash));
    FT_ASSERT_EQ(0, ft_memcmp(expected_hash, first_hash.bytes, sizeof(expected_hash)));
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
    FT_ASSERT(ft_memcmp(first_hash.bytes, second_hash.bytes, 32U) == 0);
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

FT_TEST(test_card_game_format_enforces_copy_limit_across_deck_zones)
{
    card_game_format format;
    static card_game_format_config config;
    card_game_format_diagnostic diagnostic;
    card_game_deck deck;

    card_game_format_fill(&config);
    config.maximum_side_cards = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    ft_bzero(&deck, sizeof(deck));
    deck.profile_id = 3U;
    deck.format_id = 7U;
    deck.corpus_version = 11U;
    deck.zone_count = 2U;
    deck.zones[0].zone_id = CARD_GAME_DECK_ZONE_MAIN;
    deck.zones[0].entry_count = 1U;
    deck.zones[0].entries[0].definition_id = 10U;
    deck.zones[0].entries[0].quantity = 2U;
    deck.zones[1].zone_id = CARD_GAME_DECK_ZONE_SIDE;
    deck.zones[1].entry_count = 1U;
    deck.zones[1].entries[0].definition_id = 10U;
    deck.zones[1].entries[0].quantity = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.validate_deck(deck, &diagnostic));
    deck.zones[1].entries[0].quantity = 3U;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(10U, diagnostic.card_id);
    FT_ASSERT_EQ(5U, diagnostic.observed_count);
    FT_ASSERT_EQ(4U, diagnostic.permitted_count);
    deck.zones[1].entries[0].quantity = UINT32_MAX;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(CARD_GAME_DECK_ZONE_MAIN, diagnostic.zone_id);
    FT_ASSERT_EQ(UINT32_MAX, diagnostic.observed_count);
    deck.zones[1].entries[0].quantity = 0U;
    deck.zones[0].entries[0].definition_id = 99U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(CARD_GAME_DECK_ZONE_SIDE, diagnostic.zone_id);
    FT_ASSERT_EQ(10U, diagnostic.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    return (1);
}

FT_TEST(test_card_game_format_rejects_malformed_deck_entries)
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
    deck.zones[0].entry_count = FT_CARD_GAME_DECK_CODE_MAX_ENTRIES + 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        format.validate_deck(deck, &diagnostic));
    deck.zones[0].entry_count = 2U;
    deck.zones[0].entries[0].definition_id = 10U;
    deck.zones[0].entries[0].quantity = 1U;
    deck.zones[0].entries[1].definition_id = 10U;
    deck.zones[0].entries[1].quantity = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        format.validate_deck(deck, &diagnostic));
    deck.zones[0].entry_count = 1U;
    deck.zones[0].entries[0].quantity = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        format.validate_deck(deck, &diagnostic));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    return (1);
}

FT_TEST(test_card_game_format_rejects_conflicting_config_rules)
{
    card_game_format format;
    static card_game_format_config config;

    card_game_format_fill(&config);
    config.ban_count = 2U;
    config.bans[1] = config.bans[0];
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    card_game_format_fill(&config);
    config.exceptions[1] = config.exceptions[0];
    config.exception_count = 2U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    card_game_format_fill(&config);
    config.legal_cards[0].policy = static_cast<
        card_game_format_card_policy>(255U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    return (1);
}

FT_TEST(test_card_game_format_sorted_queries_preserve_hash_and_first_ban)
{
    card_game_format format;
    static card_game_format_config config;
    card_game_format_hash baseline_hash{};
    card_game_format_hash reordered_hash{};
    card_game_format_exception exception{};
    uint32_t copy_limit = 0U;

    card_game_format_fill(&config);
    config.ban_count = 2U;
    config.bans[1] = {20U, 0U, 2U, 4U};
    config.exception_count = 2U;
    config.exceptions[1] = {91U, 20U, 2U, 13U, 3U};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_hash(&baseline_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.is_card_legal(20U, &copy_limit));
    FT_ASSERT_EQ(1U, copy_limit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_exception(20U, 2U, &exception));
    FT_ASSERT_EQ(13U, exception.replacement_program_id);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, format.get_exception(20U, 3U, &exception));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());

    const card_game_format_card_rule saved_rule = config.legal_cards[0];
    config.legal_cards[0] = config.legal_cards[1];
    config.legal_cards[1] = saved_rule;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_hash(&reordered_hash));
    FT_ASSERT_EQ(0, ft_memcmp(baseline_hash.bytes, reordered_hash.bytes, sizeof(baseline_hash.bytes)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());

    config.legal_cards[1] = config.legal_cards[0];
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    cma_set_alloc_limit(1);
    const int32_t rejected_duplicate = format.initialize(config);
    config.legal_cards[1] = saved_rule;
    const int32_t rejected_allocation = format.initialize(config);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, rejected_duplicate);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, rejected_allocation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    return (1);
}

FT_TEST(test_card_game_format_preserves_same_card_ban_order_when_sorting)
{
    card_game_format format;
    static card_game_format_config config;
    static card_game_format_config ordered;
    uint32_t copy_limit = 0U;

    card_game_format_fill(&config);
    config.ban_count = 5U;
    config.bans[0] = {20U, 1U, 2U, 4U};
    config.bans[1] = {10U, 3U, 4U, 4U};
    config.bans[2] = {20U, 0U, 1U, 4U};
    config.bans[3] = {10U, 0U, 1U, 4U};
    config.bans[4] = {20U, 2U, 4U, 4U};
    config.exception_count = 2U;
    config.exceptions[1] = {91U, 10U, 1U, 14U, 3U};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.initialize(config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.get_config(&ordered));
    const uint32_t expected_masks[] = {4U, 1U, 2U, 1U, 4U};
    uint32_t index = 0U;
    while (index < ordered.ban_count)
    {
        FT_ASSERT_EQ(expected_masks[index], ordered.bans[index].zone_mask);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.is_card_legal(10U, &copy_limit));
    FT_ASSERT_EQ(3U, copy_limit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.is_card_legal(20U, &copy_limit));
    FT_ASSERT_EQ(1U, copy_limit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, format.destroy());
    config.exceptions[1].exception_id = config.exceptions[0].exception_id;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    config.exceptions[1] = config.exceptions[0];
    config.exceptions[1].exception_id = 91U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, format.initialize(config));
    return (1);
}

FT_TEST(test_card_game_format_detects_nonlocal_duplicates_without_allocation)
{
    card_game_format format;
    static card_game_format_config config;
    static test_cma_failure_controller controller;
    card_game_format_fill(&config);
    config.legal_card_count = 8193U;
    uint32_t index = 0U;
    while (index < config.legal_card_count)
    {
        config.legal_cards[index] = {index * 65537U + 1U, 1U, 0U, CARD_GAME_FORMAT_CARD_LEGAL};
        ++index;
    }
    const uint32_t last_id = config.legal_cards[index - 1U].card_id;
    config.legal_cards[index - 1U].card_id = config.legal_cards[0].card_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(controller));
    const int32_t armed = test_cma_failure_controller_fail_next(controller, TEST_CMA_FAILURE_ALLOCATE);
    const int32_t status = format.initialize(config);
    --config.legal_card_count;
    const uint32_t second_last_id = config.legal_cards[index - 2U].card_id;
    config.legal_cards[index - 2U].card_id = config.legal_cards[0].card_id;
    const int32_t sparse_status = format.initialize(config);
    config.legal_cards[index - 2U].card_id = second_last_id;
    ++config.legal_card_count;
    const ft_size_t invalid_attempts = test_cma_failure_controller_attempt_count(
        controller, TEST_CMA_FAILURE_ALLOCATE);
    config.legal_cards[index - 1U].card_id = last_id;
    const int32_t allocation_status = format.initialize(config);
    const ft_size_t valid_attempts = test_cma_failure_controller_attempt_count(
        controller, TEST_CMA_FAILURE_ALLOCATE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_destroy(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, armed);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, status);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sparse_status);
    FT_ASSERT_EQ(0U, invalid_attempts);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, allocation_status);
    FT_ASSERT_EQ(1U, valid_attempts);
    return (1);
}

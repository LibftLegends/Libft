#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_choices.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_card_game_choices_snapshot_restores_pending_choice)
{
    card_game_choice_ledger ledger;
    card_game_choice_snapshot saved;
    card_game_choice_snapshot copy;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;

    ft_bzero(&saved, sizeof(saved));
    ft_bzero(&copy, sizeof(copy));
    option.option_id = 10U;
    option.value_a = 20U;
    option.value_b = 30U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_TARGET,
        8U, 10U, &choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::clone_snapshot(saved, &copy));
    FT_ASSERT(card_game_choice_ledger::snapshots_equal(saved, copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.choose(choice_id, 1U, 10U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT_EQ(FT_TRUE, choice.resolved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.apply_snapshot(saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT_EQ(FT_FALSE, choice.resolved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::release_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::release_snapshot(&copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_validate_owner_and_options)
{
    card_game_choice_ledger ledger;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_TARGET,
        10U, 0U, &choice_id));
    option.option_id = 4U;
    option.value_a = 77U;
    option.value_b = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, ledger.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, ledger.choose(choice_id, 2U, 4U,
        1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ledger.choose(choice_id, 1U, 5U,
        1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.choose(choice_id, 1U, 4U, 1U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, ledger.choose(choice_id, 1U, 4U,
        1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT(choice.resolved != FT_FALSE);
    FT_ASSERT_EQ(4U, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_accepts_zero_based_player_id)
{
    card_game_choice_ledger ledger;
    uint32_t choice_id;
    card_game_choice choice;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(0U, CARD_GAME_CHOICE_TARGET,
        0U, 0U, &choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT_EQ(0U, choice.player_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_expiry_failure_is_transactional)
{
    card_game_choice_ledger ledger;
    card_game_choice_option valid_option;
    uint32_t first_choice_id;
    uint32_t second_choice_id;
    card_game_choice first_choice;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    valid_option.option_id = 1U;
    valid_option.value_a = 10U;
    valid_option.value_b = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(0U, CARD_GAME_CHOICE_TARGET,
        10U, 1U, &first_choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(first_choice_id,
        valid_option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(0U, CARD_GAME_CHOICE_TARGET,
        10U, 2U, &second_choice_id));
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, ledger.resolve_expired(10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(first_choice_id, &first_choice));
    FT_ASSERT_EQ(FT_FALSE, first_choice.resolved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_resolve_expired_default_transactionally)
{
    card_game_choice_ledger ledger;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;
    uint32_t invalid_choice_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_MODE,
        5U, 8U, &choice_id));
    option.option_id = 8U;
    option.value_a = 0U;
    option.value_b = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.resolve_expired(5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT_EQ(8U, choice.selected_option_id);
    FT_ASSERT(choice.resolved != FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_NUMBER,
        5U, 9U, &invalid_choice_id));
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, ledger.resolve_expired(5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(invalid_choice_id, &choice));
    FT_ASSERT(choice.resolved == FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_session_deadline_resolves_defaults_early)
{
    card_game_choice_ledger ledger;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;
    uint32_t resolved_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_TARGET,
        500U, 9U, &choice_id));
    option.option_id = 9U;
    option.value_a = 42U;
    option.value_b = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(choice_id, option));
    resolved_count = UINT32_MAX;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ledger.resolve_all_pending_to_default(&resolved_count));
    FT_ASSERT_EQ(1U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(choice_id, &choice));
    FT_ASSERT(choice.resolved != FT_FALSE);
    FT_ASSERT_EQ(9U, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ledger.resolve_all_pending_to_default(&resolved_count));
    FT_ASSERT_EQ(0U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_session_deadline_default_failure_is_transactional)
{
    card_game_choice_ledger ledger;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t valid_choice_id;
    uint32_t invalid_choice_id;
    uint32_t resolved_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_TARGET,
        0U, 2U, &valid_choice_id));
    option.option_id = 2U;
    option.value_a = 7U;
    option.value_b = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(valid_choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_MODE,
        0U, 3U, &invalid_choice_id));
    resolved_count = UINT32_MAX;
    FT_ASSERT_EQ(FT_ERR_TIMEOUT,
        ledger.resolve_all_pending_to_default(&resolved_count));
    FT_ASSERT_EQ(0U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(valid_choice_id, &choice));
    FT_ASSERT(choice.resolved == FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(invalid_choice_id, &choice));
    FT_ASSERT(choice.resolved == FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_resolves_expired_choice_defaults)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;
    uint32_t resolved_count;
    uint64_t before_hash;
    uint64_t resolved_hash;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 8U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.open_choice(0U,
        CARD_GAME_CHOICE_MODE, 5U, 7U, &choice_id));
    option.option_id = 7U;
    option.value_a = 12U;
    option.value_b = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_choice_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&before_hash));
    resolved_count = UINT32_MAX;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.resolve_expired_choices(4U, &resolved_count));
    FT_ASSERT_EQ(0U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&resolved_hash));
    FT_ASSERT_EQ(before_hash, resolved_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.resolve_expired_choices(5U, &resolved_count));
    FT_ASSERT_EQ(1U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_choice(choice_id, &choice));
    FT_ASSERT(choice.resolved != FT_FALSE);
    FT_ASSERT_EQ(7U, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&resolved_hash));
    FT_ASSERT(before_hash != resolved_hash);
    before_hash = resolved_hash;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.resolve_expired_choices(6U, &resolved_count));
    FT_ASSERT_EQ(0U, resolved_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&resolved_hash));
    FT_ASSERT_EQ(before_hash, resolved_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_choices_serialization_is_transactional)
{
    card_game_choice_ledger source;
    card_game_choice_ledger destination;
    card_game_choice choice;
    card_game_choice_option option;
    uint8_t serialized[512];
    uint32_t choice_id;
    uint32_t destination_choice_id;
    uint32_t serialized_size;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.open(0U, CARD_GAME_CHOICE_NUMBER,
        25U, 7U, &choice_id));
    option.option_id = 7U;
    option.value_a = 42U;
    option.value_b = 99U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.choose(choice_id, 0U, 7U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize(serialized,
        sizeof(serialized), &serialized_size));
    FT_ASSERT_EQ(432U, serialized_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.open(1U,
        CARD_GAME_CHOICE_BOOLEAN, 0U, 0U, &destination_choice_id));
    serialized[40U] = 0U;
    serialized[41U] = 0U;
    serialized[42U] = 0U;
    serialized[43U] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, destination.deserialize(serialized,
        serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get(destination_choice_id,
        &choice));
    FT_ASSERT_EQ(1U, choice.player_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize(serialized,
        sizeof(serialized), &serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.deserialize(serialized,
        serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get(choice_id, &choice));
    FT_ASSERT_EQ(0U, choice.player_id);
    FT_ASSERT_EQ(7U, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

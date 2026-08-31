#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void card_game_deck_test_rules(card_game_rules &rules) noexcept
{
    rules.max_board_spaces = 4U;
    rules.max_hand_size = 8U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    return ;
}

static int32_t card_game_register_deck_test_cards(
    card_game_engine &engine) noexcept
{
    card_game_card_definition definition;
    uint32_t card_id;

    card_id = 1U;
    while (card_id <= 4U)
    {
        definition.card_id = card_id;
        definition.type = CARD_GAME_CREATURE;
        definition.cost = 1U;
        definition.attack = static_cast<int32_t>(card_id);
        definition.health = 2;
        definition.effect_id = CARD_GAME_NO_EFFECT;
        if (engine.register_card(definition) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        card_id += 1U;
    }
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_card_game_engine_deck_operations_preserve_order)
{
    card_game_engine engine;
    card_game_rules rules;
    uint32_t count;
    uint32_t card_id;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_register_deck_test_cards(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_insert_at(0U, 1U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &count));
    FT_ASSERT_EQ(4U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_draw_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_bottom(0U, &card_id));
    FT_ASSERT_EQ(2U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_remove(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.deck_remove(0U, 99U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.deck_push_top(0U, 99U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_deck_shuffle_is_deterministic)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    uint64_t first_hash;
    uint64_t second_hash;
    uint64_t first_seed;
    uint64_t second_seed;
    uint32_t card_id;
    uint32_t card_index;

    card_game_deck_test_rules(rules);
    first_seed = 1234567U;
    second_seed = 1234567U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(first_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(second_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.start_match(1U));
    card_index = 1U;
    while (card_index <= 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            first_engine.deck_push_bottom(0U, card_index));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            second_engine.deck_push_bottom(0U, card_index));
        card_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.shuffle_deck(0U, &first_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.shuffle_deck(0U, &second_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_state_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_state_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_NEQ(0U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_owned_shuffle_tracks_random_state)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    uint64_t first_state;
    uint64_t second_state;
    uint64_t first_hash;
    uint64_t second_hash;
    uint32_t card_index;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(first_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(second_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.set_random_seed(77U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.set_random_seed(77U));
    card_index = 1U;
    while (card_index <= 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            first_engine.deck_push_bottom(0U, card_index));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            second_engine.deck_push_bottom(0U, card_index));
        card_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.shuffle_deck(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.shuffle_deck(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_random_state(&first_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_engine.get_random_state(&second_state));
    FT_ASSERT_EQ(first_state, second_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_state_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_state_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.set_random_seed(99U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_random_state(&second_state));
    FT_ASSERT_EQ(99U, second_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_deck_delta_preserves_order)
{
    card_game_engine source_engine;
    card_game_engine destination_engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    uint32_t card_id;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(destination_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_bottom(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_bottom(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_top(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(4U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.deck_peek_bottom(
        0U, &card_id));
    FT_ASSERT_EQ(1U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_moves_drawn_cards_through_hand)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_deck_card card;
    uint32_t count;
    uint32_t board_count;

    card_game_deck_test_rules(rules);
    definition.card_id = 9U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 3;
    definition.health = 4;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &card));
    FT_ASSERT_EQ(9U, card.card_id);
    FT_ASSERT_NEQ(0U, card.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &count));
    FT_ASSERT_EQ(1U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 0U, &card));
    FT_ASSERT_EQ(9U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        card.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_rejects_invalid_hand_play_requests)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_deck_card card;
    uint32_t hand_count;

    card_game_deck_test_rules(rules);
    definition.card_id = 31U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 1;
    definition.health = 1;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.play_card_from_hand(1U,
        1U, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.play_card_from_hand(0U,
        0U, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.play_card_from_hand(0U, 1U,
        0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 31U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &card));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.play_card_from_hand(0U,
        card.instance_id + 1U, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(1U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_mulligan_is_deterministic_and_transactional)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_deck_card card;
    uint32_t selected_ids[2];
    uint32_t duplicate_ids[2];
    uint32_t count;
    uint64_t random_state;
    uint64_t state_hash_before;
    uint64_t state_hash_after;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_register_deck_test_cards(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &card));
    selected_ids[0] = card.instance_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &card));
    selected_ids[1] = card.instance_id;
    duplicate_ids[0] = selected_ids[0];
    duplicate_ids[1] = selected_ids[0];
    random_state = 0x123456789abcdefULL;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&state_hash_before));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, engine.mulligan_hand(0U,
        duplicate_ids, 2U, &random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&state_hash_after));
    FT_ASSERT_EQ(state_hash_before, state_hash_after);
    FT_ASSERT_EQ(0x123456789abcdefULL, random_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.mulligan_hand(0U, selected_ids, 1U,
        &random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &count));
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &count));
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_snapshot_and_delta_preserve_hand)
{
    card_game_engine source_engine;
    card_game_engine destination_engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    card_game_deck_card card;
    uint32_t count;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(destination_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_top(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.draw_to_hand(0U, &card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.get_hand_count(0U,
        &count));
    FT_ASSERT_EQ(1U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.hand_inspect(0U, 0U,
        &card));
    FT_ASSERT_EQ(1U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_configures_opening_hand_and_first_player)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_match_start_config start_config;
    card_game_card_definition definition;
    card_game_deck_card card;
    uint32_t player_id;
    uint32_t count;
    uint32_t health;
    uint32_t mana;

    card_game_deck_test_rules(rules);
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 1;
    definition.health = 1;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    player_id = 1U;
    while (player_id <= 2U)
    {
        definition.card_id = 20U + player_id;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
        player_id += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 22U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, 22U));
    start_config.opening_hand_size = 2U;
    start_config.starting_health = 23U;
    start_config.starting_mana = 3U;
    start_config.first_player = 1U;
    start_config.random_first_player = FT_FALSE;
    start_config.deal_opening_hand = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U,
        start_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_turn(&count, &player_id));
    FT_ASSERT_EQ(1U, player_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(23U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_mana(0U, &mana));
    FT_ASSERT_EQ(3U, mana);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &count));
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 0U, &card));
    FT_ASSERT_EQ(22U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_applies_scoped_start_override_precedence)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_match_start_config start_config;
    card_game_start_override override_rule;
    card_game_card_definition definition;
    uint32_t health;
    uint32_t mana;
    uint32_t hand_count;
    uint32_t turn_player;
    uint32_t turn_number;

    card_game_deck_test_rules(rules);
    definition.card_id = 61U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 1;
    definition.health = 1;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    override_rule.source_id = 100U;
    override_rule.player_id = CARD_GAME_START_ALL_PLAYERS;
    override_rule.field = CARD_GAME_START_HEALTH;
    override_rule.operation = CARD_GAME_START_OVERRIDE_SET;
    override_rule.value = 30U;
    override_rule.priority = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    override_rule.source_id = 101U;
    override_rule.priority = 10U;
    override_rule.value = 25U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    override_rule.source_id = 102U;
    override_rule.player_id = 0U;
    override_rule.field = CARD_GAME_START_MANA;
    override_rule.operation = CARD_GAME_START_OVERRIDE_ADD;
    override_rule.value = 2U;
    override_rule.priority = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    override_rule.source_id = 103U;
    override_rule.player_id = 0U;
    override_rule.field = CARD_GAME_START_OPENING_HAND;
    override_rule.operation = CARD_GAME_START_OVERRIDE_SET;
    override_rule.value = 2U;
    override_rule.priority = 5U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    override_rule.source_id = 104U;
    override_rule.player_id = CARD_GAME_START_ALL_PLAYERS;
    override_rule.field = CARD_GAME_START_FIRST_PLAYER;
    override_rule.operation = CARD_GAME_START_OVERRIDE_SET;
    override_rule.value = 1U;
    override_rule.priority = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 61U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 61U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, 61U));
    start_config.opening_hand_size = 1U;
    start_config.starting_health = 20U;
    start_config.starting_mana = 5U;
    start_config.first_player = 0U;
    start_config.random_first_player = FT_FALSE;
    start_config.deal_opening_hand = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U,
        start_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(30U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(30U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_mana(0U, &mana));
    FT_ASSERT_EQ(7U, mana);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(2U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_turn(&turn_number, &turn_player));
    FT_ASSERT_EQ(1U, turn_number);
    FT_ASSERT_EQ(1U, turn_player);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_random_first_player_supports_four_players)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    card_game_match_start_config config;
    uint32_t player_id;
    uint32_t first_turn_number;
    uint32_t second_turn_number;
    uint32_t first_active_player;
    uint32_t second_active_player;
    uint32_t hand_count;
    uint32_t health;
    uint32_t mana;
    uint64_t first_random_state;
    uint64_t second_random_state;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(first_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(second_engine));
    player_id = 0U;
    while (player_id < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.deck_push_top(player_id, 1U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.deck_push_top(player_id, 1U));
        player_id += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.set_random_seed(123456789U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.set_random_seed(123456789U));
    config.opening_hand_size = 1U;
    config.starting_health = 40U;
    config.starting_mana = 2U;
    config.first_player = 0U;
    config.random_first_player = FT_TRUE;
    config.deal_opening_hand = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.start_match(4U, config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.start_match(4U, config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_turn(&first_turn_number,
        &first_active_player));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_turn(&second_turn_number,
        &second_active_player));
    FT_ASSERT_EQ(1U, first_turn_number);
    FT_ASSERT_EQ(first_active_player, second_active_player);
    FT_ASSERT(first_active_player < 4U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_random_state(
        &first_random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_random_state(
        &second_random_state));
    FT_ASSERT_EQ(first_random_state, second_random_state);
    player_id = 0U;
    while (player_id < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_hand_count(player_id,
            &hand_count));
        FT_ASSERT_EQ(1U, hand_count);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_player_health(player_id,
            &health));
        FT_ASSERT_EQ(40U, health);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_player_mana(player_id,
            &mana));
        FT_ASSERT_EQ(2U, mana);
        player_id += 1U;
    }
    player_id = 0U;
    while (player_id < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.end_turn());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.end_turn());
        player_id += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_turn(&first_turn_number,
        &first_active_player));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_turn(&second_turn_number,
        &second_active_player));
    FT_ASSERT_EQ(5U, first_turn_number);
    FT_ASSERT_EQ(first_active_player, second_active_player);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_rejects_conflicting_start_overrides_transactionally)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_match_start_config start_config;
    card_game_start_override override_rule;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    override_rule.source_id = 201U;
    override_rule.player_id = CARD_GAME_START_ALL_PLAYERS;
    override_rule.field = CARD_GAME_START_HEALTH;
    override_rule.operation = CARD_GAME_START_OVERRIDE_SET;
    override_rule.value = 20U;
    override_rule.priority = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    override_rule.source_id = 202U;
    override_rule.value = 30U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(override_rule));
    start_config.opening_hand_size = 0U;
    start_config.starting_health = 20U;
    start_config.starting_mana = 5U;
    start_config.first_player = 0U;
    start_config.random_first_player = FT_FALSE;
    start_config.deal_opening_hand = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, engine.start_match(1U,
        start_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_start_override(
        override_rule));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

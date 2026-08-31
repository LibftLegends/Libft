#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_resolution_stack.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_failure_controller.hpp"

static card_game_rules card_game_simulation_rules() noexcept
{
    card_game_rules rules;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 8U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 30U;
    return (rules);
}

static int32_t card_game_simulation_heal_callback(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;
    uint32_t player_id;
    int32_t append_error;

    (void)engine;
    (void)context;
    player_id = *static_cast<uint32_t *>(user_data);
    if (test_failure_controller_should_fail(
        TEST_FAILURE_CARD_GAME_CALLBACK) == FT_TRUE)
        return (FT_ERR_INTERNAL);
    operation.type = CARD_GAME_OPERATION_HEALTH;
    operation.player_id = player_id;
    operation.amount = 3;
    operation.event_type = 0U;
    operation.source_instance = 0U;
    operation.target_instance = 0U;
    append_error = operations.append(operation);
    if (append_error != FT_ERR_SUCCESS)
        return (append_error);
    if (test_failure_controller_should_fail(
        TEST_FAILURE_CARD_GAME_OPERATION) == FT_TRUE)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static void card_game_simulation_setup_definition(
    card_game_card_definition &definition, uint32_t card_id,
    uint32_t cost, uint32_t effect_id) noexcept
{
    definition.card_id = card_id;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = cost;
    definition.attack = 4;
    definition.health = 6;
    definition.effect_id = effect_id;
    return ;
}

FT_TEST(test_card_game_simulation_authoritative_delta_between_players)
{
    card_game_engine server;
    card_game_engine client;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_snapshot baseline;
    card_game_delta delta;
    card_game_command command;
    uint64_t server_hash;
    uint64_t client_hash;
    uint32_t board_count;
    uint32_t turn_number;
    uint32_t active_player;

    rules = card_game_simulation_rules();
    card_game_simulation_setup_definition(definition, 100U, 2U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.apply_snapshot(baseline));
    command.command_sequence = 1U;
    command.expected_state_sequence = baseline.state_sequence;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_PLAY_CARD;
    command.card_id = 100U;
    command.target_instance = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_state_hash(&server_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_state_hash(&client_hash));
    FT_ASSERT_EQ(server_hash, client_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_turn(&turn_number, &active_player));
    FT_ASSERT_EQ(2U, turn_number);
    FT_ASSERT_EQ(1U, active_player);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_callback_failure_is_transactional)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_snapshot before;
    card_game_snapshot after;
    uint32_t effect_id;
    uint32_t player_id;
    uint32_t health;

    rules = card_game_simulation_rules();
    player_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_simulation_heal_callback, &player_id, 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(0U, -5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&before));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_fail_next(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(FT_ERR_INTERNAL, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&after));
    FT_ASSERT_EQ(before.state_sequence, after.state_sequence);
    FT_ASSERT_EQ(before.event_count, after.event_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(15U, health);
    FT_ASSERT_EQ(1U, test_failure_controller_failures(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(18U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_fail_next(
        TEST_FAILURE_CARD_GAME_OPERATION));
    FT_ASSERT_EQ(FT_ERR_INTERNAL, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(18U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(21U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_end());
    return (1);
}

FT_TEST(test_card_game_simulation_deck_operations_preserve_order)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    uint32_t card_id;
    uint64_t random_state;
    uint32_t deck_count;

    rules = card_game_simulation_rules();
    random_state = 0x12345678U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    card_game_simulation_setup_definition(definition, 1U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    card_game_simulation_setup_definition(definition, 2U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    card_game_simulation_setup_definition(definition, 3U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    card_game_simulation_setup_definition(definition, 4U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_insert_at(0U, 1U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_draw_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(4U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.shuffle_deck(0U, &random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &deck_count));
    FT_ASSERT_EQ(3U, deck_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_remove(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &deck_count));
    FT_ASSERT_EQ(2U, deck_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_deck_instances_are_inspectable_and_exact)
{
    card_game_engine source;
    card_game_engine destination;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_deck_card card;
    card_game_deck_card preserved;
    card_game_snapshot snapshot;
    uint64_t before_hash;
    uint64_t after_hash;
    uint64_t random_state;

    rules = card_game_simulation_rules();
    card_game_simulation_setup_definition(definition, 50U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_push_bottom_instance(0U,
        1001U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_push_bottom_instance(0U,
        1002U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_push_bottom_instance(0U,
        1003U, 50U));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.deck_push_top_instance(0U,
        1002U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&before_hash));
    preserved.instance_id = 7777U;
    preserved.card_id = 8888U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_inspect(0U, 1U, &card));
    FT_ASSERT_EQ(1002U, card.instance_id);
    FT_ASSERT_EQ(50U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_get_instance(0U, 1002U,
        &preserved));
    FT_ASSERT_EQ(1002U, preserved.instance_id);
    FT_ASSERT_EQ(50U, preserved.card_id);
    preserved.instance_id = 7777U;
    preserved.card_id = 8888U;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.deck_get_instance(0U, 9999U,
        &preserved));
    FT_ASSERT_EQ(7777U, preserved.instance_id);
    FT_ASSERT_EQ(8888U, preserved.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&after_hash));
    FT_ASSERT_EQ(before_hash, after_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_draw_instance(0U, 1002U,
        &card));
    FT_ASSERT_EQ(1002U, card.instance_id);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.deck_get_instance(0U, 1002U,
        &card));
    random_state = 99U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.shuffle_deck(0U, &random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_inspect(0U, 0U, &card));
    FT_ASSERT(card.instance_id == 1001U || card.instance_id == 1003U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_inspect(0U, 1U, &preserved));
    FT_ASSERT(preserved.instance_id == 1001U
        || preserved.instance_id == 1003U);
    FT_ASSERT(card.instance_id != preserved.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.apply_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.deck_inspect(0U, 0U, &card));
    FT_ASSERT_EQ(snapshot.players[0].deck_instance_ids[0], card.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.deck_inspect(0U, 1U, &card));
    FT_ASSERT_EQ(snapshot.players[0].deck_instance_ids[1], card.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_draw_top_returns_exact_entry)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_deck_card card;

    rules = card_game_simulation_rules();
    card_game_simulation_setup_definition(definition, 60U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom_instance(0U,
        6001U, 60U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_draw_top(0U, &card));
    FT_ASSERT_EQ(6001U, card.instance_id);
    FT_ASSERT_EQ(60U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_EMPTY, engine.deck_draw_top(0U, &card));
    FT_ASSERT_EQ(6001U, card.instance_id);
    FT_ASSERT_EQ(60U, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_modifiers_expire_and_sync)
{
    card_game_engine source;
    card_game_engine destination;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_snapshot baseline;
    card_game_snapshot modified_baseline;
    card_game_delta delta;
    card_game_card_instance instance;
    card_game_card_modifier modifier;
    uint32_t permanent_modifier_id;
    uint32_t temporary_modifier_id;
    uint64_t before_hash;
    uint64_t after_hash;

    rules = card_game_simulation_rules();
    card_game_simulation_setup_definition(definition, 70U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.play_card(0U, 70U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_card_modifier(0U, 0U, 1, 0,
        CARD_GAME_MODIFIER_PERMANENT, 10U, &permanent_modifier_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_card_modifier(0U, 0U, 2, 3,
        CARD_GAME_MODIFIER_UNTIL_END_TURN, 11U, &temporary_modifier_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_instance(0U, 0U, &instance));
    FT_ASSERT_EQ(7, instance.attack);
    FT_ASSERT_EQ(9, instance.health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_card_modifier(
        temporary_modifier_id, &modifier));
    FT_ASSERT_EQ(0U, modifier.target_player_id);
    FT_ASSERT_EQ(0U, modifier.target_instance_index);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&modified_baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_instance(0U, 0U, &instance));
    FT_ASSERT_EQ(7, instance.attack);
    FT_ASSERT_EQ(9, instance.health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_instance(0U, 0U, &instance));
    FT_ASSERT_EQ(5, instance.attack);
    FT_ASSERT_EQ(6, instance.health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_delta(modified_baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.get_card_modifier(
        temporary_modifier_id, &modifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_card_modifier(
        permanent_modifier_id, &modifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_instance(0U, 0U, &instance));
    FT_ASSERT_EQ(5, instance.attack);
    FT_ASSERT_EQ(6, instance.health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_state_hash(&before_hash));
    modified_baseline.modifiers[0].modifier_id = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        destination.apply_snapshot(modified_baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_state_hash(&after_hash));
    FT_ASSERT_EQ(before_hash, after_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_combat_uses_effective_stats_and_removes_dead)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition attacker_definition;
    card_game_card_definition defender_definition;
    card_game_card_instance instance;
    uint32_t modifier_id;
    uint32_t board_count;

    rules = card_game_simulation_rules();
    card_game_simulation_setup_definition(attacker_definition, 80U, 0U,
        CARD_GAME_NO_EFFECT);
    card_game_simulation_setup_definition(defender_definition, 81U, 0U,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(attacker_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(defender_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 80U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, 81U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_card_modifier(0U, 0U, 10, 0,
        CARD_GAME_MODIFIER_PERMANENT, 12U, &modifier_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 0U, 1U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_instance(0U, 0U, &instance));
    FT_ASSERT_EQ(14, instance.attack);
    FT_ASSERT_EQ(6, instance.health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_resolution_admission_transcript)
{
    card_game_resolution_stack stack;
    card_game_resolution_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.initialize(8U,
        CARD_GAME_RESOLUTION_LIFO, CARD_GAME_RESOLUTION_OPEN_DEFERRED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.push(1U, 11U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.push(2U, 22U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.push(3U, 33U, 0U, 0U));
    FT_ASSERT_EQ(2U, stack.deferred_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.pop_next(&entry));
    FT_ASSERT_EQ(1U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.end_resolution());
    FT_ASSERT_EQ(2U, stack.deferred_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.pop_next(&entry));
    FT_ASSERT_EQ(3U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.pop_next(&entry));
    FT_ASSERT_EQ(2U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_EMPTY, stack.pop_next(&entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_configured_phase_graph_is_observable)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_phase_definition first_phase;
    card_game_phase_definition second_phase;
    card_game_phase_definition observed_phase;
    uint32_t phase_id;

    rules = card_game_simulation_rules();
    first_phase.phase_id = 10U;
    first_phase.next_phase_id = 20U;
    first_phase.entry_event_type = 0U;
    first_phase.exit_event_type = 1U;
    first_phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    second_phase.phase_id = 20U;
    second_phase.next_phase_id = 10U;
    second_phase.entry_event_type = 0U;
    second_phase.exit_event_type = 0U;
    second_phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(first_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(second_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_current_phase(&phase_id));
    FT_ASSERT_EQ(10U, phase_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_phase(10U, &observed_phase));
    FT_ASSERT_EQ(20U, observed_phase.next_phase_id);
    phase_id = 0U;
    while (phase_id < 1024U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(1U, 0U, 0U));
        phase_id += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.advance_phase());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_current_phase(&phase_id));
    FT_ASSERT_EQ(20U, phase_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.advance_phase());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_current_phase(&phase_id));
    FT_ASSERT_EQ(10U, phase_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_failure_controller_replays_exactly)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_begin());
    FT_ASSERT_EQ(FT_ERR_ALREADY_INITIALISED, test_failure_controller_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_fail_after(
        TEST_FAILURE_SCENARIO_STEP, 2U));
    FT_ASSERT(test_failure_controller_should_fail(
        TEST_FAILURE_SCENARIO_STEP) == FT_FALSE);
    FT_ASSERT(test_failure_controller_should_fail(
        TEST_FAILURE_SCENARIO_STEP) == FT_FALSE);
    FT_ASSERT(test_failure_controller_should_fail(
        TEST_FAILURE_SCENARIO_STEP) == FT_TRUE);
    FT_ASSERT_EQ(3U, test_failure_controller_attempts(
        TEST_FAILURE_SCENARIO_STEP));
    FT_ASSERT_EQ(1U, test_failure_controller_failures(
        TEST_FAILURE_SCENARIO_STEP));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_end());
    FT_ASSERT(test_failure_controller_should_fail(
        TEST_FAILURE_SCENARIO_STEP) == FT_FALSE);
    return (1);
}

FT_TEST(test_card_game_simulation_delta_failure_preserves_existing_output)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    uint32_t pool_id;
    uint32_t unit_id;
    uint64_t previous_target_sequence;

    rules = card_game_simulation_rules();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 1U,
        10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(0U, 1U, 3U,
        0U, 0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.create_delta(baseline, &delta));
    previous_target_sequence = delta.target_state_sequence;
    cma_set_alloc_limit(1U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, engine.create_delta(baseline, &delta));
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(previous_target_sequence, delta.target_state_sequence);
    FT_ASSERT_EQ(FT_CARD_GAME_STATE_FORMAT_VERSION, delta.format_version);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_snapshot_failure_preserves_existing_output)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_snapshot snapshot;
    uint32_t pool_id;
    uint64_t previous_sequence;

    rules = card_game_simulation_rules();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    previous_sequence = snapshot.state_sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 1U,
        10U, &pool_id));
    cma_set_alloc_limit(1U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, engine.get_snapshot(&snapshot));
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(previous_sequence, snapshot.state_sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_hearthstone_style_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition unit_alpha;
    card_game_card_definition unit_beta;
    uint32_t heal_effect_id;
    uint32_t damaged_health;
    uint32_t board_count;
    uint32_t player_one = 1U;

    rules = card_game_simulation_rules();
    rules.max_board_spaces = 7U;
    rules.max_hand_size = 10U;
    rules.starting_health = 30U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    unit_alpha.card_id = 1001U;
    unit_alpha.type = CARD_GAME_CREATURE;
    unit_alpha.cost = 2U;
    unit_alpha.attack = 3;
    unit_alpha.health = 2;
    unit_alpha.effect_id = CARD_GAME_NO_EFFECT;
    unit_beta.card_id = 1002U;
    unit_beta.type = CARD_GAME_CREATURE;
    unit_beta.cost = 1U;
    unit_beta.attack = 2;
    unit_beta.health = 1;
    unit_beta.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_simulation_heal_callback, &player_one, 7001U,
        &heal_effect_id));
    unit_beta.effect_id = heal_effect_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(unit_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(unit_beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(1U, -5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 1001U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(1U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, 1002U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U,
        &damaged_health));
    FT_ASSERT_EQ(28U, damaged_health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_yugioh_style_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition normal_unit;
    card_game_card_definition extra_unit;
    uint32_t normal_summon_limit;
    uint32_t first_unit_count;
    uint32_t second_unit_count;
    uint32_t player_one_health;
    uint32_t player_two_health;

    rules = card_game_simulation_rules();
    rules.max_board_spaces = 5U;
    rules.starting_health = 8000U;
    rules.starting_mana = 0U;
    normal_unit.card_id = 2001U;
    normal_unit.type = CARD_GAME_CREATURE;
    normal_unit.cost = 0U;
    normal_unit.attack = 1500;
    normal_unit.health = 1200;
    normal_unit.effect_id = CARD_GAME_NO_EFFECT;
    extra_unit.card_id = 2002U;
    extra_unit.type = CARD_GAME_CREATURE;
    extra_unit.cost = 0U;
    extra_unit.attack = 2200;
    extra_unit.health = 1800;
    extra_unit.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(normal_unit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(extra_unit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_usage_limit(9001U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_RESOLUTION,
        0U, &normal_summon_limit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.consume_usage_limit(
        normal_summon_limit, 1U, 1U));
    FT_ASSERT_EQ(FT_ERR_FULL, engine.consume_usage_limit(
        normal_summon_limit, 1U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 2001U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, 2002U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(1U, 0U, 0U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U,
        &first_unit_count));
    FT_ASSERT_EQ(0U, first_unit_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U,
        &second_unit_count));
    FT_ASSERT_EQ(1U, second_unit_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(0U, -2200));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U,
        &player_one_health));
    FT_ASSERT_EQ(5800U, player_one_health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U,
        &player_two_health));
    FT_ASSERT_EQ(8000U, player_two_health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_magic_style_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition unit_alpha;
    card_game_card_definition unit_beta;
    card_game_resource_requirement requirement;
    card_game_cost cost;
    card_game_cost_plan payment_plan;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;
    uint32_t board_count;
    uint32_t player_health;

    rules = card_game_simulation_rules();
    rules.max_board_spaces = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 0U;
    unit_alpha.card_id = 3001U;
    unit_alpha.type = CARD_GAME_CREATURE;
    unit_alpha.cost = 2U;
    unit_alpha.attack = 2;
    unit_alpha.health = 2;
    unit_alpha.effect_id = CARD_GAME_NO_EFFECT;
    unit_beta.card_id = 3002U;
    unit_beta.type = CARD_GAME_CREATURE;
    unit_beta.cost = 0U;
    unit_beta.attack = 3;
    unit_beta.health = 3;
    unit_beta.effect_id = CARD_GAME_NO_EFFECT;
    ft_bzero(&requirement, sizeof(requirement));
    ft_bzero(&cost, sizeof(cost));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(unit_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(unit_beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 1U,
        3U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 2U,
        3U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(0U, 1U, 1U,
        0U, 0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(0U, 2U, 1U,
        0U, 0U, FT_FALSE, &unit_id));
    requirement.resource_type_id = 1U;
    requirement.amount = 1U;
    cost.component_count = 1U;
    cost.components[0] = requirement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.create_resource_cost_plan(0U,
        cost, 0U, &payment_plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.spend_resource_cost(payment_plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_resource_pool(0U, 1U, &pool));
    FT_ASSERT_EQ(0U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 3001U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, 3002U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(1U, 0U, 0U, 0U,
        CARD_GAME_COMBAT_SIMULTANEOUS));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &player_health));
    FT_ASSERT_EQ(20U, player_health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t card_game_test_effect(card_game_engine &engine,
    uint32_t source_instance, uint32_t target_instance, void *context) noexcept
{
    (void)source_instance;
    (void)target_instance;
    return (engine.modify_player_health(
        static_cast<uint32_t *>(context)[0], 3));
}

static int32_t card_game_test_event_effect(const card_game_engine &engine,
    const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;

    (void)engine;
    (void)user_data;
    operation.type = CARD_GAME_OPERATION_HEALTH;
    operation.player_id = context.active_player;
    operation.amount = 2;
    operation.event_type = 0U;
    operation.source_instance = context.source_instance;
    operation.target_instance = context.target_instance;
    return (operations.append(operation));
}

static int32_t card_game_test_failing_effect(const card_game_engine &engine,
    const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    (void)engine;
    (void)context;
    (void)operations;
    (void)user_data;
    return (FT_ERR_PERMISSION_DENIED);
}

FT_TEST(test_card_game_engine_dispatches_configured_effect)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    uint32_t effect_id;
    uint32_t player_id;
    uint32_t health;
    uint32_t board_count;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 100U;
    player_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect(
        card_game_test_effect, &effect_id));
    definition.card_id = 42U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 2U;
    definition.attack = 4;
    definition.health = 6;
    definition.effect_id = effect_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 42U, 0U, &player_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(23U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_enforces_configured_board_limit)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    uint32_t effect_id;

    rules.max_board_spaces = 1U;
    rules.max_hand_size = 1U;
    rules.starting_health = 1U;
    rules.starting_mana = 10U;
    rules.max_mana = 10U;
    rules.max_turns = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect(
        card_game_test_effect, &effect_id));
    definition.card_id = 1U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 1;
    definition.health = 1;
    definition.effect_id = effect_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 1U, 0U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_FULL, engine.play_card(0U, 1U, 0U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_resolves_configured_phase_event)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_phase_definition phase;
    uint32_t effect_id;
    uint32_t health;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 10U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    phase.phase_id = 1U;
    phase.next_phase_id = 1U;
    phase.entry_event_type = 77U;
    phase.exit_event_type = 0U;
    phase.allowed_command_mask = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_test_event_effect, ft_nullptr, 77U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(0U, -5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(77U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(9U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_enforces_phase_command_mask)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_phase_definition phase;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 10U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    phase.phase_id = 1U;
    phase.next_phase_id = 1U;
    phase.entry_event_type = 0U;
    phase.exit_event_type = 0U;
    phase.allowed_command_mask = CARD_GAME_COMMAND_PLAY_CARD;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.advance_phase());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_creates_and_applies_authoritative_delta)
{
    card_game_engine source_engine;
    card_game_engine destination_engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    uint32_t health;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 3U;
    rules.starting_health = 20U;
    rules.starting_mana = 4U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.modify_player_health(0U, -6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.set_player_mana(0U, 9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.get_player_health(0U,
        &health));
    FT_ASSERT_EQ(14U, health);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, destination_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_rolls_back_card_when_effect_fails)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    uint32_t effect_id;
    uint32_t board_count;
    uint32_t health;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_test_failing_effect, ft_nullptr, 0U, &effect_id));
    definition.card_id = 91U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 2U;
    definition.attack = 3;
    definition.health = 4;
    definition.effect_id = effect_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.play_card(0U, 91U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(20U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_rolls_back_turn_when_event_effect_fails)
{
    card_game_engine engine;
    card_game_rules rules;
    uint32_t effect_id;
    uint32_t turn_number;
    uint32_t active_player;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_test_failing_effect, ft_nullptr, 77U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(77U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_turn(&turn_number,
        &active_player));
    FT_ASSERT_EQ(1U, turn_number);
    FT_ASSERT_EQ(0U, active_player);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_accepts_cards_without_effects)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    uint32_t board_count;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    definition.card_id = 123U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 2;
    definition.health = 3;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 123U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_exposes_deterministic_rules_and_state_hashes)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    uint64_t first_rules_hash;
    uint64_t second_rules_hash;
    uint64_t first_state_hash;
    uint64_t second_state_hash;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_rules_hash(
        &first_rules_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_rules_hash(
        &second_rules_hash));
    FT_ASSERT_EQ(first_rules_hash, second_rules_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_state_hash(&first_state_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_engine.get_state_hash(&second_state_hash));
    FT_ASSERT_EQ(first_state_hash, second_state_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.modify_player_health(0U, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_state_hash(&first_state_hash));
    FT_ASSERT_NEQ(first_state_hash, second_state_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_registers_configurable_zones)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_zone_definition zone;
    card_game_zone_definition loaded_zone;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    zone.zone_id = 7U;
    zone.capacity = 30U;
    zone.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    zone.owner_scoped = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_zone(7U, &loaded_zone));
    FT_ASSERT_EQ(zone.zone_id, loaded_zone.zone_id);
    FT_ASSERT_EQ(zone.capacity, loaded_zone.capacity);
    FT_ASSERT_EQ(zone.allowed_card_type_mask,
        loaded_zone.allowed_card_type_mask);
    FT_ASSERT_EQ(zone.owner_scoped, loaded_zone.owner_scoped);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, engine.register_zone(zone));
    zone.zone_id = 8U;
    zone.allowed_card_type_mask = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.register_zone(zone));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.get_zone(9U, &loaded_zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_registers_custom_card_types)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_type_definition type;
    card_game_card_type_definition loaded_type;
    card_game_card_definition definition;
    card_game_card_type_definition disallowed_type;
    card_game_card_definition disallowed_definition;
    card_game_zone_definition board_zone;
    uint32_t board_count;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    type.type_id = 4U;
    type.allowed_zone_mask = 1U << CARD_GAME_BOARD_ZONE_ID;
    type.max_copies_per_player = 2U;
    definition.card_id = 200U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 2;
    definition.health = 2;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    board_zone.zone_id = CARD_GAME_BOARD_ZONE_ID;
    board_zone.capacity = 3U;
    board_zone.allowed_card_type_mask = 1U << 4U;
    board_zone.owner_scoped = FT_TRUE;
    disallowed_type.type_id = 5U;
    disallowed_type.allowed_zone_mask = 1U << 2U;
    disallowed_type.max_copies_per_player = 2U;
    disallowed_definition = definition;
    disallowed_definition.card_id = 201U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card_type(type));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(board_zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_card_type(4U, &loaded_type));
    FT_ASSERT_EQ(type.type_id, loaded_type.type_id);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.register_card_with_type(
        definition, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card_with_type(
        definition, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card_type(disallowed_type));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card_with_type(
        disallowed_definition, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 200U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.play_card(0U, 201U, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, engine.register_card_type(type));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_validates_authoritative_command_envelopes)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_command command;
    card_game_snapshot snapshot;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    definition.card_id = 210U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 2;
    definition.health = 3;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    command.command_sequence = 1U;
    command.expected_state_sequence = 0U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_PLAY_CARD;
    command.card_id = 210U;
    command.target_instance = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.submit_command(command,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    command.command_sequence = 2U;
    command.expected_state_sequence = snapshot.state_sequence - 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, engine.submit_command(command,
        ft_nullptr));
    command.expected_state_sequence = 0U;
    command.player_id = 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.submit_command(command,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_records_authoritative_commands)
{
    card_game_engine first;
    card_game_engine second;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_command command;
    card_game_command_record record;
    uint32_t record_count;
    uint64_t first_rules_hash;
    uint64_t second_rules_hash;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    definition.card_id = 220U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 1U;
    definition.attack = 2;
    definition.health = 3;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    command.command_sequence = 1U;
    command.expected_state_sequence = 0U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_PLAY_CARD;
    command.card_id = 220U;
    command.target_instance = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get_rules_hash(&first_rules_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.get_rules_hash(&second_rules_hash));
    FT_ASSERT_EQ(first_rules_hash, second_rules_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get_command_record_count(&record_count));
    FT_ASSERT_EQ(1U, record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get_command_record(0U, &record));
    FT_ASSERT_EQ(command.command_sequence, record.command.command_sequence);
    FT_ASSERT_EQ(first_rules_hash, record.rules_hash);
    FT_ASSERT(record.state_hash_before != record.state_hash_after);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, first.get_command_record(1U, &record));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, first.submit_command(command,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get_command_record_count(&record_count));
    FT_ASSERT_EQ(1U, record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

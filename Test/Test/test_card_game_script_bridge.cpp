#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_script_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_card_game_script_bridge_draws_through_event_operation)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard_zone;
    card_game_zone_definition burned_zone;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_draw_policy draw_policy;
    card_game_snapshot snapshot;
    uint32_t effect_id;
    uint32_t hand_count;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&card, sizeof(card));
    card.card_id = 701U;
    card.type = CARD_GAME_CREATURE;
    card.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(card));
    ft_bzero(&discard_zone, sizeof(discard_zone));
    discard_zone.zone_id = 41U;
    discard_zone.capacity = 8U;
    discard_zone.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    discard_zone.owner_scoped = FT_TRUE;
    ft_bzero(&burned_zone, sizeof(burned_zone));
    burned_zone.zone_id = 42U;
    burned_zone.capacity = 8U;
    burned_zone.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    burned_zone.owner_scoped = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(discard_zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(burned_zone));
    ft_bzero(&empty_event, sizeof(empty_event));
    empty_event.type_id = 601U;
    empty_event.version = 1U;
    empty_event.field_count = 2U;
    empty_event.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    empty_event.field_types[1] = CARD_GAME_HOST_INTENT_U32;
    ft_bzero(&overflow_event, sizeof(overflow_event));
    overflow_event.type_id = 602U;
    overflow_event.version = 1U;
    overflow_event.field_count = 2U;
    overflow_event.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    overflow_event.field_types[1] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(empty_event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.register_host_intent_type(overflow_event));
    ft_bzero(&draw_policy, sizeof(draw_policy));
    draw_policy.discard_zone_id = discard_zone.zone_id;
    draw_policy.burned_zone_id = burned_zone.zone_id;
    draw_policy.reshuffle_discard = FT_TRUE;
    draw_policy.empty_draw_event_type_id = empty_event.type_id;
    draw_policy.empty_draw_event_version = empty_event.version;
    draw_policy.overflow_event_type_id = overflow_event.type_id;
    draw_policy.overflow_event_version = overflow_event.version;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.configure_draw_policy(draw_policy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "draw_cards(1);", 899U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(899U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(1U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "require_discard(1);", 898U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(898U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(1U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_player_id);
    FT_ASSERT_EQ(discard_zone.zone_id, snapshot.pending_discard_zone_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_emits_typed_host_intent)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    uint32_t effect_id;
    uint32_t intent_count;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_I64;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "emit_host_intent(800, 1, 4, -12);", 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&intent_count));
    FT_ASSERT_EQ(1U, intent_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(800U, record.intent.type_id);
    FT_ASSERT_EQ(1U, record.intent.version);
    FT_ASSERT_EQ(1U, record.intent.field_count);
    FT_ASSERT_EQ(CARD_GAME_HOST_INTENT_I64, record.intent.fields[0].type);
    FT_ASSERT_EQ(static_cast<uint64_t>(-12LL),
        record.intent.fields[0].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_unregisters_callbacks_on_destroy)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    uint32_t effect_id;
    uint32_t pending_count;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 802U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "emit_host_intent(802, 1, 1, 7);", 902U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(902U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_pending_host_intent_count(
        &pending_count));
    FT_ASSERT_EQ(0U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_exposes_invocation_context)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_phase_definition phase;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    uint32_t effect_id;
    uint32_t phase_id;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 3U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    definition.field_types[1] = CARD_GAME_HOST_INTENT_U32;
    definition.field_types[2] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    ft_bzero(&phase, sizeof(phase));
    phase.phase_id = 19U;
    phase.next_phase_id = 19U;
    phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_current_phase(&phase_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let actor = card_actor_id(); let instance = card_instance_id(); "
        "let phase = session_phase(); "
        "emit_host_intent(800, 1, 1, actor, 1, instance, 1, phase);",
        900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 37U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(3U, record.intent.field_count);
    FT_ASSERT_EQ(0U, record.intent.fields[0].value);
    FT_ASSERT_EQ(37U, record.intent.fields[1].value);
    FT_ASSERT_EQ(phase_id, record.intent.fields[2].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_reads_manifested_policy_context)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    uint32_t effect_id;
    uint64_t manifest_before_value;
    uint64_t manifest_after_value;
    std::vector<uint8_t> context_snapshot;
    std::vector<uint8_t> corrupt_context_snapshot;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 801U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_I64;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_manifest_hash(&manifest_before_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_context_key(41U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, bridge.set_context_value(99U, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_manifest_hash(&manifest_after_value));
    FT_ASSERT_NE(manifest_before_value, manifest_after_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.set_context_value(41U, 23));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_manifest_hash(&manifest_before_value));
    FT_ASSERT_EQ(manifest_after_value, manifest_before_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let policy = context_value(41); "
        "emit_host_intent(801, 1, 4, policy);", 901U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.register_context_key(42U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(901U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(1U, record.intent.field_count);
    FT_ASSERT_EQ(static_cast<uint64_t>(23), record.intent.fields[0].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        bridge.serialize_context_values(context_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.clear_context_value(41U));
    corrupt_context_snapshot = context_snapshot;
    corrupt_context_snapshot[12U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.restore_context_values(
        corrupt_context_snapshot.data(),
        static_cast<uint32_t>(corrupt_context_snapshot.size())));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.restore_context_values(
        context_snapshot.data(), static_cast<uint32_t>(context_snapshot.size())));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(901U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(static_cast<uint64_t>(23), record.intent.fields[0].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_uses_match_owned_bounded_rng)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    uint32_t effect_id;
    uint32_t intent_count;
    uint64_t random_state_before;
    uint64_t random_state_after;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_random_seed(0x123456789ULL));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let value = random_bounded(17); "
        "emit_host_intent(800, 1, 1, value);", 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_random_state(&random_state_before));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_random_state(&random_state_after));
    FT_ASSERT_NE(random_state_before, random_state_after);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&intent_count));
    FT_ASSERT_EQ(1U, intent_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(CARD_GAME_HOST_INTENT_U32, record.intent.fields[0].type);
    FT_ASSERT(record.intent.fields[0].value < 17U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_rng_failure_rolls_back)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    uint32_t effect_id;
    uint64_t random_state_before;
    uint64_t random_state_after;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_random_seed(0x987654321ULL));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let value = random_bounded(17); "
        "emit_host_intent(801, 1, 1, value);", 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_random_state(&random_state_before));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_random_state(&random_state_after));
    FT_ASSERT_EQ(random_state_before, random_state_after);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_reads_only_authorized_resolved_choices)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    card_game_choice_option option;
    uint32_t owned_choice_id;
    uint32_t other_choice_id;
    uint32_t owned_effect_id;
    uint32_t other_effect_id;
    uint32_t intent_count;

    ft_bzero(&rules, sizeof(rules));
    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.open_choice(0U,
        CARD_GAME_CHOICE_TARGET, 100U, 0U, &owned_choice_id));
    ft_bzero(&option, sizeof(option));
    option.option_id = 17U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_choice_option(owned_choice_id,
        option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.choose_option(owned_choice_id, 0U,
        option.option_id, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.open_choice(1U,
        CARD_GAME_CHOICE_TARGET, 100U, 0U, &other_choice_id));
    option.option_id = 23U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_choice_option(other_choice_id,
        option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.choose_option(other_choice_id, 1U,
        option.option_id, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let selected = choice_selected_id(1); "
        "emit_host_intent(800, 1, 1, selected);", 900U, &owned_effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "let selected = choice_selected_id(2); "
        "emit_host_intent(800, 1, 1, selected);", 901U, &other_effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(17U, record.intent.fields[0].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(901U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&intent_count));
    FT_ASSERT_EQ(1U, intent_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_rejects_untyped_script_arguments)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    uint32_t effect_id;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_I64;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "emit_host_intent(800, 1, 4, true);", 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_NE(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_rejects_negative_unsigned_intent)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    uint32_t effect_id;
    uint32_t intent_count;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 800U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_script_effect(
        "emit_host_intent(800, 1, 1, -1);", 900U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(900U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&intent_count));
    FT_ASSERT_EQ(0U, intent_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_manifest_tracks_content_not_addresses)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_script_bridge first_bridge;
    card_game_script_bridge second_bridge;
    card_game_rules rules;
    uint32_t first_effect;
    uint32_t second_effect;
    uint64_t first_hash;
    uint64_t second_hash;
    uint64_t changed_hash;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_bridge.initialize(first_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_bridge.initialize(second_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_bridge.register_script_effect(
        "emit_host_intent(800, 1);", 900U, &first_effect));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_bridge.register_script_effect(
        "emit_host_intent(800, 1);", 900U, &second_effect));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_bridge.get_manifest_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_bridge.get_manifest_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_bridge.register_script_effect(
        "emit_host_intent(801, 1);", 901U, &first_effect));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        first_bridge.get_manifest_hash(&changed_hash));
    FT_ASSERT_NE(first_hash, changed_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_bridge.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_manifest_lifecycle_and_pointer)
{
    card_game_engine engine;
    card_game_script_bridge bridge;
    card_game_rules rules;
    uint64_t hash;

    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, bridge.get_manifest_hash(&hash));
    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_manifest_hash(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, bridge.get_manifest_hash(&hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_script_bridge_loads_trusted_script_catalog)
{
    card_game_engine engine;
    card_game_script_bridge source_bridge;
    card_game_script_bridge loaded_bridge;
    card_game_rules rules;
    card_game_host_intent_definition definition;
    card_game_host_intent_record record;
    std::vector<uint8_t> catalog_bytes;
    std::vector<uint8_t> corrupt_catalog_bytes;
    uint32_t effect_id;
    uint32_t effect_count;
    uint32_t intent_count;
    uint64_t trusted_manifest_hash;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 910U;
    definition.version = 1U;
    definition.field_count = 1U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_host_intent_type(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_bridge.initialize(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_bridge.set_policy_manifest_hash(0x504F4C4943590001ULL));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_bridge.register_script_effect(
        "emit_host_intent(910, 1, 1, 77);", 911U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_bridge.get_manifest_hash(&trusted_manifest_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_bridge.serialize_script_catalog(catalog_bytes));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_bridge.initialize(engine));
    corrupt_catalog_bytes = catalog_bytes;
    corrupt_catalog_bytes[corrupt_catalog_bytes.size() - 1U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, loaded_bridge.load_script_catalog(
        corrupt_catalog_bytes.data(),
        static_cast<uint32_t>(corrupt_catalog_bytes.size()),
        trusted_manifest_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.get_registered_effect_count(&effect_count));
    FT_ASSERT_EQ(0U, effect_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_bridge.set_operation_limit(511U));
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, loaded_bridge.load_script_catalog(
        catalog_bytes.data(), static_cast<uint32_t>(catalog_bytes.size()),
        trusted_manifest_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.get_registered_effect_count(&effect_count));
    FT_ASSERT_EQ(0U, effect_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_bridge.set_operation_limit(512U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.set_policy_manifest_hash(0x504F4C4943590002ULL));
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, loaded_bridge.load_script_catalog(
        catalog_bytes.data(), static_cast<uint32_t>(catalog_bytes.size()),
        trusted_manifest_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.get_registered_effect_count(&effect_count));
    FT_ASSERT_EQ(0U, effect_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.set_policy_manifest_hash(0x504F4C4943590001ULL));
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, loaded_bridge.load_script_catalog(
        catalog_bytes.data(), static_cast<uint32_t>(catalog_bytes.size()),
        trusted_manifest_hash ^ 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.get_registered_effect_count(&effect_count));
    FT_ASSERT_EQ(0U, effect_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_bridge.load_script_catalog(
        catalog_bytes.data(), static_cast<uint32_t>(catalog_bytes.size()),
        trusted_manifest_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_bridge.get_registered_effect_count(&effect_count));
    FT_ASSERT_EQ(1U, effect_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(911U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&intent_count));
    FT_ASSERT_EQ(1U, intent_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(910U, record.intent.type_id);
    FT_ASSERT_EQ(77U, record.intent.fields[0].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_script_environment_hash_sorts_host_schemas)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    card_game_host_intent_definition first_definition;
    card_game_host_intent_definition second_definition;
    uint64_t first_hash;
    uint64_t second_hash;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    ft_bzero(&first_definition, sizeof(first_definition));
    first_definition.type_id = 920U;
    first_definition.version = 1U;
    first_definition.field_count = 1U;
    first_definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    ft_bzero(&second_definition, sizeof(second_definition));
    second_definition.type_id = 921U;
    second_definition.version = 2U;
    second_definition.field_count = 2U;
    second_definition.field_types[0] = CARD_GAME_HOST_INTENT_I64;
    second_definition.field_types[1] = CARD_GAME_HOST_INTENT_U32;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        first_engine.register_host_intent_type(first_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        first_engine.register_host_intent_type(second_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_engine.register_host_intent_type(second_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_engine.register_host_intent_type(first_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        first_engine.get_script_environment_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second_engine.get_script_environment_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

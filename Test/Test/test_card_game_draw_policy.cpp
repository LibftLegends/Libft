#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_replay.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void card_game_draw_policy_test_setup(card_game_rules &rules,
    card_game_card_definition &card, card_game_zone_definition &discard,
    card_game_zone_definition &burned,
    card_game_host_intent_definition &empty_event,
    card_game_host_intent_definition &overflow_event)
{
    rules.max_board_spaces = 2U;
    rules.max_hand_size = 1U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 1U;
    rules.max_turns = 5U;
    ft_bzero(&card, sizeof(card));
    card.card_id = 931U;
    card.type = CARD_GAME_CREATURE;
    card.effect_id = CARD_GAME_NO_EFFECT;
    discard.zone_id = 41U;
    discard.capacity = 8U;
    discard.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    discard.owner_scoped = FT_TRUE;
    burned.zone_id = 42U;
    burned.capacity = 8U;
    burned.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    burned.owner_scoped = FT_TRUE;
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
}

static int32_t card_game_draw_policy_test_configure(card_game_engine &engine,
    const card_game_rules &rules, const card_game_card_definition &card,
    const card_game_zone_definition &discard,
    const card_game_zone_definition &burned,
    const card_game_host_intent_definition &empty_event,
    const card_game_host_intent_definition &overflow_event,
    ft_bool reshuffle)
{
    card_game_draw_policy policy;
    int32_t result;

    result = engine.initialize(rules);
    if (result == FT_ERR_SUCCESS)
        result = engine.register_card(card);
    if (result == FT_ERR_SUCCESS)
        result = engine.register_zone(discard);
    if (result == FT_ERR_SUCCESS)
        result = engine.register_zone(burned);
    if (result == FT_ERR_SUCCESS)
        result = engine.register_host_intent_type(empty_event);
    if (result == FT_ERR_SUCCESS)
        result = engine.register_host_intent_type(overflow_event);
    ft_bzero(&policy, sizeof(policy));
    policy.discard_zone_id = reshuffle == FT_TRUE ? discard.zone_id : 0U;
    policy.burned_zone_id = burned.zone_id;
    policy.reshuffle_discard = reshuffle;
    policy.empty_draw_event_type_id = empty_event.type_id;
    policy.empty_draw_event_version = empty_event.version;
    policy.overflow_event_type_id = overflow_event.type_id;
    policy.overflow_event_version = overflow_event.version;
    if (result == FT_ERR_SUCCESS)
        result = engine.configure_draw_policy(policy);
    return (result);
}

static int32_t card_game_draw_policy_test_submit(card_game_engine &engine)
{
    card_game_command command;

    ft_bzero(&command, sizeof(command));
    command.command_sequence = 1U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = 1U;
    return (engine.submit_command(command, ft_nullptr));
}

static int32_t card_game_draw_policy_test_submit_count(
    card_game_engine &engine, uint32_t sequence, uint32_t count)
{
    card_game_command command;

    ft_bzero(&command, sizeof(command));
    command.command_sequence = sequence;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = count;
    return (engine.submit_command(command, ft_nullptr));
}

static int32_t card_game_draw_policy_test_submit_required(
    card_game_engine &engine, uint32_t sequence, uint32_t draw_count,
    uint32_t required_discard_count)
{
    card_game_command command;

    ft_bzero(&command, sizeof(command));
    command.command_sequence = sequence;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = draw_count;
    command.required_discard_count = required_discard_count;
    return (engine.submit_command(command, ft_nullptr));
}

static int32_t card_game_draw_policy_test_submit_discard(
    card_game_engine &engine, uint32_t sequence, uint32_t instance_id)
{
    card_game_command command;

    ft_bzero(&command, sizeof(command));
    command.command_sequence = sequence;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DISCARD_HAND;
    command.card_id = 41U;
    command.target_instance = instance_id;
    return (engine.submit_command(command, ft_nullptr));
}

FT_TEST(test_card_game_draw_policy_multi_draw_continues_after_empty_attempts)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_host_intent_record intent;
    card_game_command_record command_record;
    uint8_t encoded[1024];
    uint32_t encoded_size;
    uint32_t hand_count;
    uint32_t pending;
    uint32_t index;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    rules.max_hand_size = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_draw_policy_test_submit_count(engine, 1U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(2U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(2U, pending);
    index = 0U;
    while (index < pending)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&intent));
        FT_ASSERT_EQ(empty_event.type_id, intent.intent.type_id);
        FT_ASSERT_EQ(0U, intent.intent.fields[1].value);
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            engine.acknowledge_host_intent(intent.sequence));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_command_record(0U,
        &command_record));
    FT_ASSERT_EQ(4U, command_record.command.draw_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.serialize_command_records(encoded,
        sizeof(encoded), &encoded_size));
    FT_ASSERT_EQ(13U, encoded[4]);
    FT_ASSERT_EQ(4U, encoded[64]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_policy_multi_draw_burns_each_overflow)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_deck_card drawn;
    card_game_host_intent_record intent;
    uint32_t pending;
    uint32_t burned_count;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &drawn));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_draw_policy_test_submit_count(engine, 1U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_zone_count(0U, burned.zone_id,
        &burned_count));
    FT_ASSERT_EQ(3U, burned_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(3U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&intent));
    FT_ASSERT_EQ(overflow_event.type_id, intent.intent.type_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.acknowledge_host_intent(
        intent.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&intent));
    FT_ASSERT_EQ(overflow_event.type_id, intent.intent.type_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.acknowledge_host_intent(
        intent.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&intent));
    FT_ASSERT_EQ(overflow_event.type_id, intent.intent.type_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_required_discard_blocks_until_physical_discard)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_phase_definition discard_restricted_phase;
    card_game_snapshot snapshot;
    card_game_deck_card hand_card;
    card_game_deck_card foreign_card;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    rules.max_hand_size = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    ft_bzero(&discard_restricted_phase, sizeof(discard_restricted_phase));
    discard_restricted_phase.phase_id = 7U;
    discard_restricted_phase.next_phase_id = 7U;
    discard_restricted_phase.allowed_command_mask =
        CARD_GAME_COMMAND_END_TURN | CARD_GAME_COMMAND_DRAW_CARD;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_phase(
        discard_restricted_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &foreign_card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_required(
        engine, 1U, 2U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(0U, snapshot.pending_discard_player_id);
    FT_ASSERT_EQ(1U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(discard.zone_id, snapshot.pending_discard_zone_id);

    {
        card_game_command command;

        ft_bzero(&command, sizeof(command));
        command.command_sequence = 2U;
        command.player_id = 0U;
        command.type = CARD_GAME_INTENT_END_TURN;
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
            engine.submit_command(command, ft_nullptr));
        command.type = CARD_GAME_INTENT_DRAW_CARD;
        command.draw_count = 1U;
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
            engine.submit_command(command, ft_nullptr));
    }
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, card_game_draw_policy_test_submit_discard(
        engine, 2U, foreign_card.instance_id));
    {
        card_game_command command;

        ft_bzero(&command, sizeof(command));
        command.command_sequence = 2U;
        command.player_id = 0U;
        command.type = CARD_GAME_INTENT_DISCARD_HAND;
        command.card_id = discard.zone_id + 1U;
        command.target_instance = foreign_card.instance_id;
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
            engine.submit_command(command, ft_nullptr));
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 0U, &hand_card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_discard(
        engine, 2U, hand_card.instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(0U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_player_id);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_zone_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_required_discard_preserves_partial_progress)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_snapshot snapshot;
    card_game_deck_card first;
    card_game_deck_card second;
    card_game_deck_card removed_discard;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    rules.max_hand_size = 4U;
    discard.capacity = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_required(
        engine, 1U, 3U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 0U, &first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 1U, &second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_discard(
        engine, 2U, first.instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(1U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_player_id);
    FT_ASSERT_EQ(discard.zone_id, snapshot.pending_discard_zone_id);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, card_game_draw_policy_test_submit_discard(
        engine, 3U, first.instance_id));
    FT_ASSERT_EQ(FT_ERR_FULL, card_game_draw_policy_test_submit_discard(
        engine, 3U, second.instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(1U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.zone_pop_top(0U, discard.zone_id,
        &removed_discard));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_discard(
        engine, 3U, second.instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(0U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_required_discard_empty_hand_has_no_obligation)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_snapshot snapshot;
    uint32_t hand_count;
    uint32_t pending_intents;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_required(
        engine, 1U, 2U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(0U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_player_id);
    FT_ASSERT_EQ(0U, snapshot.pending_discard_zone_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(0U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending_intents));
    FT_ASSERT_EQ(2U, pending_intents);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_required_discard_is_independent_of_reshuffle_policy)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_draw_policy policy;
    card_game_snapshot snapshot;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(discard));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_zone(burned));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.register_host_intent_type(empty_event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.register_host_intent_type(overflow_event));
    ft_bzero(&policy, sizeof(policy));
    policy.discard_zone_id = discard.zone_id;
    policy.burned_zone_id = burned.zone_id;
    policy.reshuffle_discard = FT_FALSE;
    policy.empty_draw_event_type_id = empty_event.type_id;
    policy.empty_draw_event_version = empty_event.version;
    policy.overflow_event_type_id = overflow_event.type_id;
    policy.overflow_event_version = overflow_event.version;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.configure_draw_policy(policy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_draw_policy_test_submit_required(engine, 1U, 1U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&snapshot));
    FT_ASSERT_EQ(1U, snapshot.pending_discard_count);
    FT_ASSERT_EQ(discard.zone_id, snapshot.pending_discard_zone_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_required_discard_snapshot_delta_and_replay)
{
    card_game_engine *source = new card_game_engine();
    card_game_engine *destination = new card_game_engine();
    card_game_engine *log_reader = new card_game_engine();
    card_game_engine *replay_target = new card_game_engine();
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_snapshot *baseline = new card_game_snapshot();
    card_game_snapshot *pending_snapshot = new card_game_snapshot();
    card_game_snapshot *reverted_snapshot = new card_game_snapshot();
    card_game_delta *delta = new card_game_delta();
    card_game_replay_header replay_header;
    card_game_replay replay;
    card_game_replay replay_copy;
    card_game_replay_event replay_event;
    card_game_deck_card hand_card;
    card_game_command_record records[2];
    uint8_t encoded[1024];
    uint8_t replay_encoded[1024];
    uint32_t encoded_size;
    uint32_t replay_encoded_size;
    uint64_t source_hash;
    uint64_t destination_hash;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    rules.max_hand_size = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(*source,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(
        *destination, rules, card, discard, burned, empty_event, overflow_event,
        FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(
        *log_reader, rules, card, discard, burned, empty_event, overflow_event,
        FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(
        *replay_target, rules, card, discard, burned, empty_event,
        overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_snapshot(*baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_required(
        *source, 1U, 2U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_snapshot(pending_snapshot));
    FT_ASSERT_EQ(1U, pending_snapshot->pending_discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->create_delta(*baseline, delta));
    FT_ASSERT_EQ(FT_TRUE, delta->global_state_changed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_delta(*delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->get_state_hash(&destination_hash));
    FT_ASSERT_EQ(source_hash, destination_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_snapshot(*baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_snapshot(*pending_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->get_snapshot(reverted_snapshot));
    FT_ASSERT_EQ(1U, reverted_snapshot->pending_discard_count);
    reverted_snapshot->pending_discard_count = 0U;
    reverted_snapshot->pending_discard_player_id = 0U;
    reverted_snapshot->pending_discard_zone_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_snapshot(*reverted_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->get_state_hash(&destination_hash));
    FT_ASSERT_NEQ(source_hash, destination_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->apply_snapshot(*pending_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->get_state_hash(&destination_hash));
    FT_ASSERT_EQ(source_hash, destination_hash);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->hand_inspect(0U, 0U, &hand_card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit_discard(
        *source, 2U, hand_card.instance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->serialize_command_records(encoded,
        sizeof(encoded), &encoded_size));
    FT_ASSERT_EQ(13U, encoded[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, log_reader->deserialize_command_records(
        encoded, encoded_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, log_reader->get_command_record(0U,
        &records[0]));
    FT_ASSERT_EQ(1U, records[0].command.required_discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, log_reader->get_command_record(1U,
        &records[1]));
    ft_bzero(&replay_header, sizeof(replay_header));
    replay_header.rules_hash = records[0].rules_hash;
    replay_header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(records[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&replay_encoded_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(replay_encoded,
        sizeof(replay_encoded), &replay_encoded_size));
    FT_ASSERT_EQ(11U, replay_encoded[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_copy.deserialize(replay_encoded,
        replay_encoded_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_copy.get_event(0U, &replay_event));
    FT_ASSERT_EQ(1U, replay_event.required_discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->replay_command_records(
        records, 2U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->get_state_hash(&destination_hash));
    FT_ASSERT_EQ(source_hash, destination_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, log_reader->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_copy.destroy());
    delete source;
    delete destination;
    delete log_reader;
    delete replay_target;
    delete baseline;
    delete pending_snapshot;
    delete reverted_snapshot;
    delete delta;
    return (1);
}

FT_TEST(test_card_game_draw_policy_multi_draw_bounds_are_strict)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    uint32_t pending;
    uint32_t records;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        card_game_draw_policy_test_submit_count(engine, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        card_game_draw_policy_test_submit_count(engine, 1U, 101U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(0U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_command_record_count(&records));
    FT_ASSERT_EQ(0U, records);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_draw_policy_test_submit_count(engine, 1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(100U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_multi_draw_count_survives_log_replay_and_retry)
{
    card_game_engine *source = new card_game_engine();
    card_game_engine *reader = new card_game_engine();
    card_game_engine *target = new card_game_engine();
    card_game_engine *replay_target = new card_game_engine();
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_command_record record;
    card_game_replay_header replay_header;
    card_game_replay replay;
    card_game_replay restored_replay;
    card_game_action action;
    card_game_action_result action_result;
    uint8_t encoded[1024];
    uint32_t encoded_size;
    uint32_t replay_size;
    uint32_t hand_count;
    uint32_t pending;
    uint32_t source_pending;
    uint32_t intent_index;
    uint32_t field_index;
    uint64_t source_hash;
    uint64_t target_hash;
    card_game_host_intent_record source_intent;
    card_game_host_intent_record replay_intent;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    rules.max_hand_size = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(*source,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->deck_push_top(0U, card.card_id));
    ft_bzero(&action, sizeof(action));
    action.schema_version = 1U;
    action.request_id = 7001U;
    action.command.command_sequence = 1U;
    action.command.player_id = 0U;
    action.command.type = CARD_GAME_INTENT_DRAW_CARD;
    action.command.draw_count = 4U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->submit_action(action,
        &action_result, ft_nullptr));
    FT_ASSERT_EQ(FT_FALSE, action_result.replayed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->submit_action(action,
        &action_result, ft_nullptr));
    FT_ASSERT_EQ(FT_TRUE, action_result.replayed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_command_record(0U, &record));
    FT_ASSERT_EQ(4U, record.command.draw_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->serialize_command_records(encoded,
        sizeof(encoded), &encoded_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader->initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader->deserialize_command_records(encoded,
        encoded_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader->get_command_record(0U, &record));
    FT_ASSERT_EQ(4U, record.command.draw_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(*target,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->replay_command_records(&record, 1U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(2U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(2U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->get_state_hash(&target_hash));
    FT_ASSERT_EQ(source_hash, target_hash);
    ft_bzero(&replay_header, sizeof(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_rules_hash(&replay_header.rules_hash));
    replay_header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&replay_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(encoded, sizeof(encoded),
        &replay_size));
    FT_ASSERT_EQ(11U, encoded[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.deserialize(encoded,
        replay_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(
        *replay_target, rules, card, discard, burned, empty_event,
        overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.replay_into(*replay_target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_pending_host_intent_count(
        &source_pending));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->get_pending_host_intent_count(
        &pending));
    FT_ASSERT_EQ(source_pending, pending);
    intent_index = 0U;
    while (intent_index < source_pending)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source->inspect_pending_host_intent(
            intent_index, &source_intent));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->inspect_pending_host_intent(
            intent_index, &replay_intent));
        FT_ASSERT_EQ(source_intent.sequence, replay_intent.sequence);
        FT_ASSERT_EQ(source_intent.intent.type_id,
            replay_intent.intent.type_id);
        FT_ASSERT_EQ(source_intent.intent.version,
            replay_intent.intent.version);
        FT_ASSERT_EQ(source_intent.intent.field_count,
            replay_intent.intent.field_count);
        field_index = 0U;
        while (field_index < source_intent.intent.field_count)
        {
            FT_ASSERT_EQ(source_intent.intent.fields[field_index].type,
                replay_intent.intent.fields[field_index].type);
            FT_ASSERT_EQ(source_intent.intent.fields[field_index].value,
                replay_intent.intent.fields[field_index].value);
            field_index += 1U;
        }
        intent_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->get_state_hash(&target_hash));
    FT_ASSERT_EQ(source_hash, target_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_target->destroy());
    delete source;
    delete reader;
    delete target;
    delete replay_target;
    return (1);
}

FT_TEST(test_card_game_draw_policy_empty_deck_emits_one_intent)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_host_intent_record record;
    uint32_t pending;
    uint32_t hand_count;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(0U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending));
    FT_ASSERT_EQ(1U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(empty_event.type_id, record.intent.type_id);
    FT_ASSERT_EQ(0U, record.intent.fields[0].value);
    FT_ASSERT_EQ(0U, record.intent.fields[1].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_policy_full_hand_burns_instance_and_emits_intent)
{
    card_game_engine engine;
    card_game_engine target;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_deck_card initial_draw;
    card_game_deck_card pending_draw;
    card_game_deck_card burned_card;
    card_game_host_intent_record record;
    card_game_command_record command_record;
    card_game_replay_header replay_header;
    card_game_replay replay;
    uint32_t pending;
    uint64_t source_hash;
    uint64_t target_hash;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &initial_draw));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_inspect(0U, 0U, &pending_draw));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_zone_count(0U,
        burned.zone_id, &pending));
    FT_ASSERT_EQ(1U, pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.zone_inspect(0U, burned.zone_id,
        0U, &burned_card));
    FT_ASSERT_EQ(pending_draw.instance_id, burned_card.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(overflow_event.type_id, record.intent.type_id);
    FT_ASSERT_EQ(0U, record.intent.fields[0].value);
    FT_ASSERT_EQ(pending_draw.instance_id, record.intent.fields[1].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_command_record(0U,
        &command_record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_rules_hash(&replay_header.rules_hash));
    replay_header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(command_record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(target,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.deck_push_top(0U, card.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.draw_to_hand(0U, &initial_draw));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.replay_into(target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&target_hash));
    FT_ASSERT_EQ(source_hash, target_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_policy_reshuffles_discard_and_preserves_instance)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_zone_definition discard;
    card_game_zone_definition burned;
    card_game_host_intent_definition empty_event;
    card_game_host_intent_definition overflow_event;
    card_game_deck_card discarded_card;
    card_game_deck_card drawn_card;
    uint32_t count;

    card_game_draw_policy_test_setup(rules, card, discard, burned,
        empty_event, overflow_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_configure(engine,
        rules, card, discard, burned, empty_event, overflow_event, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.zone_push_top(0U, discard.zone_id,
        card.card_id, &discarded_card.instance_id));
    discarded_card.card_id = card.card_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_draw_policy_test_submit(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.hand_inspect(0U, 0U, &drawn_card));
    FT_ASSERT_EQ(discarded_card.instance_id, drawn_card.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_zone_count(0U, discard.zone_id, &count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&count));
    FT_ASSERT_EQ(0U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

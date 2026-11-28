#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_choices.hpp"
#include "../../Modules/CardGame/card_game_resources.hpp"
#include "../../Modules/CardGame/card_game_usage_limits.hpp"
#include "../../Modules/CardGame/card_game_zone_store.hpp"
#include "../../Modules/Buffer/buffer_endian.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"

FT_TEST(test_card_game_engine_byte_snapshot_round_trip_and_rejects_corruption)
{
    card_game_engine source;
    card_game_engine restored;
    card_game_engine wrong_rules;
    card_game_rules rules;
    card_game_rules other_rules;
    std::vector<uint8_t> bytes;
    uint64_t source_hash;
    uint64_t restored_hash;
    int32_t error_code;

    rules.max_board_spaces = 3U;
    rules.max_hand_size = 3U;
    rules.starting_health = 20U;
    rules.starting_mana = 4U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.modify_player_health(0U, -5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_snapshot(bytes));
    FT_ASSERT(bytes.size() > 16U);
    const std::vector<uint8_t> saved = bytes;
    cma_set_alloc_limit(1U);
    const int32_t write_failure = source.serialize_snapshot(bytes);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, write_failure);
    FT_ASSERT(bytes == saved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize(rules));
    other_rules = rules;
    other_rules.max_turns += 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wrong_rules.initialize(other_rules));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        wrong_rules.restore_snapshot(bytes.data(), bytes.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wrong_rules.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored.restore_snapshot(bytes.data(), bytes.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_state_hash(&restored_hash));
    FT_ASSERT_EQ(source_hash, restored_hash);

    bytes[bytes.size() / 2U] ^= 1U;
    error_code = restored.restore_snapshot(bytes.data(), bytes.size());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_state_hash(&restored_hash));
    FT_ASSERT_EQ(source_hash, restored_hash);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        restored.restore_snapshot(bytes.data(), bytes.size() - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.destroy());
    return (1);
}

FT_TEST(test_card_game_decoder_rejects_truncated_counts_before_allocation)
{
    card_game_engine engine;
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    std::vector<uint8_t> bytes;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.serialize_snapshot(bytes));
    constexpr std::size_t EVENT_COUNT_OFFSET = 16U + 8U + 5U * 4U;
    FT_ASSERT_EQ(0U, buffer_load_le<uint32_t>(bytes.data() + EVENT_COUNT_OFFSET));
    const std::size_t payload_size = bytes.size() - sizeof(uint64_t);
    buffer_store_le<uint32_t>(bytes.data() + EVENT_COUNT_OFFSET, 256U);
    uint64_t checksum = UINT64_C(14695981039346656037);
    std::size_t index = 0U;
    while (index < payload_size)
    {
        checksum ^= bytes[index];
        checksum *= UINT64_C(1099511628211);
        ++index;
    }
    buffer_store_le<uint64_t>(bytes.data() + payload_size, checksum);
    uint64_t before;
    uint64_t after;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&before));
    cma_set_alloc_limit(1U);
    const int32_t result = engine.restore_snapshot(bytes.data(), bytes.size());
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_state_hash(&after));
    FT_ASSERT_EQ(before, after);
    return (1);
}

static ft_bool card_game_snapshot_test_predicate(uint32_t action_id,
    uint32_t action_tags, uint32_t predicate_context_id,
    void *user_data) noexcept
{
    (void)action_id;
    (void)predicate_context_id;
    (void)user_data;
    if ((action_tags & 1U) == 0U)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_card_game_delta_reuses_owned_snapshot_buffers)
{
    card_game_engine source;
    card_game_engine replica;
    card_game_snapshot baseline;
    test_cma_failure_controller controller{};
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    uint32_t allowance_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.emit_event(501U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.grant_action_allowance(
        0U, 1U, 0U, 1U, 0U, 0U, 0U, 0U, 0U, &allowance_id));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(controller));
    {
        card_game_snapshot captured;
        card_game_delta delta;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&captured));
        const ft_size_t snapshot_allocations = test_cma_failure_controller_attempt_count(
            controller, TEST_CMA_FAILURE_ALLOCATE);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_delta(baseline, &delta));
        const ft_size_t delta_allocations = test_cma_failure_controller_attempt_count(
            controller, TEST_CMA_FAILURE_ALLOCATE) - snapshot_allocations;
        FT_ASSERT_EQ(snapshot_allocations, delta_allocations);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_fail_next(
            controller, TEST_CMA_FAILURE_ALLOCATE));
        const card_game_event *saved_events = delta.events;
        FT_ASSERT_EQ(FT_ERR_NO_MEMORY, source.create_delta(baseline, &delta));
        FT_ASSERT_EQ(saved_events, delta.events);
        FT_ASSERT_EQ(1U, delta.event_count);
        FT_ASSERT_EQ(1U, delta.allowances.count);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.apply_delta(delta));
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_destroy(controller));

    uint64_t source_hash;
    uint64_t replica_hash;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(source_hash, replica_hash);
    return (1);
}

FT_TEST(test_card_game_unchanged_delta_checks_wrapped_zones_without_allocation)
{
    card_game_engine source;
    card_game_snapshot baseline;
    card_game_snapshot result;
    card_game_delta delta;
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};
    const card_game_zone_definition zone{2U, 3U, 1U << CARD_GAME_CREATURE, FT_TRUE};
    uint32_t first_id;
    uint32_t second_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_zone(zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.zone_push_bottom(0U, zone.zone_id, card.card_id, &first_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.zone_push_top(0U, zone.zone_id, card.card_id, &second_id));
    uint64_t original_hash;
    uint64_t capped_hash;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&original_hash));
    cma_set_alloc_limit(1U);
    const int32_t hash_status = source.get_state_hash(&capped_hash);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hash_status);
    FT_ASSERT_EQ(original_hash, capped_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_FALSE, delta.global_state_changed);
    FT_ASSERT_EQ(0U, delta.changed_player_mask);
    FT_ASSERT_EQ(2U, delta.zones.entry_count);
    FT_ASSERT_EQ(second_id, delta.zones.entries[0].instance_id);
    FT_ASSERT_EQ(first_id, delta.zones.entries[1].instance_id);
    delta.target_state_sequence += 1U;

    cma_set_alloc_limit(1U);
    delta.zones.entries[1].instance_id = second_id;
    const int32_t invalid_entry = source.apply_delta(delta);
    delta.zones.entries[1].instance_id = first_id;
    delta.zones.offsets[0U][0U] += 1U;
    const int32_t invalid_offset = source.apply_delta(delta);
    delta.zones.offsets[0U][0U] -= 1U;
    const int32_t valid = source.apply_delta(delta);
    cma_set_alloc_limit(0U);

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_entry);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_offset);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&result));
    FT_ASSERT_EQ(delta.target_state_sequence, result.state_sequence);
    FT_ASSERT(card_game_zone_store::snapshots_equal(baseline.zones, result.zones));
    return (1);
}

FT_TEST(test_card_game_snapshot_restore_checks_deck_ids_in_submission_order)
{
    card_game_engine engine;
    card_game_snapshot incoming;
    card_game_snapshot restored;
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&incoming));
    incoming.players[0U].deck_count = 2U;
    incoming.players[0U].deck[0U] = 1U;
    incoming.players[0U].deck[1U] = 1U;
    incoming.players[0U].deck_instance_ids[0U] = UINT32_MAX;
    incoming.players[0U].deck_instance_ids[1U] = 0U;
    incoming.players[1U].deck_count = 2U;
    incoming.players[1U].deck[0U] = 1U;
    incoming.players[1U].deck[1U] = 99U;
    incoming.players[1U].deck_instance_ids[0U] = 1U;
    incoming.players[1U].deck_instance_ids[1U] = 42U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.apply_snapshot(incoming));
    incoming.players[1U].deck_instance_ids[0U] = 12U;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.apply_snapshot(incoming));
    incoming.players[1U].deck[1U] = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.apply_snapshot(incoming));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&restored));
    FT_ASSERT_EQ(UINT32_MAX, restored.players[0U].deck_instance_ids[0U]);
    FT_ASSERT_EQ(1U, restored.players[0U].deck_instance_ids[1U]);
    FT_ASSERT_EQ(12U, restored.players[1U].deck_instance_ids[0U]);
    FT_ASSERT_EQ(42U, restored.players[1U].deck_instance_ids[1U]);
    return (1);
}

FT_TEST(test_card_game_restore_preserves_unsorted_board_and_modifier_ids)
{
    card_game_engine source;
    card_game_engine replica;
    card_game_snapshot incoming;
    card_game_snapshot baseline;
    card_game_snapshot restored;
    card_game_delta delta;
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_snapshot(&incoming));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.apply_snapshot(baseline));
    card_game_player_snapshot &player = incoming.players[0U];
    player.board_count = 3U;
    player.board[0U] = 0U;
    player.board[1U] = 1U;
    player.board[2U] = 2U;
    player.instances[0U] = {9U, 1U, 0U, 2, 3, 0, FT_TRUE};
    player.instances[1U] = {3U, 1U, 0U, 2, 3, 0, FT_TRUE};
    player.instances[2U] = {7U, 1U, 0U, 2, 3, 0, FT_TRUE};
    incoming.modifier_count = 2U;
    incoming.modifiers[0U] = {8U, 0U, 0U, 7U, UINT32_MAX, 1, 0,
        CARD_GAME_MODIFIER_PERMANENT, 1U, 0U};
    incoming.modifiers[1U] = {2U, 0U, 0U, 0U, 1U, 2, 0,
        CARD_GAME_MODIFIER_PERMANENT, 1U, 0U};
    incoming.state_sequence += 1U;
    player.instances[2U].instance_id = 9U;
    incoming.modifiers[0U].target_instance_id = 9U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source.apply_snapshot(incoming));
    player.instances[2U].instance_id = 7U;
    incoming.modifiers[0U].target_instance_id = 7U;
    incoming.modifiers[1U].modifier_id = 8U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source.apply_snapshot(incoming));
    incoming.modifiers[1U].modifier_id = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.apply_snapshot(incoming));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_delta(baseline, &delta));
    delta.modifiers[1U].modifier_id = 8U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, replica.apply_delta(delta));
    delta.modifiers[1U].modifier_id = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica.get_snapshot(&restored));
    FT_ASSERT_EQ(9U, restored.players[0U].instances[0U].instance_id);
    FT_ASSERT_EQ(3U, restored.players[0U].instances[1U].instance_id);
    FT_ASSERT_EQ(7U, restored.players[0U].instances[2U].instance_id);
    FT_ASSERT_EQ(8U, restored.modifiers[0U].modifier_id);
    FT_ASSERT_EQ(2U, restored.modifiers[1U].modifier_id);
    return (1);
}

FT_TEST(test_card_game_delta_compares_only_live_player_slots)
{
    card_game_engine engine;
    card_game_snapshot baseline;
    card_game_snapshot captured;
    card_game_delta delta;
    const card_game_rules rules{3U, 3U, 20U, 4U, 10U, 20U};
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&baseline));
    card_game_player_snapshot &player = baseline.players[0U];
    player.board[5U] = 5U;
    player.instances[5U] = {91U, 1U, 0U, 2, 3, 0, FT_TRUE};
    player.deck[5U] = 1U;
    player.deck_instance_ids[5U] = 92U;
    player.hand[5U] = 1U;
    player.hand_instance_ids[5U] = 93U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_snapshot(&captured));
    FT_ASSERT_EQ(0U, captured.players[0U].board[5U]);
    FT_ASSERT_EQ(0U, captured.players[0U].instances[5U].instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(0U, delta.changed_player_mask);
    FT_ASSERT_EQ(FT_FALSE, delta.global_state_changed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, 1U, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(1U, delta.changed_player_mask);
    FT_ASSERT_EQ(1U, delta.players[0U].board_count);
    return (1);
}

FT_TEST(test_card_game_allowance_snapshot_failures_preserve_destination)
{
    card_game_allowance_ledger ledger;
    card_game_allowance_snapshot snapshot;
    card_game_allowance_snapshot clone;
    card_game_action_allowance *snapshot_allowances;
    card_game_action_allowance *clone_allowances;
    test_cma_failure_controller controller;
    uint32_t allowance_id;
    uint32_t snapshot_count;
    uint32_t snapshot_next_id;
    int32_t error_code;

    ft_bzero(&snapshot, sizeof(snapshot));
    ft_bzero(&clone, sizeof(clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_predicate(1U,
        card_game_snapshot_test_predicate, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.grant(1U, 2U, 1U, 1U, 0U, 0U, 0U,
        1U, 1U, &allowance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_allowance_ledger::clone_snapshot(snapshot, &clone));
    snapshot_allowances = snapshot.allowances;
    snapshot_count = snapshot.count;
    snapshot_next_id = snapshot.next_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = ledger.get_snapshot(&snapshot);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(snapshot_allowances, snapshot.allowances);
    FT_ASSERT_EQ(snapshot_count, snapshot.count);
    FT_ASSERT_EQ(snapshot_next_id, snapshot.next_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    clone_allowances = clone.allowances;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = card_game_allowance_ledger::clone_snapshot(snapshot, &clone);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(clone_allowances, clone.allowances);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_allowance_ledger::release_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_allowance_ledger::release_snapshot(&clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_choice_snapshot_failures_preserve_destination)
{
    card_game_choice_ledger ledger;
    card_game_choice_snapshot snapshot;
    card_game_choice_snapshot clone;
    card_game_choice *snapshot_choices;
    card_game_choice *clone_choices;
    card_game_choice_option option;
    test_cma_failure_controller controller;
    uint32_t choice_id;
    uint32_t snapshot_count;
    uint32_t snapshot_next_id;
    int32_t error_code;

    ft_bzero(&snapshot, sizeof(snapshot));
    ft_bzero(&clone, sizeof(clone));
    option.option_id = 1U;
    option.value_a = 2U;
    option.value_b = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.open(1U, CARD_GAME_CHOICE_TARGET,
        10U, 1U, &choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::clone_snapshot(snapshot, &clone));
    snapshot_choices = snapshot.choices;
    snapshot_count = snapshot.count;
    snapshot_next_id = snapshot.next_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = ledger.get_snapshot(&snapshot);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(snapshot_choices, snapshot.choices);
    FT_ASSERT_EQ(snapshot_count, snapshot.count);
    FT_ASSERT_EQ(snapshot_next_id, snapshot.next_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    clone_choices = clone.choices;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = card_game_choice_ledger::clone_snapshot(snapshot, &clone);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(clone_choices, clone.choices);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::release_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_choice_ledger::release_snapshot(&clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_usage_limit_snapshot_failures_preserve_destination)
{
    card_game_usage_limit_ledger ledger;
    card_game_usage_limit_snapshot snapshot;
    card_game_usage_limit_snapshot clone;
    card_game_usage_limit *snapshot_limits;
    card_game_usage_limit *clone_limits;
    test_cma_failure_controller controller;
    uint32_t limit_id;
    uint32_t snapshot_count;
    uint32_t snapshot_next_id;
    uint32_t snapshot_capacity;
    int32_t error_code;

    ft_bzero(&snapshot, sizeof(snapshot));
    ft_bzero(&clone, sizeof(clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_limit(1U, 1U,
        CARD_GAME_USAGE_MATCH, 0U, 2U, CARD_GAME_USAGE_ON_ACTIVATION,
        0U, &limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::clone_snapshot(snapshot, &clone));
    snapshot_limits = snapshot.limits;
    snapshot_count = snapshot.count;
    snapshot_next_id = snapshot.next_id;
    snapshot_capacity = snapshot.capacity;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = ledger.get_snapshot(&snapshot);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(snapshot_limits, snapshot.limits);
    FT_ASSERT_EQ(snapshot_count, snapshot.count);
    FT_ASSERT_EQ(snapshot_next_id, snapshot.next_id);
    FT_ASSERT_EQ(snapshot_capacity, snapshot.capacity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    clone_limits = clone.limits;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = card_game_usage_limit_ledger::clone_snapshot(snapshot, &clone);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(clone_limits, clone.limits);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::release_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::release_snapshot(&clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_zone_snapshot_failures_preserve_destination)
{
    card_game_zone_store store;
    card_game_zone_store_snapshot snapshot;
    card_game_zone_store_snapshot clone;
    card_game_zone_store_definition definition;
    card_game_zone_entry entry;
    card_game_zone_entry *snapshot_entries;
    card_game_zone_entry *clone_entries;
    test_cma_failure_controller controller;
    int32_t error_code;

    ft_bzero(&snapshot, sizeof(snapshot));
    ft_bzero(&clone, sizeof(clone));
    definition.zone_id = 1U;
    definition.capacity = 4U;
    definition.allowed_card_type_mask = UINT32_MAX;
    definition.owner_scoped = FT_TRUE;
    entry.card_id = 10U;
    entry.instance_id = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_zone(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_bottom(0U, 1U, entry, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.get_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_zone_store::clone_snapshot(snapshot, &clone));
    snapshot_entries = snapshot.entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = store.get_snapshot(&snapshot);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(snapshot_entries, snapshot.entries);
    FT_ASSERT_EQ(1U, snapshot.entry_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    clone_entries = clone.entries;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    error_code = card_game_zone_store::clone_snapshot(snapshot, &clone);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(clone_entries, clone.entries);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_zone_store::release_snapshot(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_zone_store::release_snapshot(&clone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_snapshot_failure_preserves_destination)
{
    card_game_engine source;
    card_game_engine destination;
    card_game_rules rules;
    card_game_snapshot snapshot;
    test_cma_failure_controller controller;
    card_game_event *old_events;
    card_game_zone_entry *old_zone_entries;
    card_game_resource_pool *old_pools;
    card_game_resource_unit *old_units;
    card_game_action_allowance *old_allowances;
    card_game_choice *old_choices;
    card_game_usage_limit *old_limits;
    uint32_t old_event_count;
    uint64_t old_state_sequence;
    int32_t error_code;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.emit_event(1U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_snapshot(&snapshot));
    old_events = snapshot.events;
    old_zone_entries = snapshot.zones.entries;
    old_pools = snapshot.resources.pools;
    old_units = snapshot.resources.units;
    old_allowances = snapshot.allowances.allowances;
    old_choices = snapshot.choices.choices;
    old_limits = snapshot.usage_limits.limits;
    old_event_count = snapshot.event_count;
    old_state_sequence = snapshot.state_sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_on_call(controller,
            TEST_CMA_FAILURE_ALLOCATE, 2U));
    error_code = source.get_snapshot(&snapshot);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(old_events, snapshot.events);
    FT_ASSERT_EQ(old_zone_entries, snapshot.zones.entries);
    FT_ASSERT_EQ(old_pools, snapshot.resources.pools);
    FT_ASSERT_EQ(old_units, snapshot.resources.units);
    FT_ASSERT_EQ(old_allowances, snapshot.allowances.allowances);
    FT_ASSERT_EQ(old_choices, snapshot.choices.choices);
    FT_ASSERT_EQ(old_limits, snapshot.usage_limits.limits);
    FT_ASSERT_EQ(old_event_count, snapshot.event_count);
    FT_ASSERT_EQ(old_state_sequence, snapshot.state_sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

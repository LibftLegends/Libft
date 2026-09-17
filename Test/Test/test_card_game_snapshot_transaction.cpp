#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_choices.hpp"
#include "../../Modules/CardGame/card_game_resources.hpp"
#include "../../Modules/CardGame/card_game_usage_limits.hpp"
#include "../../Modules/CardGame/card_game_zone_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"

static ft_bool card_game_snapshot_test_predicate(uint32_t action_id,
    uint32_t action_tags, void *user_data) noexcept
{
    (void)action_id;
    (void)user_data;
    if ((action_tags & 1U) == 0U)
        return (FT_FALSE);
    return (FT_TRUE);
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

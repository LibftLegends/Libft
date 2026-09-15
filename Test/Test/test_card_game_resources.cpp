#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_resources.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_bool card_game_resource_test_creature_only(uint32_t action_id,
    uint32_t action_tags, void *user_data) noexcept
{
    (void)action_id;
    (void)user_data;
    if ((action_tags & 1U) == 0U)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_card_game_resources_support_typed_conditional_payment)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    card_game_resource_requirement requirement;
    card_game_payment_plan plan;
    uint32_t pool_id;
    uint32_t first_unit;
    uint32_t second_unit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 2U, 1U, 0U,
        FT_FALSE, &first_unit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 3U, 2U, 0U,
        FT_FALSE, &second_unit));
    (void)first_unit;
    (void)second_unit;
    requirement.resource_type_id = 1U;
    requirement.amount = 2U;
    requirement.required_tags = 1U;
    requirement.forbidden_tags = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_payment_plan(1U, requirement,
        &plan));
    FT_ASSERT_EQ(2U, plan.total_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.spend(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(3U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_expire_temporary_units)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    card_game_resource_requirement requirement;
    card_game_payment_plan plan;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 4U, 0U, 3U,
        FT_TRUE, &unit_id));
    (void)unit_id;
    requirement.resource_type_id = 1U;
    requirement.amount = 4U;
    requirement.required_tags = 0U;
    requirement.forbidden_tags = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_payment_plan(1U, requirement,
        &plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.refresh(3U));
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.create_payment_plan(1U, requirement,
        &plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(0U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_allowances_support_conditional_additional_summons)
{
    card_game_allowance_ledger ledger;
    card_game_action_allowance allowance;
    uint32_t base_allowance;
    uint32_t conditional_allowance;
    uint32_t consumed_id;
    uint32_t count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_predicate(7U,
        card_game_resource_test_creature_only, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.grant(1U, 1U, 2U, 1U, 0U, 0U, 0U,
        0U, 0U, &base_allowance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.grant(1U, 1U, 1U, 1U, 0U, 42U, 7U,
        7U, 0U,
        &conditional_allowance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.count_eligible(1U, 1U, 1U, 4U,
        &count));
    FT_ASSERT_EQ(1U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume_first(1U, 1U, 1U, 4U,
        &consumed_id));
    FT_ASSERT_EQ(conditional_allowance, consumed_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(conditional_allowance, &allowance));
    FT_ASSERT_EQ(0U, allowance.remaining_uses);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(base_allowance, 1U, 1U, 2U,
        4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.reset_epoch(5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(base_allowance, &allowance));
    FT_ASSERT_EQ(1U, allowance.remaining_uses);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_locking_is_partial_and_excludes_locked_units)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    card_game_resource_requirement requirement;
    card_game_payment_plan plan;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 5U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.lock_units(1U, 1U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(5U, pool.current_amount);
    FT_ASSERT_EQ(2U, pool.locked_amount);
    requirement.resource_type_id = 1U;
    requirement.amount = 4U;
    requirement.required_tags = 0U;
    requirement.forbidden_tags = 0U;
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.create_payment_plan(1U, requirement,
        &plan));
    requirement.amount = 3U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_payment_plan(1U, requirement,
        &plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.spend(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(2U, pool.current_amount);
    FT_ASSERT_EQ(2U, pool.locked_amount);
    (void)unit_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_rejects_duplicate_payment_underflow)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    card_game_payment_plan plan;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 20U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 10U, 0U, 0U,
        FT_FALSE, &unit_id));
    plan.count = 2U;
    plan.total_amount = 14U;
    plan.units[0].unit_id = unit_id;
    plan.units[0].amount = 7U;
    plan.units[1].unit_id = unit_id;
    plan.units[1].amount = 7U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ledger.spend(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(10U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_scheduled_lock_unlocks_without_expiry)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 5U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.lock_units_until(1U, 1U, 2U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(2U, pool.locked_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.refresh(3U));
    FT_ASSERT_EQ(2U, pool.locked_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.refresh(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(0U, pool.locked_amount);
    (void)unit_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_rejected_add_is_transactional)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 2U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 2U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.add_units(1U, 1U, 1U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(2U, pool.current_amount);
    FT_ASSERT_EQ(1U, ledger.unit_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_snapshot_restores_units_and_pools)
{
    card_game_resource_ledger ledger;
    card_game_resource_snapshot saved;
    card_game_resource_snapshot copy;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;
    card_game_resource_requirement requirement;
    card_game_payment_plan plan;

    ft_bzero(&saved, sizeof(saved));
    ft_bzero(&copy, sizeof(copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(3U, 9U, 10U,
        &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(3U, 9U, 5U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_resource_ledger::clone_snapshot(saved, &copy));
    FT_ASSERT(card_game_resource_ledger::snapshots_equal(saved, copy));
    requirement.resource_type_id = 9U;
    requirement.amount = 2U;
    requirement.required_tags = 0U;
    requirement.forbidden_tags = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_payment_plan(3U,
        requirement, &plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.spend(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(3U, 9U, &pool));
    FT_ASSERT_EQ(3U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.apply_snapshot(saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(3U, 9U, &pool));
    FT_ASSERT_EQ(5U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_resource_ledger::release_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_resource_ledger::release_snapshot(&copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    (void)unit_id;
    return (1);
}

FT_TEST(test_card_game_engine_owns_resource_and_allowance_ledgers)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;
    uint32_t allowance_id;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(1U, 1U, 5U,
        &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(1U, 1U, 3U, 0U,
        0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_resource_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(3U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.grant_action_allowance(1U, 1U, 0U,
        1U, 0U, 0U, 0U, 0U, 0U, &allowance_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.consume_action_allowance(1U, 1U, 0U,
        1U, &allowance_id));
    (void)unit_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_support_multi_component_and_alternative_costs)
{
    card_game_resource_ledger ledger;
    card_game_resource_pool pool;
    card_game_cost cost;
    card_game_cost_plan plan;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 2U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 3U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 2U, 2U, 0U, 0U,
        FT_FALSE, &unit_id));
    cost.component_count = 2U;
    cost.components[0].resource_type_id = 1U;
    cost.components[0].amount = 2U;
    cost.components[0].required_tags = 0U;
    cost.components[0].forbidden_tags = 0U;
    cost.components[1].resource_type_id = 2U;
    cost.components[1].amount = 1U;
    cost.components[1].required_tags = 0U;
    cost.components[1].forbidden_tags = 0U;
    cost.alternative_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_cost_plan(1U, cost, 0U,
        &plan));
    FT_ASSERT_EQ(3U, plan.combined.total_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.spend_cost(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(1U, pool.current_amount);
    cost.component_count = 0U;
    cost.alternative_count = 1U;
    cost.alternative_component_counts[0] = 1U;
    cost.alternatives[0][0].resource_type_id = 1U;
    cost.alternatives[0][0].amount = 1U;
    cost.alternatives[0][0].required_tags = 0U;
    cost.alternatives[0][0].forbidden_tags = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_cost_plan(1U, cost, 0U,
        &plan));
    FT_ASSERT_EQ(0U, plan.selected_alternative);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_exposes_serializable_choices)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_choice choice;
    card_game_choice_option option;
    uint32_t choice_id;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.open_choice(1U,
        CARD_GAME_CHOICE_MODE, 20U, 0U, &choice_id));
    option.option_id = 3U;
    option.value_a = 11U;
    option.value_b = 12U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_choice_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.choose_option(choice_id, 1U, 3U,
        1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_choice(choice_id, &choice));
    FT_ASSERT(choice.resolved != FT_FALSE);
    FT_ASSERT_EQ(3U, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_cost_components_share_units_transactionally)
{
    card_game_resource_ledger ledger;
    card_game_cost cost;
    card_game_cost_plan plan;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 5U, 0U, 0U,
        FT_FALSE, &unit_id));
    cost.component_count = 2U;
    cost.components[0].resource_type_id = 1U;
    cost.components[0].amount = 3U;
    cost.components[0].required_tags = 0U;
    cost.components[0].forbidden_tags = 0U;
    cost.components[1].resource_type_id = 1U;
    cost.components[1].amount = 2U;
    cost.components[1].required_tags = 0U;
    cost.components[1].forbidden_tags = 0U;
    cost.alternative_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.create_cost_plan(1U, cost, 0U,
        &plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.spend_cost(plan));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_pool(1U, 1U, &pool));
    FT_ASSERT_EQ(0U, pool.current_amount);
    (void)unit_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_resources_conflicting_lock_schedule_is_atomic)
{
    card_game_resource_ledger ledger;
    card_game_resource_unit unit;
    uint32_t pool_id;
    uint32_t unit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_pool(1U, 1U, 10U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.add_units(1U, 1U, 5U, 0U, 0U,
        FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.lock_units_until(1U, 1U, 2U, 4U));
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.lock_units_until(1U, 1U, 1U, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_unit(unit_id, &unit));
    FT_ASSERT_EQ(2U, unit.locked_amount);
    FT_ASSERT_EQ(4U, unit.unlock_epoch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

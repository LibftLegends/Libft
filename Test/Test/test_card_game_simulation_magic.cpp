#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void magic_simulation_card(card_game_card_definition &card,
    uint32_t card_id, int32_t attack, int32_t health) noexcept
{
    card.card_id = card_id;
    card.type = CARD_GAME_CREATURE;
    card.cost = 0U;
    card.attack = attack;
    card.health = health;
    card.effect_id = CARD_GAME_NO_EFFECT;
    return ;
}

static int32_t magic_simulation_pay_color(card_game_engine &engine,
    uint32_t player_id, uint32_t resource_type_id) noexcept
{
    card_game_cost cost;
    card_game_cost_plan plan;

    ft_bzero(&cost, sizeof(cost));
    cost.component_count = 1U;
    cost.components[0].resource_type_id = resource_type_id;
    cost.components[0].amount = 1U;
    if (engine.create_resource_cost_plan(player_id, cost, 0U,
            &plan) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (engine.spend_resource_cost(plan));
}

struct magic_land_context
{
    uint32_t player_id;
    uint32_t resource_type_id;
    uint32_t event_type;
};

struct magic_trigger_context
{
    uint32_t player_id;
    uint32_t next_event;
};

static int32_t magic_simulation_land_effect(card_game_engine &engine,
    uint32_t source_instance, uint32_t target_instance, void *context) noexcept
{
    magic_land_context *land;
    uint32_t unit_id;

    (void)source_instance;
    (void)target_instance;
    if (context == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    land = static_cast<magic_land_context *>(context);
    if (engine.add_resource_units(land->player_id,
            land->resource_type_id, 1U, 0U, 0U, FT_FALSE,
            &unit_id) != FT_ERR_SUCCESS)
        return (FT_ERR_FULL);
    return (engine.emit_event(land->event_type, source_instance, 0U));
}

static int32_t magic_simulation_trigger(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;
    magic_trigger_context *trigger;

    (void)engine;
    (void)context;
    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    trigger = static_cast<magic_trigger_context *>(user_data);
    ft_bzero(&operation, sizeof(operation));
    operation.type = CARD_GAME_OPERATION_HEALTH;
    operation.player_id = trigger->player_id;
    operation.amount = 1;
    if (operations.append(operation) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (trigger->next_event == 0U)
        return (FT_ERR_SUCCESS);
    operation.type = CARD_GAME_OPERATION_EMIT_EVENT;
    operation.event_type = trigger->next_event;
    return (operations.append(operation));
}

static int32_t magic_simulation_pay_either(card_game_engine &engine,
    uint32_t player_id, uint32_t first_color, uint32_t second_color) noexcept
{
    card_game_cost cost;
    card_game_cost_plan plan;

    ft_bzero(&cost, sizeof(cost));
    cost.alternative_count = 2U;
    cost.alternative_component_counts[0] = 1U;
    cost.alternatives[0][0].resource_type_id = first_color;
    cost.alternatives[0][0].amount = 1U;
    cost.alternative_component_counts[1] = 1U;
    cost.alternatives[1][0].resource_type_id = second_color;
    cost.alternatives[1][0].amount = 1U;
    if (engine.create_resource_cost_plan(player_id, cost, 0U,
            &plan) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (engine.spend_resource_cost(plan));
}

FT_TEST(test_card_game_simulation_magic_two_player_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition alpha;
    card_game_card_definition beta;
    card_game_card_definition gamma;
    card_game_deck_card p0_alpha;
    card_game_deck_card p0_gamma;
    card_game_deck_card p1_beta;
    card_game_resource_pool pool;
    uint32_t pool_id;
    uint32_t unit_id;
    uint32_t modifier_id;
    uint32_t board_count;
    uint32_t life;

    rules.max_board_spaces = 7U;
    rules.max_hand_size = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    magic_simulation_card(alpha, 4301U, 2, 2);
    magic_simulation_card(beta, 4302U, 2, 2);
    magic_simulation_card(gamma, 4303U, 3, 3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(gamma));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 1U,
        4U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 2U,
        4U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(1U, 1U,
        4U, &pool_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(0U, 1U, 1U,
        0U, 0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(0U, 2U, 1U,
        0U, 0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_resource_units(1U, 1U, 1U,
        0U, 0U, FT_FALSE, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, gamma.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, alpha.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, beta.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &p0_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &p0_gamma));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &p1_beta));
    FT_ASSERT_EQ(alpha.card_id, p0_alpha.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, magic_simulation_pay_color(engine, 0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        p0_alpha.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, magic_simulation_pay_color(engine, 1U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(1U,
        p1_beta.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, magic_simulation_pay_color(engine, 0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        p0_gamma.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_card_modifier(0U, 0U, 1, 0,
        CARD_GAME_MODIFIER_UNTIL_END_TURN, 1U, &modifier_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 0U, 1U, 0U,
        CARD_GAME_COMBAT_SIMULTANEOUS));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &life));
    FT_ASSERT_EQ(20U, life);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_resource_pool(0U, 1U, &pool));
    FT_ASSERT_EQ(0U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_simulation_magic_multicolored_land_and_trigger_chain)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition land;
    card_game_card_definition alpha;
    card_game_card_definition beta;
    magic_land_context p0_land_color_a;
    magic_land_context p0_land_color_b;
    magic_land_context p1_land_color_b;
    magic_trigger_context p0_trigger;
    magic_trigger_context p0_followup;
    magic_trigger_context p1_trigger;
    card_game_resource_pool pool;
    uint32_t p0_land_effect;
    uint32_t p1_land_effect;
    uint32_t trigger_a;
    uint32_t trigger_b;
    uint32_t board_count;
    uint32_t life;
    uint32_t unit_id;
    uint32_t modifier_id;

    rules.max_board_spaces = 7U;
    rules.max_hand_size = 7U;
    rules.starting_health = 20U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    land.card_id = 4401U;
    land.type = CARD_GAME_ARTIFACT;
    land.cost = 0U;
    land.attack = 0;
    land.health = 1;
    land.effect_id = CARD_GAME_NO_EFFECT;
    alpha.card_id = 4402U;
    alpha.type = CARD_GAME_CREATURE;
    alpha.cost = 0U;
    alpha.attack = 2;
    alpha.health = 2;
    alpha.effect_id = CARD_GAME_NO_EFFECT;
    beta.card_id = 4403U;
    beta.type = CARD_GAME_CREATURE;
    beta.cost = 0U;
    beta.attack = 2;
    beta.health = 2;
    beta.effect_id = CARD_GAME_NO_EFFECT;
    p0_land_color_a.player_id = 0U;
    p0_land_color_a.resource_type_id = 1U;
    p0_land_color_a.event_type = 4410U;
    p0_land_color_b.player_id = 0U;
    p0_land_color_b.resource_type_id = 2U;
    p0_land_color_b.event_type = 4410U;
    p1_land_color_b.player_id = 1U;
    p1_land_color_b.resource_type_id = 2U;
    p1_land_color_b.event_type = 4411U;
    p0_trigger.player_id = 0U;
    p0_trigger.next_event = 4412U;
    p0_followup.player_id = 0U;
    p0_followup.next_event = 0U;
    p1_trigger.player_id = 1U;
    p1_trigger.next_event = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 1U,
        4U, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(0U, 2U,
        4U, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(1U, 1U,
        4U, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_resource_pool(1U, 2U,
        4U, &unit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect(
        magic_simulation_land_effect, &p0_land_effect));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect(
        magic_simulation_land_effect, &p1_land_effect));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        magic_simulation_trigger, &p0_trigger, 4410U, &trigger_a));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        magic_simulation_trigger, &p0_followup, 4412U, &trigger_a));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        magic_simulation_trigger, &p1_trigger, 4411U, &trigger_b));
    p0_land_color_a.event_type = 4410U;
    p0_land_color_b.event_type = 4410U;
    p1_land_color_b.event_type = 4411U;
    land.effect_id = p0_land_effect;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(land));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, land.card_id, 0U,
        &p0_land_color_a));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &life));
    FT_ASSERT_EQ(22U, life);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    land.effect_id = p1_land_effect;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, land.card_id, 0U,
        &p1_land_color_b));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, magic_simulation_pay_color(engine, 1U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(1U, beta.card_id, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    land.effect_id = p0_land_effect;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, land.card_id, 0U,
        &p0_land_color_b));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, magic_simulation_pay_either(engine, 0U, 2U,
        1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, alpha.card_id, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.add_card_modifier(0U, 2U, 1, 1,
        CARD_GAME_MODIFIER_PERMANENT, 4404U, &modifier_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 2U, 1U, 1U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_resource_pool(0U, 2U, &pool));
    FT_ASSERT_EQ(0U, pool.current_amount);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &life));
    FT_ASSERT_EQ(21U, life);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void yugioh_simulation_card(card_game_card_definition &card,
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

FT_TEST(test_card_game_simulation_yugioh_two_player_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition normal_alpha;
    card_game_card_definition normal_beta;
    card_game_card_definition extra_alpha;
    card_game_deck_card p0_normal;
    card_game_deck_card p0_extra;
    card_game_deck_card p1_normal;
    uint32_t summon_limit;
    uint32_t board_count;
    uint32_t life_points;

    rules.max_board_spaces = 5U;
    rules.max_hand_size = 6U;
    rules.starting_health = 8000U;
    rules.starting_mana = 0U;
    rules.max_mana = 1U;
    rules.max_turns = 20U;
    yugioh_simulation_card(normal_alpha, 4201U, 1500, 1200);
    yugioh_simulation_card(normal_beta, 4202U, 1600, 1000);
    yugioh_simulation_card(extra_alpha, 4203U, 2300, 1800);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(normal_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(normal_beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(extra_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_usage_limit(9201U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_RESOLUTION,
        0U, &summon_limit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U,
        extra_alpha.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U,
        normal_alpha.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U,
        normal_beta.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &p0_normal));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &p0_extra));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &p1_normal));
    FT_ASSERT_EQ(normal_alpha.card_id, p0_normal.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.consume_usage_limit(summon_limit,
        1U, 1U));
    FT_ASSERT_EQ(FT_ERR_FULL, engine.consume_usage_limit(summon_limit,
        1U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        p0_normal.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(1U,
        p1_normal.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(1U, 0U, 0U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.reset_usage_limits(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.consume_usage_limit(summon_limit,
        1U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        p0_extra.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 0U, 1U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(1U, -2300));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &life_points));
    FT_ASSERT_EQ(5700U, life_points);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(0U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

struct yugioh_chain_context
{
    uint32_t player_id;
    uint32_t next_event;
    int32_t health_amount;
};

static int32_t yugioh_simulation_chain(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;
    yugioh_chain_context *chain;

    (void)engine;
    (void)context;
    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    chain = static_cast<yugioh_chain_context *>(user_data);
    ft_bzero(&operation, sizeof(operation));
    operation.type = CARD_GAME_OPERATION_HEALTH;
    operation.player_id = chain->player_id;
    operation.amount = chain->health_amount;
    if (operations.append(operation) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (chain->next_event == 0U)
        return (FT_ERR_SUCCESS);
    operation.type = CARD_GAME_OPERATION_EMIT_EVENT;
    operation.event_type = chain->next_event;
    return (operations.append(operation));
}

FT_TEST(test_card_game_simulation_yugioh_once_per_turn_and_handtrap_chain)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_match_start_config config;
    yugioh_chain_context once_per_turn;
    yugioh_chain_context once_followup;
    yugioh_chain_context handtrap;
    yugioh_chain_context handtrap_followup;
    uint32_t once_limit_id;
    uint32_t handtrap_limit_id;
    uint32_t effect_id;
    uint32_t health;

    rules.max_board_spaces = 5U;
    rules.max_hand_size = 6U;
    rules.starting_health = 8000U;
    rules.starting_mana = 0U;
    rules.max_mana = 1U;
    rules.max_turns = 20U;
    config.opening_hand_size = 0U;
    config.starting_health = 8000U;
    config.starting_mana = 0U;
    config.first_player = 0U;
    config.random_first_player = FT_FALSE;
    config.deal_opening_hand = FT_FALSE;
    once_per_turn.player_id = 1U;
    once_per_turn.next_event = 5202U;
    once_per_turn.health_amount = -400;
    once_followup.player_id = 0U;
    once_followup.next_event = 0U;
    once_followup.health_amount = -200;
    handtrap.player_id = 0U;
    handtrap.next_event = 5204U;
    handtrap.health_amount = -700;
    handtrap_followup.player_id = 1U;
    handtrap_followup.next_event = 0U;
    handtrap_followup.health_amount = -300;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_usage_limit(5201U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_RESOLUTION,
        0U, &once_limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_usage_limit(5203U, 2U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_RESOLUTION,
        0U, &handtrap_limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback_with_usage_limit(
        yugioh_simulation_chain, &once_per_turn, 5201U, 0U,
        once_limit_id, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        yugioh_simulation_chain, &once_followup, 5202U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback_with_usage_limit(
        yugioh_simulation_chain, &handtrap, 5203U, 0U,
        handtrap_limit_id, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        yugioh_simulation_chain, &handtrap_followup, 5204U, &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U, config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5201U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5203U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(7100U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(7300U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5201U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_FULL, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.reset_usage_limits(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(0U, &health));
    FT_ASSERT_EQ(6900U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(6900U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

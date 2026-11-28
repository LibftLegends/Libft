#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t hearthstone_simulation_heal(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;
    uint32_t player_id;

    (void)engine;
    (void)context;
    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    player_id = *static_cast<uint32_t *>(user_data);
    operation.type = CARD_GAME_OPERATION_HEALTH;
    operation.player_id = player_id;
    operation.amount = 3;
    operation.event_type = 0U;
    operation.source_instance = 0U;
    operation.target_instance = 0U;
    return (operations.append(operation));
}

static void hearthstone_simulation_card(
    card_game_card_definition &card, uint32_t card_id, uint32_t cost,
    int32_t attack, int32_t health, uint32_t effect_id) noexcept
{
    card.card_id = card_id;
    card.type = CARD_GAME_CREATURE;
    card.cost = cost;
    card.attack = attack;
    card.health = health;
    card.effect_id = effect_id;
    return ;
}

FT_TEST(test_card_game_simulation_hearthstone_two_player_match)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition alpha;
    card_game_card_definition beta;
    card_game_card_definition healer;
    card_game_card_definition finisher;
    card_game_deck_card drawn_alpha;
    card_game_deck_card drawn_healer;
    card_game_deck_card drawn_finisher;
    card_game_deck_card drawn_beta;
    card_game_deck_card opponent_healer;
    card_game_deck_card opponent_finisher;
    uint32_t p0;
    uint32_t p1;
    uint32_t effect_p0;
    uint32_t effect_p1;
    uint32_t board_count;
    uint32_t health;

    rules.max_board_spaces = 7U;
    rules.max_hand_size = 10U;
    rules.starting_health = 30U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 30U;
    p0 = 0U;
    p1 = 1U;
    hearthstone_simulation_card(alpha, 4101U, 1U, 2, 2,
        CARD_GAME_NO_EFFECT);
    hearthstone_simulation_card(beta, 4102U, 2U, 3, 3,
        CARD_GAME_NO_EFFECT);
    hearthstone_simulation_card(healer, 4103U, 3U, 2, 3,
        CARD_GAME_NO_EFFECT);
    hearthstone_simulation_card(finisher, 4104U, 4U, 4, 4,
        CARD_GAME_NO_EFFECT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        hearthstone_simulation_heal, &p0, 4103U, &effect_p0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        hearthstone_simulation_heal, &p1, 4104U, &effect_p1));
    healer.effect_id = effect_p0;
    finisher.effect_id = effect_p1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(healer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(finisher));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, finisher.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, healer.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, alpha.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, finisher.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, healer.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(1U, beta.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &drawn_alpha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &drawn_healer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &drawn_finisher));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &drawn_beta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &opponent_healer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(1U, &opponent_finisher));
    FT_ASSERT_EQ(alpha.card_id, drawn_alpha.card_id);
    FT_ASSERT_EQ(beta.card_id, drawn_beta.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        drawn_alpha.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.modify_player_health(1U, -8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(1U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(1U,
        drawn_beta.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        drawn_healer.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 0U, 1U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(1U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(1U,
        opponent_healer.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card_from_hand(0U,
        drawn_finisher.instance_id, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_combat(0U, 1U, 1U, 0U,
        CARD_GAME_COMBAT_ORDERED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(1U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(25U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

struct hearthstone_chain_context
{
    uint32_t player_id;
    uint32_t next_event;
    int32_t health_amount;
};

static int32_t hearthstone_simulation_chain(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;
    hearthstone_chain_context *chain;

    (void)engine;
    (void)context;
    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    chain = static_cast<hearthstone_chain_context *>(user_data);
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

static int32_t hearthstone_simulation_passive(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_operation operation;

    (void)engine;
    (void)context;
    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bzero(&operation, sizeof(operation));
    operation.type = CARD_GAME_OPERATION_MODIFY_INSTANCE_STATS;
    operation.player_id = *static_cast<uint32_t *>(user_data);
    operation.target_instance = 0U;
    operation.attack_delta = 1;
    operation.health_delta = 1;
    operation.duration = CARD_GAME_MODIFIER_PERMANENT;
    operation.source_effect_id = 5100U;
    return (operations.append(operation));
}

FT_TEST(test_card_game_simulation_hearthstone_effect_chain_passive_and_hero_power)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition card;
    card_game_match_start_config config;
    hearthstone_chain_context first_chain;
    hearthstone_chain_context second_chain;
    uint32_t passive_player;
    uint32_t passive_effect_id;
    uint32_t hero_effect_id;
    uint32_t hero_limit_id;
    uint32_t board_count;
    uint32_t health;
    int32_t attack;
    int32_t effective_health;

    rules.max_board_spaces = 7U;
    rules.max_hand_size = 10U;
    rules.starting_health = 30U;
    rules.starting_mana = 0U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    card.card_id = 5101U;
    card.type = CARD_GAME_CREATURE;
    card.cost = 0U;
    card.attack = 2;
    card.health = 2;
    card.effect_id = CARD_GAME_NO_EFFECT;
    passive_player = 0U;
    first_chain.player_id = 1U;
    first_chain.next_event = 5103U;
    first_chain.health_amount = -2;
    second_chain.player_id = 1U;
    second_chain.next_event = 0U;
    second_chain.health_amount = 4;
    config.opening_hand_size = 0U;
    config.starting_health = 30U;
    config.starting_mana = 0U;
    config.first_player = 0U;
    config.random_first_player = FT_FALSE;
    config.deal_opening_hand = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        hearthstone_simulation_passive, &passive_player, 5100U,
        &passive_effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        hearthstone_simulation_chain, &first_chain, 5102U,
        &hero_effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        hearthstone_simulation_chain, &second_chain, 5103U,
        &hero_limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_usage_limit(5104U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_RESOLUTION,
        0U, &hero_limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback_with_usage_limit(
        hearthstone_simulation_chain, &second_chain, 5104U, 0U,
        hero_limit_id, &hero_effect_id));
    card.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U, config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_player_mana(0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.play_card(0U, card.card_id, 0U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_board_count(0U, &board_count));
    FT_ASSERT_EQ(1U, board_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5100U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_effective_instance_stats(0U, 0U,
        &attack, &effective_health));
    FT_ASSERT_EQ(3, attack);
    FT_ASSERT_EQ(3, effective_health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5102U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(32U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5104U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(36U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(5104U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_FULL, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.reset_usage_limits(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.end_turn());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_player_health(1U, &health));
    FT_ASSERT_EQ(40U, health);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

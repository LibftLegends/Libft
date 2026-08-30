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

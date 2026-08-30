#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void card_game_deck_test_rules(card_game_rules &rules) noexcept
{
    rules.max_board_spaces = 4U;
    rules.max_hand_size = 8U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 20U;
    return ;
}

static int32_t card_game_register_deck_test_cards(
    card_game_engine &engine) noexcept
{
    card_game_card_definition definition;
    uint32_t card_id;

    card_id = 1U;
    while (card_id <= 4U)
    {
        definition.card_id = card_id;
        definition.type = CARD_GAME_CREATURE;
        definition.cost = 1U;
        definition.attack = static_cast<int32_t>(card_id);
        definition.health = 2;
        definition.effect_id = CARD_GAME_NO_EFFECT;
        if (engine.register_card(definition) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        card_id += 1U;
    }
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_card_game_engine_deck_operations_preserve_order)
{
    card_game_engine engine;
    card_game_rules rules;
    uint32_t count;
    uint32_t card_id;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_register_deck_test_cards(engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_bottom(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_insert_at(0U, 1U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &count));
    FT_ASSERT_EQ(4U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_draw_top(0U, &card_id));
    FT_ASSERT_EQ(3U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_peek_bottom(0U, &card_id));
    FT_ASSERT_EQ(2U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_remove(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.deck_remove(0U, 99U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.deck_push_top(0U, 99U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_deck_shuffle_is_deterministic)
{
    card_game_engine first_engine;
    card_game_engine second_engine;
    card_game_rules rules;
    uint64_t first_hash;
    uint64_t second_hash;
    uint64_t first_seed;
    uint64_t second_seed;
    uint32_t card_id;
    uint32_t card_index;

    card_game_deck_test_rules(rules);
    first_seed = 1234567U;
    second_seed = 1234567U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(first_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(second_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.start_match(1U));
    card_index = 1U;
    while (card_index <= 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            first_engine.deck_push_bottom(0U, card_index));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            second_engine.deck_push_bottom(0U, card_index));
        card_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.shuffle_deck(0U, &first_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.shuffle_deck(0U, &second_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.get_state_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.get_state_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_NEQ(0U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_engine_deck_delta_preserves_order)
{
    card_game_engine source_engine;
    card_game_engine destination_engine;
    card_game_rules rules;
    card_game_snapshot baseline;
    card_game_delta delta;
    uint32_t card_id;

    card_game_deck_test_rules(rules);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_register_deck_test_cards(destination_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_bottom(0U, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_bottom(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.deck_push_top(0U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(baseline, &delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.apply_delta(delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.deck_peek_top(0U, &card_id));
    FT_ASSERT_EQ(4U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.deck_peek_bottom(
        0U, &card_id));
    FT_ASSERT_EQ(1U, card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_engine.destroy());
    return (1);
}

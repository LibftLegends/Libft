#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/CardGame/card_game_script_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

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

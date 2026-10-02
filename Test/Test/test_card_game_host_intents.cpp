#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct card_game_host_intent_test_context
{
    ft_bool fail_after_intent;
};

static int32_t card_game_host_intent_test_callback(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_host_intent_test_context *callback_context;
    card_game_host_intent intent;
    card_game_operation operation;

    (void)engine;
    (void)context;
    callback_context = static_cast<card_game_host_intent_test_context *>(
        user_data);
    ft_bzero(&intent, sizeof(intent));
    intent.type_id = 0x47544553U;
    intent.version = 2U;
    intent.field_count = 3U;
    intent.fields[0].type = CARD_GAME_HOST_INTENT_U32;
    intent.fields[0].value = 17U;
    intent.fields[1].type = CARD_GAME_HOST_INTENT_I64;
    intent.fields[1].value = static_cast<uint64_t>(-42LL);
    intent.fields[2].type = CARD_GAME_HOST_INTENT_BOOL;
    intent.fields[2].value = 1U;
    if (operations.append_host_intent(intent) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (callback_context != ft_nullptr
        && callback_context->fail_after_intent == FT_TRUE)
    {
        ft_bzero(&operation, sizeof(operation));
        operation.type = CARD_GAME_OPERATION_HEALTH;
        operation.player_id = UINT32_MAX;
        operation.amount = 1;
        return (operations.append(operation));
    }
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_host_intent_unregistered_callback(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_host_intent intent;

    (void)engine;
    (void)context;
    (void)user_data;
    ft_bzero(&intent, sizeof(intent));
    intent.type_id = 900U;
    intent.version = 1U;
    return (operations.append_host_intent(intent));
}

static int32_t card_game_host_intent_test_setup(card_game_engine *engine,
    card_game_host_intent_test_context *callback_context) noexcept
{
    card_game_rules rules;
    card_game_host_intent_definition definition;
    uint32_t effect_id;
    int32_t error_code;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    error_code = engine->initialize(rules);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_bzero(&definition, sizeof(definition));
    definition.type_id = 0x47544553U;
    definition.version = 2U;
    definition.field_count = 3U;
    definition.field_types[0] = CARD_GAME_HOST_INTENT_U32;
    definition.field_types[1] = CARD_GAME_HOST_INTENT_I64;
    definition.field_types[2] = CARD_GAME_HOST_INTENT_BOOL;
    error_code = engine->register_host_intent_type(definition);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = engine->start_match(2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = engine->register_effect_callback(
        card_game_host_intent_test_callback, callback_context, 0x47544554U,
        &effect_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (engine->emit_event(0x47544554U, 1U, 0U));
}

FT_TEST(test_card_game_host_intents_are_registered_and_acknowledged_in_order)
{
    card_game_engine engine;
    card_game_host_intent_test_context callback_context;
    card_game_host_intent_record record;
    uint32_t pending_count;

    callback_context.fail_after_intent = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_test_setup(&engine, &callback_context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(1U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(1U, record.sequence);
    FT_ASSERT_EQ(0x47544553U, record.intent.type_id);
    FT_ASSERT_EQ(2U, record.intent.version);
    FT_ASSERT_EQ(3U, record.intent.field_count);
    FT_ASSERT_EQ(17U, record.intent.fields[0].value);
    FT_ASSERT_EQ(static_cast<uint64_t>(-42LL),
        record.intent.fields[1].value);
    FT_ASSERT_EQ(1U, record.intent.fields[2].value);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        engine.acknowledge_host_intent(record.sequence + 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(1U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(0U, pending_count);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_host_intents_are_not_published_on_effect_rollback)
{
    card_game_engine engine;
    card_game_host_intent_test_context callback_context;
    card_game_host_intent_record record;
    uint32_t pending_count;

    callback_context.fail_after_intent = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_test_setup(&engine, &callback_context));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(0U, pending_count);
    callback_context.fail_after_intent = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(1U, record.sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_host_intents_reject_unregistered_schema)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_host_intent_record record;
    uint32_t effect_id;

    rules.max_board_spaces = 4U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_effect_callback(
        card_game_host_intent_unregistered_callback, ft_nullptr, 0x47544554U,
        &effect_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.emit_event(0x47544554U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.resolve_events());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static const uint32_t CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_TYPE = 0x534E4150U;
static const uint32_t CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_EVENT = 0x534E4556U;

static int32_t card_game_host_intent_snapshot_test_callback(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    card_game_host_intent intent;

    (void)engine;
    (void)context;
    (void)user_data;
    ft_bzero(&intent, sizeof(intent));
    intent.type_id = CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_TYPE;
    intent.version = 1U;
    intent.field_count = 2U;
    intent.fields[0U].type = CARD_GAME_HOST_INTENT_U32;
    intent.fields[0U].value = 77U;
    intent.fields[1U].type = CARD_GAME_HOST_INTENT_BOOL;
    intent.fields[1U].value = 1U;
    return (operations.append_host_intent(intent));
}

static int32_t card_game_host_intent_snapshot_test_setup(
    card_game_engine *engine, ft_bool register_callback) noexcept
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
    definition.type_id = CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_TYPE;
    definition.version = 1U;
    definition.field_count = 2U;
    definition.field_types[0U] = CARD_GAME_HOST_INTENT_U32;
    definition.field_types[1U] = CARD_GAME_HOST_INTENT_BOOL;
    error_code = engine->register_host_intent_type(definition);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = engine->start_match(2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (register_callback == FT_TRUE)
    {
        error_code = engine->register_effect_callback(
            card_game_host_intent_snapshot_test_callback, ft_nullptr,
            CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_EVENT, &effect_id);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_host_intent_snapshot_test_emit(
    card_game_engine *engine) noexcept
{
    int32_t error_code;

    error_code = engine->emit_event(
        CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_EVENT, 1U, 0U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (engine->resolve_events());
}

FT_TEST(test_card_game_host_intents_survive_snapshot_delta_ack_and_revert)
{
    card_game_engine source_engine;
    card_game_engine replica_engine;
    card_game_snapshot baseline;
    card_game_snapshot pending_snapshot;
    card_game_snapshot acknowledged_snapshot;
    card_game_snapshot invalid_snapshot;
    card_game_delta pending_delta;
    card_game_delta acknowledged_delta;
    card_game_host_intent_record record;
    uint64_t pending_hash;
    uint64_t replica_hash;
    uint64_t acknowledged_hash;
    uint32_t pending_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_host_intent_snapshot_test_setup(
        &source_engine, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_host_intent_snapshot_test_setup(
        &replica_engine, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_snapshot(&baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.apply_snapshot(baseline));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_snapshot_test_emit(&source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_engine.get_snapshot(&pending_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_state_hash(&pending_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(baseline,
        &pending_delta));
    FT_ASSERT_EQ(FT_TRUE, pending_delta.global_state_changed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_engine.acknowledge_host_intent(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.apply_delta(pending_delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(pending_hash, replica_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(1U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.peek_pending_host_intent(
        &record));
    FT_ASSERT_EQ(1U, record.sequence);
    FT_ASSERT_EQ(77U, record.intent.fields[0U].value);

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_engine.get_snapshot(&acknowledged_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_engine.get_state_hash(&acknowledged_hash));
    FT_ASSERT_NEQ(pending_hash, acknowledged_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.create_delta(pending_snapshot,
        &acknowledged_delta));
    FT_ASSERT_EQ(FT_TRUE, acknowledged_delta.global_state_changed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.apply_delta(acknowledged_delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(0U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(acknowledged_hash, replica_hash);
    acknowledged_delta.next_host_intent_sequence = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        replica_engine.apply_delta(acknowledged_delta));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(acknowledged_hash, replica_hash);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.apply_snapshot(pending_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(pending_hash, replica_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_snapshot_test_emit(&replica_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.peek_pending_host_intent(&record));
    FT_ASSERT_EQ(2U, record.sequence);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.apply_snapshot(
        acknowledged_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(acknowledged_hash, replica_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(0U, pending_count);

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        replica_engine.get_snapshot(&invalid_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&pending_hash));
    invalid_snapshot.next_host_intent_sequence = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        replica_engine.apply_snapshot(invalid_snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.get_state_hash(&replica_hash));
    FT_ASSERT_EQ(pending_hash, replica_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replica_engine.destroy());
    return (1);
}

FT_TEST(test_card_game_host_intents_survive_byte_snapshot_restore)
{
    card_game_engine source_engine;
    card_game_engine restored_engine;
    card_game_host_intent_record record;
    card_game_host_intent_record second_record;
    std::vector<uint8_t> serialized;
    uint64_t source_hash;
    uint64_t restored_hash;
    uint32_t pending_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_host_intent_snapshot_test_setup(
        &source_engine, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, card_game_host_intent_snapshot_test_setup(
        &restored_engine, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_snapshot_test_emit(&source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_host_intent_snapshot_test_emit(&source_engine));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.serialize_snapshot(serialized));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_engine.restore_snapshot(
        serialized.data(), serialized.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_engine.get_state_hash(&restored_hash));
    FT_ASSERT_EQ(source_hash, restored_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(2U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.inspect_pending_host_intent(0U, &record));
    FT_ASSERT_EQ(1U, record.sequence);
    FT_ASSERT_EQ(CARD_GAME_HOST_INTENT_SNAPSHOT_TEST_TYPE,
        record.intent.type_id);
    FT_ASSERT_EQ(77U, record.intent.fields[0U].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.inspect_pending_host_intent(1U, &second_record));
    FT_ASSERT_EQ(2U, second_record.sequence);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        restored_engine.inspect_pending_host_intent(2U, &second_record));

    serialized[serialized.size() - 1U] ^= 0x01U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, restored_engine.restore_snapshot(
        serialized.data(), serialized.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_engine.get_state_hash(&restored_hash));
    FT_ASSERT_EQ(source_hash, restored_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.acknowledge_host_intent(record.sequence));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.get_pending_host_intent_count(&pending_count));
    FT_ASSERT_EQ(1U, pending_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        restored_engine.inspect_pending_host_intent(0U, &record));
    FT_ASSERT_EQ(2U, record.sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_engine.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_engine.destroy());
    return (1);
}

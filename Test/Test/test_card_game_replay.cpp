#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_replay.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"

static card_game_replay_event card_game_replay_test_event(uint64_t sequence,
    uint32_t player_id) noexcept
{
    card_game_replay_event event;

    ft_bzero(&event, sizeof(event));
    event.sequence = sequence;
    event.player_id = player_id;
    event.command_type = CARD_GAME_INTENT_PLAY_CARD;
    event.card_id = 10U + static_cast<uint32_t>(sequence);
    event.state_hash_before = sequence * 10U;
    event.state_hash_after = sequence * 10U + 1U;
    return (event);
}

FT_TEST(test_card_game_replay_full_round_trip_and_player_projection)
{
    card_game_replay_header header;
    card_game_replay_event event;
    card_game_replay full;
    card_game_replay view;
    card_game_replay restored;
    card_game_replay_event read_event;
    card_game_replay_result match_result;
    uint8_t serialized[1024];
    uint32_t serialized_size;

    ft_bzero(&header, sizeof(header));
    header.profile_id = 1U;
    header.format_id = 2U;
    header.corpus_version = 3U;
    header.rules_hash = 44U;
    header.corpus_hash = 55U;
    header.source_replay_hash = 66U;
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.initialize(header));
    event = card_game_replay_test_event(1U, 1U);
    event.private_owner_id = 1U;
    event.private_size = 2U;
    event.private_data[0] = 7U;
    event.private_data[1] = 8U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.append(event));
    ft_bzero(&match_result, sizeof(match_result));
    match_result.outcome = 1U;
    match_result.winner_player_id = 1U;
    match_result.final_state_hash = 101U;
    match_result.duration_epoch = 42U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.set_result(match_result));
    event = card_game_replay_test_event(2U, 2U);
    event.private_owner_id = 2U;
    event.private_size = 2U;
    event.private_data[0] = 9U;
    event.private_data[1] = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.append(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.project_player_view(1U, view));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.get_event(0U, &read_event));
    FT_ASSERT_EQ(2U, read_event.private_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.get_event(1U, &read_event));
    FT_ASSERT_EQ(0U, read_event.private_size);
    FT_ASSERT_EQ(0U, read_event.private_owner_id);
    FT_ASSERT_EQ(0U, read_event.private_data[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.serialize(serialized, sizeof(serialized),
        &serialized_size));
    FT_ASSERT_EQ(12U, serialized[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.deserialize(serialized,
        serialized_size));
    FT_ASSERT_EQ(2U, restored.event_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_result(&match_result));
    FT_ASSERT_EQ(1U, match_result.winner_player_id);
    FT_ASSERT_EQ(101U, match_result.final_state_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_event(1U, &read_event));
    FT_ASSERT_EQ(0U, read_event.private_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, full.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_rejects_private_data_in_wrong_view)
{
    card_game_replay_header header;
    card_game_replay replay;
    card_game_replay_event event;

    ft_bzero(&header, sizeof(header));
    header.visibility_mode = CARD_GAME_REPLAY_PLAYER_VIEW;
    header.viewer_player_id = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    event = card_game_replay_test_event(1U, 1U);
    event.private_owner_id = 2U;
    event.private_size = 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, replay.append(event));
    FT_ASSERT_EQ(0U, replay.event_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_projection_failure_preserves_output)
{
    card_game_replay_header header;
    card_game_replay source;
    card_game_replay output;
    card_game_replay_event source_event;
    card_game_replay_event output_event;

    ft_bzero(&header, sizeof(header));
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(header));
    source_event = card_game_replay_test_event(1U, 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append(source_event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize(header));
    output_event = card_game_replay_test_event(7U, 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.append(output_event));
    cma_set_alloc_limit(1U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, source.project_player_view(1U, output));
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.get_event(0U, &output_event));
    FT_ASSERT_EQ(7U, output_event.sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_file_round_trip_is_transactional)
{
    card_game_replay_header header;
    card_game_replay source;
    card_game_replay restored;
    card_game_replay_event event;
    const char *path;

    path = "card_game_replay_test.bin";
    (void)file_delete(path);
    ft_bzero(&header, sizeof(header));
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(header));
    event = card_game_replay_test_event(1U, 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.save_file(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.load_file(path));
    FT_ASSERT_EQ(1U, restored.event_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_event(0U, &event));
    FT_ASSERT_EQ(1U, event.sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_card_game_replay_presizes_restore_and_retains_session_validation)
{
    constexpr uint32_t COUNT = 300U;
    static uint8_t serialized[100000];
    static test_cma_failure_controller controller;
    card_game_replay_header header{};
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    card_game_replay source;
    card_game_replay restored;
    card_game_replay projected;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(header));
    uint32_t index = 0U;
    while (index < COUNT)
    {
        card_game_replay_event event = card_game_replay_test_event(index + 1U, 1U);
        if (index % 2U != 0U)
            event.session_id = 23U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append(event));
        ++index;
    }
    uint32_t size = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize(serialized, sizeof(serialized), &size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(controller));
    const int32_t result = restored.deserialize(serialized, size);
    const ft_size_t allocations = test_cma_failure_controller_attempt_count(
        controller, TEST_CMA_FAILURE_ALLOCATE);
    card_game_replay_event next = card_game_replay_test_event(COUNT + 1U, 1U);
    next.session_id = 24U;
    const int32_t rejected_session = restored.append(next);
    const uint32_t restored_count = restored.event_count();
    (void)restored.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_destroy(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(1U, allocations);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, rejected_session);
    FT_ASSERT_EQ(COUNT, restored_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.project_player_view(1U, projected));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, projected.append(next));
    next.session_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, projected.append(next));

    const char *path = "card_game_replay_stream_test.bin";
    (void)file_delete(path);
    cma_set_alloc_limit(1024U);
    const int32_t saved = source.save_file(path);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, saved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.append(next));
    cma_set_alloc_limit(1U);
    const int32_t rejected_save = source.save_file(path);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, rejected_save);
    ft_string file_data;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_data.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, file_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(size, file_data.size());
    FT_ASSERT_EQ(0, ft_memcmp(serialized, file_data.data(), size));
    return (1);
}

FT_TEST(test_card_game_replay_reconstructs_authoritative_commands)
{
    card_game_engine source;
    card_game_engine target;
    card_game_rules rules;
    card_game_phase_definition phase;
    card_game_phase_definition next_phase;
    card_game_command command;
    card_game_command_record record;
    card_game_replay_header header;
    card_game_replay replay;
    card_game_replay_event replay_event;
    uint32_t source_phase;
    uint32_t target_phase;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    phase.phase_id = 1U;
    phase.next_phase_id = 2U;
    phase.entry_event_type = 0U;
    phase.exit_event_type = 0U;
    phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    next_phase.phase_id = 2U;
    next_phase.next_phase_id = 1U;
    next_phase.entry_event_type = 0U;
    next_phase.exit_event_type = 0U;
    next_phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_phase(next_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    command.command_sequence = 1U;
    command.expected_state_sequence = 0U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    command.card_id = 0U;
    command.target_instance = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_current_phase(&source_phase));
    FT_ASSERT_EQ(2U, source_phase);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
    ft_bzero(&header, sizeof(header));
    header.rules_hash = record.rules_hash;
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.get_event(0U, &replay_event));
    FT_ASSERT_EQ(1U, replay_event.sequence);
    FT_ASSERT_EQ(0U, replay_event.player_id);
    FT_ASSERT_EQ(CARD_GAME_INTENT_ADVANCE_PHASE, replay_event.command_type);
    FT_ASSERT_EQ(record.state_hash_before, replay_event.state_hash_before);
    FT_ASSERT_EQ(record.state_hash_after, replay_event.state_hash_after);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_phase(next_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.replay_into(target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_current_phase(&target_phase));
    FT_ASSERT_EQ(source_phase, target_phase);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    return (1);
}

FT_TEST(test_card_game_submit_action_is_idempotent_and_serialized)
{
    card_game_engine source;
    card_game_engine target;
    card_game_engine reader;
    card_game_rules rules;
    card_game_phase_definition first_phase;
    card_game_phase_definition second_phase;
    card_game_action action;
    card_game_action_result action_result;
    card_game_command_record record;
    card_game_command_record restored_record;
    uint8_t command_log[1024];
    uint32_t command_log_size;
    uint64_t stable_hash;
    uint64_t hash_after_conflict;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    first_phase.phase_id = 1U;
    first_phase.next_phase_id = 2U;
    first_phase.entry_event_type = 0U;
    first_phase.exit_event_type = 0U;
    first_phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    second_phase.phase_id = 2U;
    second_phase.next_phase_id = 1U;
    second_phase.entry_event_type = 0U;
    second_phase.exit_event_type = 0U;
    second_phase.allowed_command_mask = CARD_GAME_COMMAND_ADVANCE_PHASE;
    ft_bzero(&action, sizeof(action));
    action.schema_version = 3U;
    action.session_id = 9901U;
    action.request_id = 7701U;
    action.command.command_sequence = 1U;
    action.command.player_id = 0U;
    action.command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_phase(first_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_phase(second_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_action(action, &action_result,
        ft_nullptr));
    FT_ASSERT_EQ(FT_FALSE, action_result.replayed);
    FT_ASSERT_EQ(action.session_id, action_result.session_id);
    FT_ASSERT_EQ(action.request_id, action_result.request_id);
    stable_hash = action_result.state_hash;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_action(action, &action_result,
        ft_nullptr));
    FT_ASSERT_EQ(FT_TRUE, action_result.replayed);
    FT_ASSERT_EQ(stable_hash, action_result.state_hash);
    action.session_id = 9902U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, source.submit_action(action,
        &action_result, ft_nullptr));
    action.session_id = 9901U;
    action.command.command_sequence = 2U;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.submit_action(action,
        &action_result, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&hash_after_conflict));
    FT_ASSERT_EQ(stable_hash, hash_after_conflict);
    action.command.command_sequence = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
    FT_ASSERT_EQ(action.request_id, record.request_id);
    FT_ASSERT_EQ(action.session_id, record.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_command_records(command_log,
        sizeof(command_log), &command_log_size));
    FT_ASSERT_EQ(13U, command_log[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.deserialize_command_records(
        command_log, command_log_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.get_command_record(0U,
        &restored_record));
    FT_ASSERT_EQ(action.request_id, restored_record.request_id);
    FT_ASSERT_EQ(action.session_id, restored_record.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.submit_action(action, &action_result,
        ft_nullptr));
    FT_ASSERT_EQ(FT_TRUE, action_result.replayed);
    FT_ASSERT_EQ(stable_hash, action_result.state_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_phase(first_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_phase(second_phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.replay_command_records(&record, 1U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_command_record(0U,
        &restored_record));
    FT_ASSERT_EQ(action.request_id, restored_record.request_id);
    FT_ASSERT_EQ(action.session_id, restored_record.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.destroy());
    return (1);
}

FT_TEST(test_card_game_command_journals_reject_duplicates_and_invalid_replay_hashes)
{
    card_game_engine source;
    card_game_engine target;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    const card_game_phase_definition phase{
        1U, 1U, 0U, 0U, CARD_GAME_COMMAND_ADVANCE_PHASE};
    card_game_action action{};
    card_game_action_result result{};
    card_game_command_record records[2];
    uint8_t journal[FT_CARD_GAME_REPLAY_HEADER_BYTES
        + 2U * FT_CARD_GAME_REPLAY_V13_RECORD_BYTES];
    uint32_t size;
    uint32_t count;
    uint64_t before_hash;
    uint64_t after_hash;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    action.schema_version = 2U;
    action.request_id = 11U;
    action.command.command_sequence = 1U;
    action.command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_action(action, &result, ft_nullptr));
    action.schema_version = 3U;
    action.session_id = 7U;
    action.command.command_sequence = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_action(action, &result, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &records[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(1U, &records[1]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_command_records(
        journal, sizeof(journal), &size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_phase(phase));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&before_hash));
    records[1].session_id = 0U;
    cma_set_alloc_limit(1U);
    const int32_t replay_error = target.replay_command_records(records, 2U,
        ft_nullptr);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, replay_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&after_hash));
    FT_ASSERT_EQ(before_hash, after_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_command_record_count(&count));
    FT_ASSERT_EQ(0U, count);
    records[1].session_id = 7U;
    uint64_t *hashes[] = {&records[1].rules_hash, &records[1].state_hash_before,
        &records[1].state_hash_after};
    uint32_t hash_index = 0U;
    while (hash_index < sizeof(hashes) / sizeof(hashes[0]))
    {
        *hashes[hash_index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
            target.replay_command_records(records, 2U, ft_nullptr));
        *hashes[hash_index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&after_hash));
        FT_ASSERT_EQ(before_hash, after_hash);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_command_record_count(&count));
        FT_ASSERT_EQ(0U, count);
        ++hash_index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.replay_command_records(records, 2U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.deserialize_command_records(journal, size));
    constexpr uint32_t SESSION_OFFSET = 2U * sizeof(uint64_t);
    ft_bzero(journal + FT_CARD_GAME_REPLAY_HEADER_BYTES
        + FT_CARD_GAME_REPLAY_V13_RECORD_BYTES + SESSION_OFFSET,
        sizeof(uint64_t));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        target.deserialize_command_records(journal, size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_command_record_count(&count));
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_command_record(1U, &records[1]));
    FT_ASSERT_EQ(7U, records[1].session_id);
    return (1);
}

FT_TEST(test_card_game_choice_action_is_idempotent_and_replayable)
{
    card_game_engine source;
    card_game_engine target;
    card_game_engine reader;
    card_game_rules rules;
    card_game_choice_option option;
    card_game_action action;
    card_game_action_result action_result;
    card_game_command_record record;
    card_game_command_record restored_record;
    card_game_choice choice;
    card_game_replay replay;
    card_game_replay restored_replay;
    card_game_replay_header replay_header;
    card_game_replay_event replay_event;
    uint8_t command_log[1024];
    uint8_t serialized_replay[512];
    uint32_t command_log_size;
    uint32_t serialized_replay_size;
    uint32_t replay_size;
    uint32_t source_choice_id;
    uint32_t target_choice_id;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    option.option_id = 7U;
    option.value_a = 12U;
    option.value_b = 3U;
    ft_bzero(&action, sizeof(action));
    action.schema_version = 3U;
    action.session_id = 4411U;
    action.request_id = 8801U;
    action.command.command_sequence = 1U;
    action.command.player_id = 0U;
    action.command.type = CARD_GAME_INTENT_CHOOSE_OPTION;
    action.command.target_instance = option.option_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.open_choice(0U,
        CARD_GAME_CHOICE_MODE, 10U, option.option_id, &source_choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_choice_option(source_choice_id,
        option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.open_choice(0U,
        CARD_GAME_CHOICE_MODE, 10U, option.option_id, &target_choice_id));
    FT_ASSERT_EQ(source_choice_id, target_choice_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.add_choice_option(target_choice_id,
        option));
    action.command.card_id = source_choice_id;
    action.command.authoritative_epoch = 4U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source.submit_action(action,
        &action_result, ft_nullptr));
    action.command.authoritative_epoch = 0U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, source.submit_action(action,
        &action_result, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_host_action(action,
        &action_result, ft_nullptr, 4U));
    FT_ASSERT_EQ(FT_FALSE, action_result.replayed);
    FT_ASSERT_EQ(action.session_id, action_result.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_host_action(action,
        &action_result, ft_nullptr, 50U));
    FT_ASSERT_EQ(FT_TRUE, action_result.replayed);
    action.command.target_instance = 8U;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.submit_host_action(action,
        &action_result, ft_nullptr, 50U));
    action.command.target_instance = option.option_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_choice(source_choice_id, &choice));
    FT_ASSERT_EQ(option.option_id, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
    FT_ASSERT_EQ(4U, record.command.authoritative_epoch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_command_records(command_log,
        sizeof(command_log), &command_log_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.deserialize_command_records(
        command_log, command_log_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.get_command_record(0U,
        &restored_record));
    FT_ASSERT_EQ(4U, restored_record.command.authoritative_epoch);
    ft_bzero(&replay_header, sizeof(replay_header));
    replay_header.rules_hash = record.rules_hash;
    replay_header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.get_event(0U, &replay_event));
    FT_ASSERT_EQ(action.session_id, replay_event.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&replay_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(serialized_replay,
        sizeof(serialized_replay), &serialized_replay_size));
    FT_ASSERT_EQ(replay_size, serialized_replay_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.deserialize(serialized_replay,
        serialized_replay_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.get_event(0U, &replay_event));
    FT_ASSERT_EQ(4U, replay_event.authoritative_epoch);
    FT_ASSERT_EQ(action.session_id, replay_event.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.replay_command_records(&record, 1U,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_choice(target_choice_id, &choice));
    FT_ASSERT_EQ(option.option_id, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.destroy());
    return (1);
}

FT_TEST(test_card_game_expired_choice_host_action_replays_result)
{
    card_game_engine source;
    card_game_engine reader;
    card_game_engine target;
    card_game_rules rules;
    card_game_choice_option option;
    card_game_choice choice;
    card_game_action action;
    card_game_action_result action_result;
    card_game_command_record record;
    card_game_command_record restored_record;
    card_game_replay replay;
    card_game_replay restored_replay;
    card_game_replay_header replay_header;
    card_game_replay_event replay_event;
    uint8_t command_log[1024];
    uint8_t replay_bytes[512];
    uint32_t command_log_size;
    uint32_t replay_size;
    uint32_t replay_written;
    uint32_t choice_id;
    uint32_t target_choice_id;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 2U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 3U;
    rules.max_turns = 10U;
    option.option_id = 9U;
    option.value_a = 0U;
    option.value_b = 0U;
    ft_bzero(&action, sizeof(action));
    action.schema_version = 1U;
    action.request_id = 9901U;
    action.command.command_sequence = 1U;
    action.command.type = CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.open_choice(0U,
        CARD_GAME_CHOICE_MODE, 10U, option.option_id, &choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_choice_option(choice_id, option));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, source.submit_action(action,
        &action_result, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_host_action(action,
        &action_result, ft_nullptr, 10U));
    FT_ASSERT_EQ(1U, action_result.result_value);
    FT_ASSERT_EQ(FT_FALSE, action_result.replayed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_host_action(action,
        &action_result, ft_nullptr, 50U));
    FT_ASSERT_EQ(1U, action_result.result_value);
    FT_ASSERT_EQ(FT_TRUE, action_result.replayed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
    FT_ASSERT_EQ(CARD_GAME_SERVER_ACTOR_ID, record.command.player_id);
    FT_ASSERT_EQ(10U, record.command.authoritative_epoch);
    FT_ASSERT_EQ(1U, record.result_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_command_records(command_log,
        sizeof(command_log), &command_log_size));
    FT_ASSERT_EQ(13U, command_log[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.deserialize_command_records(
        command_log, command_log_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.get_command_record(0U,
        &restored_record));
    FT_ASSERT_EQ(1U, restored_record.result_value);
    ft_bzero(&replay_header, sizeof(replay_header));
    replay_header.rules_hash = restored_record.rules_hash;
    replay_header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(replay_header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(
        restored_record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.get_event(0U, &replay_event));
    FT_ASSERT_EQ(action.session_id, replay_event.session_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&replay_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(replay_bytes,
        sizeof(replay_bytes), &replay_written));
    FT_ASSERT_EQ(replay_size, replay_written);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.deserialize(replay_bytes,
        replay_written));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.get_event(0U,
        &replay_event));
    FT_ASSERT_EQ(action.session_id, replay_event.session_id);
    FT_ASSERT_EQ(1U, replay_event.result_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.open_choice(0U,
        CARD_GAME_CHOICE_MODE, 10U, option.option_id, &target_choice_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.add_choice_option(target_choice_id,
        option));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.replay_into(target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_choice(target_choice_id, &choice));
    FT_ASSERT_EQ(option.option_id, choice.selected_option_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_replay.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_reconstructs_physical_hand_discard)
{
    card_game_engine source;
    card_game_engine target;
    card_game_engine command_log_reader;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_zone_definition discard_zone;
    card_game_deck_card drawn_card;
    card_game_command command;
    card_game_command_record record;
    card_game_replay_header header;
    card_game_replay replay;
    card_game_replay restored;
    card_game_replay_event restored_event;
    uint8_t serialized[1024];
    uint8_t command_log[FT_CARD_GAME_MAX_REPLAY_BYTES];
    uint32_t serialized_size;
    uint32_t command_log_size;
    uint32_t hand_count;
    uint32_t discard_count;
    uint64_t source_hash;
    uint64_t target_hash;

    rules.max_board_spaces = 2U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 5U;
    rules.max_mana = 10U;
    rules.max_turns = 10U;
    definition.card_id = 901U;
    definition.type = CARD_GAME_CREATURE;
    definition.cost = 0U;
    definition.attack = 0;
    definition.health = 0;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    discard_zone.zone_id = 9U;
    discard_zone.capacity = 4U;
    discard_zone.allowed_card_type_mask = 1U << CARD_GAME_CREATURE;
    discard_zone.owner_scoped = FT_TRUE;
    ft_bzero(&header, sizeof(header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_zone(discard_zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_push_top(0U, 901U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.draw_to_hand(0U, &drawn_card));
    ft_bzero(&command, sizeof(command));
    command.command_sequence = 1U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DISCARD_HAND;
    command.card_id = discard_zone.zone_id;
    command.target_instance = drawn_card.instance_id;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
    FT_ASSERT_EQ(command.card_id, record.command.card_id);
    FT_ASSERT_EQ(command.target_instance, record.command.target_instance);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.serialize_command_records(command_log,
        sizeof(command_log), &command_log_size));
    FT_ASSERT_EQ(13U, command_log[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_log_reader.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_log_reader.deserialize_command_records(
        command_log, command_log_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_rules_hash(&header.rules_hash));
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(serialized,
        sizeof(serialized), &serialized_size));
    FT_ASSERT_EQ(12U, serialized[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.deserialize(serialized,
        serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_event(0U, &restored_event));
    FT_ASSERT_EQ(CARD_GAME_INTENT_DISCARD_HAND, restored_event.command_type);
    FT_ASSERT_EQ(discard_zone.zone_id, restored_event.card_id);
    FT_ASSERT_EQ(drawn_card.instance_id, restored_event.target_instance);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_zone(discard_zone));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.deck_push_top(0U, 901U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.draw_to_hand(0U, &drawn_card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.replay_into(target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(0U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(0U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_zone_count(0U,
        discard_zone.zone_id, &discard_count));
    FT_ASSERT_EQ(1U, discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_zone_count(0U,
        discard_zone.zone_id, &discard_count));
    FT_ASSERT_EQ(1U, discard_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&target_hash));
    FT_ASSERT_EQ(source_hash, target_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, command_log_reader.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_command_is_replayable)
{
    card_game_engine source;
    card_game_engine target;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_command command;
    card_game_replay_header header;
    card_game_replay replay;
    card_game_deck_card card;
    uint8_t serialized[1024];
    uint32_t serialized_size;
    uint32_t hand_count;
    uint64_t source_hash;
    uint64_t target_hash;

    rules.max_board_spaces = 1U;
    rules.max_hand_size = 4U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 1U;
    rules.max_turns = 5U;
    ft_bzero(&definition, sizeof(definition));
    definition.card_id = 911U;
    definition.type = CARD_GAME_CREATURE;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    ft_bzero(&command, sizeof(command));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.deck_push_top(0U, definition.card_id));
    command.command_sequence = 1U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(1U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.hand_inspect(0U, 0U, &card));
    FT_ASSERT_EQ(definition.card_id, card.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_rules_hash(&header.rules_hash));
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    {
        card_game_command_record record;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_command_record(0U, &record));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialized_size(&serialized_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.serialize(serialized,
        sizeof(serialized), &serialized_size));
    FT_ASSERT_EQ(12U, serialized[4]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.deck_push_top(0U, definition.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.replay_into(target));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(1U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_state_hash(&source_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_state_hash(&target_hash));
    FT_ASSERT_EQ(source_hash, target_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    return (1);
}

FT_TEST(test_card_game_draw_command_full_hand_is_non_mutating)
{
    card_game_engine engine;
    card_game_rules rules;
    card_game_card_definition definition;
    card_game_command command;
    card_game_deck_card drawn;
    uint32_t hand_count;
    uint32_t deck_count;
    uint32_t record_count;

    rules.max_board_spaces = 1U;
    rules.max_hand_size = 1U;
    rules.starting_health = 20U;
    rules.starting_mana = 1U;
    rules.max_mana = 1U;
    rules.max_turns = 5U;
    ft_bzero(&definition, sizeof(definition));
    definition.card_id = 912U;
    definition.type = CARD_GAME_CREATURE;
    definition.effect_id = CARD_GAME_NO_EFFECT;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize(rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_card(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.start_match(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, definition.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.deck_push_top(0U, definition.card_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.draw_to_hand(0U, &drawn));
    ft_bzero(&command, sizeof(command));
    command.command_sequence = 1U;
    command.player_id = 0U;
    command.type = CARD_GAME_INTENT_DRAW_CARD;
    command.draw_count = 1U;
    FT_ASSERT_EQ(FT_ERR_FULL, engine.submit_command(command, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_hand_count(0U, &hand_count));
    FT_ASSERT_EQ(1U, hand_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_deck_count(0U, &deck_count));
    FT_ASSERT_EQ(1U, deck_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_command_record_count(&record_count));
    FT_ASSERT_EQ(0U, record_count);
    command.card_id = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        engine.submit_command(command, ft_nullptr));
    command.card_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_command_record_count(&record_count));
    FT_ASSERT_EQ(0U, record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_grows_past_original_capacity)
{
    card_game_replay_header header;
    card_game_replay replay;
    card_game_replay_event event;
    uint32_t index;

    ft_bzero(&header, sizeof(header));
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    index = 0U;
    while (index < 300U)
    {
        event = card_game_replay_test_event(static_cast<uint64_t>(index + 1U),
            1U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append(event));
        index += 1U;
    }
    FT_ASSERT_EQ(300U, replay.event_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.get_event(299U, &event));
    FT_ASSERT_EQ(300U, event.sequence);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    return (1);
}

FT_TEST(test_card_game_replay_accepts_authoritative_command_records)
{
    card_game_replay_header header;
    card_game_replay replay;
    card_game_command_record record;
    card_game_replay_event event;

    ft_bzero(&header, sizeof(header));
    header.rules_hash = 91U;
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.initialize(header));
    ft_bzero(&record, sizeof(record));
    record.command.command_sequence = 4U;
    record.command.expected_state_sequence = 7U;
    record.command.player_id = 2U;
    record.command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    record.command.card_id = 9U;
    record.command.target_instance = 11U;
    record.rules_hash = header.rules_hash;
    record.state_hash_before = 12U;
    record.state_hash_after = 13U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.append_command_record(record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.get_event(0U, &event));
    FT_ASSERT_EQ(4U, event.sequence);
    FT_ASSERT_EQ(7U, event.expected_state_sequence);
    FT_ASSERT_EQ(2U, event.player_id);
    FT_ASSERT_EQ(CARD_GAME_INTENT_ADVANCE_PHASE, event.command_type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay.destroy());
    return (1);
}

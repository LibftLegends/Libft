#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_replay.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

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

    path = "Test/card_game_replay_test.bin";
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

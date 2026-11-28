#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

int main(int argc, char **argv)
{
    const ft_bool replay_turn = argc == 2
        && ft_strcmp(argv[1], "--replay-turn") == 0;
    const ft_bool replay = replay_turn
        || (argc == 2 && ft_strcmp(argv[1], "--replay") == 0);
    if (argc != 1 && replay == FT_FALSE)
        return (1);
    constexpr uint32_t RECORD_COUNT = 8192U;
    constexpr uint32_t ITERATIONS = 8U;
    card_game_engine source;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    const card_game_phase_definition phase{
        1U, 1U, 0U, 0U,
        CARD_GAME_COMMAND_ADVANCE_PHASE | CARD_GAME_COMMAND_END_TURN};
    if (source.initialize(rules) != FT_ERR_SUCCESS
        || source.register_phase(phase) != FT_ERR_SUCCESS
        || source.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    card_game_action action{};
    action.schema_version = FT_CARD_GAME_ACTION_SCHEMA_VERSION;
    action.session_id = 1U;
    action.command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    if (replay_turn)
        action.command.type = CARD_GAME_INTENT_END_TURN;
    card_game_action_result result{};
    uint32_t index = 0U;
    while (index < RECORD_COUNT)
    {
        ++index;
        action.request_id = static_cast<uint32_t>(index * 0x9E3779B1U);
        action.command.command_sequence = index;
        if (source.submit_action(action, &result, ft_nullptr) != FT_ERR_SUCCESS)
            return (1);
    }
    constexpr uint32_t JOURNAL_SIZE = FT_CARD_GAME_REPLAY_HEADER_BYTES
        + RECORD_COUNT * FT_CARD_GAME_REPLAY_V13_RECORD_BYTES;
    std::vector<uint8_t> journal;
    std::vector<card_game_command_record> records;
    uint32_t journal_size = 0U;
    if (replay)
    {
        records.resize(RECORD_COUNT);
        index = 0U;
        while (index < RECORD_COUNT)
        {
            if (source.get_command_record(index, &records[index]) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
    }
    else
    {
        journal.resize(JOURNAL_SIZE);
        if (source.serialize_command_records(journal.data(), JOURNAL_SIZE,
            &journal_size) != FT_ERR_SUCCESS)
            return (1);
    }
    double elapsed = 0.0;
    uint64_t checksum = 0U;
    index = 0U;
    while (index < ITERATIONS)
    {
        card_game_engine reader;
        if (reader.initialize(rules) != FT_ERR_SUCCESS)
            return (1);
        if (replay && (reader.register_phase(phase) != FT_ERR_SUCCESS
                || reader.start_match(1U) != FT_ERR_SUCCESS))
            return (1);
        const auto start = std::chrono::steady_clock::now();
        int32_t error;
        if (replay)
            error = reader.replay_command_records(records.data(), RECORD_COUNT,
                ft_nullptr);
        else
            error = reader.deserialize_command_records(journal.data(), journal_size);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        uint32_t count;
        card_game_command_record record;
        if (error != FT_ERR_SUCCESS
            || reader.get_command_record_count(&count) != FT_ERR_SUCCESS
            || count != RECORD_COUNT
            || reader.get_command_record(count - 1U, &record) != FT_ERR_SUCCESS
            || record.state_hash_after != result.state_hash
            || record.request_id != action.request_id)
            return (1);
        checksum += record.state_hash_after ^ record.request_id;
        ++index;
    }
    std::printf("journal checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

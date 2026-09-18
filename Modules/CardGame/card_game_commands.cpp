#include "card_game_internal.hpp"

int32_t card_game_engine::grow_command_records() noexcept
{
    card_game_command_record *records;
    uint32_t capacity;

    if (this->_command_record_capacity >= FT_CARD_GAME_MAX_COMMAND_RECORDS)
        return (FT_ERR_FULL);
    capacity = this->_command_record_capacity * 2U;
    if (capacity < this->_command_record_capacity
        || capacity > FT_CARD_GAME_MAX_COMMAND_RECORDS)
        capacity = FT_CARD_GAME_MAX_COMMAND_RECORDS;
    records = static_cast<card_game_command_record *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(card_game_command_record)));
    if (records == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_command_record_count != 0U)
        ft_memcpy(records, this->_command_records,
            static_cast<ft_size_t>(this->_command_record_count)
                * sizeof(card_game_command_record));
    cma_free(this->_command_records);
    this->_command_records = records;
    this->_command_record_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::submit_command(
    const card_game_command &command, void *context) noexcept
{
    int32_t command_error;
    uint64_t rules_hash;
    uint64_t state_hash_before;
    uint64_t state_hash_after;
    uint32_t hand_index;
    uint32_t hand_instance_id;

    if (this->_initialised_state != 2U
        || command.command_sequence == 0U
        || command.command_sequence <= this->_last_command_sequence)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_command_record_count >= FT_CARD_GAME_MAX_COMMAND_RECORDS)
        return (FT_ERR_FULL);
    if (command.expected_state_sequence != 0U
        && command.expected_state_sequence != this->_state_sequence)
        return (FT_ERR_INVALID_STATE);
    if (command.player_id >= this->_player_count
        || command.player_id != this->_active_player)
        return (FT_ERR_PERMISSION_DENIED);
    if (this->_command_record_count == this->_command_record_capacity)
    {
        command_error = this->grow_command_records();
        if (command_error != FT_ERR_SUCCESS)
            return (command_error);
    }
    if (this->get_rules_hash(&rules_hash) != FT_ERR_SUCCESS
        || this->get_state_hash(&state_hash_before) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (command.type == CARD_GAME_INTENT_PLAY_CARD)
    {
        hand_instance_id = 0U;
        hand_index = 0U;
        while (hand_index < this->_hand_count[command.player_id])
        {
            if (this->_hand[command.player_id][hand_index].card_id
                == command.card_id)
            {
                hand_instance_id = this->_hand[command.player_id]
                    [hand_index].instance_id;
                break ;
            }
            hand_index += 1U;
        }
        if (hand_instance_id == 0U)
            return (FT_ERR_NOT_FOUND);
        command_error = this->play_card_from_hand(command.player_id,
            hand_instance_id, command.target_instance, context);
    }
    else if (command.type == CARD_GAME_INTENT_END_TURN)
        command_error = this->end_turn();
    else if (command.type == CARD_GAME_INTENT_ADVANCE_PHASE)
        command_error = this->advance_phase();
    else
        return (FT_ERR_INVALID_ARGUMENT);
    if (command_error != FT_ERR_SUCCESS)
        return (command_error);
    if (this->get_state_hash(&state_hash_after) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    this->_command_records[this->_command_record_count].command = command;
    this->_command_records[this->_command_record_count].rules_hash = rules_hash;
    this->_command_records[this->_command_record_count].state_hash_before =
        state_hash_before;
    this->_command_records[this->_command_record_count].state_hash_after =
        state_hash_after;
    this->_command_record_count += 1U;
    this->_last_command_sequence = command.command_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_command_record_count(uint32_t *count) const noexcept
{
    if (count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    *count = this->_command_record_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_command_record(uint32_t index,
    card_game_command_record *record) const noexcept
{
    if (record == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    if (index >= this->_command_record_count)
        return (FT_ERR_NOT_FOUND);
    *record = this->_command_records[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::serialize_command_records(uint8_t *output,
    uint32_t output_capacity, uint32_t *output_size) const noexcept
{
    uint32_t required_size;
    uint32_t offset;
    uint32_t index;
    const card_game_command_record *record;

    if (output == ft_nullptr || output_size == ft_nullptr
        || this->_initialised_state != 2U)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = FT_CARD_GAME_REPLAY_HEADER_BYTES
        + this->_command_record_count * FT_CARD_GAME_REPLAY_RECORD_BYTES;
    if (output_capacity < required_size)
        return (FT_ERR_OUT_OF_RANGE);
    offset = 0U;
    card_game_replay_write_u32(output, &offset, FT_CARD_GAME_REPLAY_MAGIC);
    card_game_replay_write_u32(output, &offset, FT_CARD_GAME_REPLAY_VERSION);
    card_game_replay_write_u32(output, &offset, this->_command_record_count);
    index = 0U;
    while (index < this->_command_record_count)
    {
        record = &this->_command_records[index];
        card_game_replay_write_u64(output, &offset,
            record->command.command_sequence);
        card_game_replay_write_u64(output, &offset,
            record->command.expected_state_sequence);
        card_game_replay_write_u32(output, &offset, record->command.player_id);
        card_game_replay_write_u32(output, &offset,
            static_cast<uint32_t>(record->command.type));
        card_game_replay_write_u32(output, &offset, record->command.card_id);
        card_game_replay_write_u32(output, &offset,
            record->command.target_instance);
        card_game_replay_write_u64(output, &offset, record->rules_hash);
        card_game_replay_write_u64(output, &offset, record->state_hash_before);
        card_game_replay_write_u64(output, &offset, record->state_hash_after);
        index += 1U;
    }
    *output_size = offset;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deserialize_command_records(const uint8_t *input,
    uint32_t input_size) noexcept
{
    card_game_command_record *records;
    uint32_t magic;
    uint32_t version;
    uint32_t record_count;
    uint32_t required_size;
    uint32_t offset;
    uint32_t index;
    uint32_t type;
    uint64_t previous_sequence;
    int32_t read_error;


    records = ft_nullptr;
    if (input == ft_nullptr || this->_initialised_state != 2U
        || input_size < FT_CARD_GAME_REPLAY_HEADER_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 0U;
    read_error = card_game_replay_read_u32(input, input_size, &offset, &magic);
    if (read_error != FT_ERR_SUCCESS || magic != FT_CARD_GAME_REPLAY_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    read_error = card_game_replay_read_u32(input, input_size, &offset,
        &version);
    if (read_error != FT_ERR_SUCCESS || version != FT_CARD_GAME_REPLAY_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    read_error = card_game_replay_read_u32(input, input_size, &offset,
        &record_count);
    if (read_error != FT_ERR_SUCCESS
        || record_count > FT_CARD_GAME_MAX_COMMAND_RECORDS)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = FT_CARD_GAME_REPLAY_HEADER_BYTES
        + record_count * FT_CARD_GAME_REPLAY_RECORD_BYTES;
    if (input_size != required_size)
        return (FT_ERR_INVALID_ARGUMENT);
    if (record_count != 0U)
    {
        records = static_cast<card_game_command_record *>(cma_malloc(
            static_cast<ft_size_t>(record_count)
                * sizeof(card_game_command_record)));
        if (records == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    previous_sequence = 0U;
    index = 0U;
    while (index < record_count)
    {
        card_game_command_record *record;

        record = &records[index];
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.command_sequence);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.expected_state_sequence);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.player_id);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &type);
        if (read_error != FT_ERR_SUCCESS || type < 1U || type > 3U
            || record->command.command_sequence == 0U
            || record->command.command_sequence <= previous_sequence)
        {
            cma_free(records);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        record->command.type = static_cast<card_game_command_type>(type);
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.card_id);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.target_instance);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->rules_hash);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->state_hash_before);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->state_hash_after);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        previous_sequence = record->command.command_sequence;
        index += 1U;
    }
    if (record_count > this->_command_record_capacity)
    {
        card_game_command_record *new_records;

        new_records = static_cast<card_game_command_record *>(cma_malloc(
            static_cast<ft_size_t>(record_count)
                * sizeof(card_game_command_record)));
        if (new_records == ft_nullptr)
        {
            cma_free(records);
            return (FT_ERR_NO_MEMORY);
        }
        cma_free(this->_command_records);
        this->_command_records = new_records;
        this->_command_record_capacity = record_count;
    }
    if (record_count != 0U)
        ft_memcpy(this->_command_records, records,
            static_cast<ft_size_t>(record_count)
                * sizeof(card_game_command_record));
    if (record_count > 0U)
        this->_last_command_sequence = this->_command_records[record_count - 1U]
            .command.command_sequence;
    else
        this->_last_command_sequence = 0U;
    cma_free(records);
    this->_command_record_count = record_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::replay_command_records(
    const card_game_command_record *records, uint32_t record_count,
    void *context) noexcept
{
    uint32_t index;
    uint64_t rules_hash;
    uint64_t state_hash;
    card_game_command_record executed_record;
    card_game_snapshot starting_snapshot;
    int32_t command_error;
    int32_t restore_error;

    if (this->_initialised_state != 2U || this->_player_count == 0U
        || record_count > FT_CARD_GAME_MAX_COMMAND_RECORDS
        || (record_count > 0U && records == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_command_record_count != 0U
        || this->_last_command_sequence != 0U)
        return (FT_ERR_INVALID_STATE);
    if (this->get_snapshot(&starting_snapshot) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    index = 0U;
    while (index < record_count)
    {
        if (this->get_rules_hash(&rules_hash) != FT_ERR_SUCCESS
            || rules_hash != records[index].rules_hash
            || this->get_state_hash(&state_hash) != FT_ERR_SUCCESS
            || state_hash != records[index].state_hash_before)
        {
            restore_error = this->apply_snapshot_internal(starting_snapshot);
            this->_command_record_count = 0U;
            this->_last_command_sequence = 0U;
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (FT_ERR_INVALID_STATE);
        }
        command_error = this->submit_command(records[index].command, context);
        if (command_error != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot_internal(starting_snapshot);
            this->_command_record_count = 0U;
            this->_last_command_sequence = 0U;
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (command_error);
        }
        command_error = this->get_command_record(
            this->_command_record_count - 1U, &executed_record);
        if (command_error != FT_ERR_SUCCESS
            || executed_record.state_hash_after
                != records[index].state_hash_after)
        {
            restore_error = this->apply_snapshot_internal(starting_snapshot);
            this->_command_record_count = 0U;
            this->_last_command_sequence = 0U;
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (FT_ERR_INVALID_STATE);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

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

int32_t card_game_engine::append_draw_host_intent(
    const card_game_host_intent &intent) noexcept
{
    int32_t result;

    result = this->validate_host_intent(intent);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_host_intent_record_count == UINT32_MAX
        || this->_next_host_intent_sequence == UINT64_MAX)
        return (FT_ERR_FULL);
    result = this->reserve_host_intent_records(
        this->_host_intent_record_count + 1U);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_host_intent_records[this->_host_intent_record_count].sequence =
        this->_next_host_intent_sequence;
    this->_host_intent_records[this->_host_intent_record_count].intent = intent;
    this->_host_intent_record_count += 1U;
    this->_next_host_intent_sequence += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::resolve_draw_command(uint32_t player_id,
    uint32_t draw_count, uint32_t required_discard_count) noexcept
{
    card_game_snapshot before_state;
    uint32_t index;
    int32_t result;

    if (draw_count == 0U || draw_count > FT_CARD_GAME_MAX_DRAW_COUNT
        || required_discard_count
            > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT)
        return (FT_ERR_INVALID_ARGUMENT);
    if (required_discard_count != 0U
        && (this->_draw_policy_configured != FT_TRUE
            || this->_draw_policy.discard_zone_id == 0U))
        return (FT_ERR_CONFIGURATION);
    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    index = 0U;
    while (index < draw_count)
    {
        result = this->resolve_draw_attempt(player_id);
        if (result != FT_ERR_SUCCESS)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
        }
        index += 1U;
    }
    if (required_discard_count != 0U
        && this->_hand_count[player_id] != 0U)
    {
        this->_pending_discard_player_id = player_id;
        this->_pending_discard_count = required_discard_count;
        if (this->_pending_discard_count > this->_hand_count[player_id])
            this->_pending_discard_count = this->_hand_count[player_id];
        this->_pending_discard_zone_id = this->_draw_policy.discard_zone_id;
        if (this->_state_sequence == UINT64_MAX)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? FT_ERR_FULL
                : restore_error);
        }
        this->_state_sequence += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::resolve_draw_attempt(uint32_t player_id) noexcept
{
    card_game_snapshot before_state;
    card_game_zone_entry entry;
    card_game_deck_card drawn_card;
    card_game_host_intent intent;
    uint32_t discard_count;
    uint32_t deck_count;
    uint32_t index;
    uint32_t card_type_id;
    ft_bool hand_full;
    ft_bool will_emit_intent;
    ft_bool deck_card_popped;
    int32_t result;

    if (this->_draw_policy_configured != FT_TRUE)
        return (this->draw_to_hand(player_id, &drawn_card));
    if (player_id >= this->_player_count)
        return (FT_ERR_INVALID_ARGUMENT);
    deck_count = this->_decks[player_id].size();
    discard_count = 0U;
    if (this->_draw_policy.reshuffle_discard == FT_TRUE)
        discard_count = this->_zone_store.size(player_id,
            this->_draw_policy.discard_zone_id);
    hand_full = (this->_hand_count[player_id] >= this->_rules.max_hand_size);
    will_emit_intent = FT_FALSE;
    if (hand_full == FT_TRUE && deck_count + discard_count > 0U)
        will_emit_intent = FT_TRUE;
    else if (deck_count + discard_count == 0U)
        will_emit_intent = FT_TRUE;
    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (will_emit_intent == FT_TRUE)
    {
        if (this->_host_intent_record_count == UINT32_MAX
            || this->_next_host_intent_sequence == UINT64_MAX)
            return (FT_ERR_FULL);
        result = this->reserve_host_intent_records(
            this->_host_intent_record_count + 1U);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    if (deck_count == 0U && discard_count > 0U)
    {
        if (discard_count > FT_CARD_GAME_MAX_CARDS - deck_count)
            result = FT_ERR_FULL;
        index = 0U;
        while (result == FT_ERR_SUCCESS && index < discard_count)
        {
            result = this->_zone_store.inspect(player_id,
                this->_draw_policy.discard_zone_id, index, &entry);
            if (result == FT_ERR_SUCCESS
                && this->find_card_type_id(entry.card_id, &card_type_id)
                    != FT_ERR_SUCCESS)
                result = FT_ERR_INVALID_STATE;
            index += 1U;
        }
        index = 0U;
        while (result == FT_ERR_SUCCESS && index < discard_count)
        {
            result = this->zone_pop_top(player_id,
                this->_draw_policy.discard_zone_id, &drawn_card);
            if (result == FT_ERR_SUCCESS)
                result = this->deck_push_top_instance(player_id,
                    drawn_card.instance_id, drawn_card.card_id);
            index += 1U;
        }
        if (result == FT_ERR_SUCCESS)
            result = this->shuffle_deck(player_id);
        if (result != FT_ERR_SUCCESS)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
        }
    }
    deck_count = this->_decks[player_id].size();
    if (deck_count == 0U)
    {
        ft_bzero(&intent, sizeof(intent));
        intent.type_id = this->_draw_policy.empty_draw_event_type_id;
        intent.version = this->_draw_policy.empty_draw_event_version;
        intent.field_count = 2U;
        intent.fields[0].type = CARD_GAME_HOST_INTENT_U32;
        intent.fields[0].value = player_id;
        intent.fields[1].type = CARD_GAME_HOST_INTENT_U32;
        intent.fields[1].value = 0U;
        result = this->append_draw_host_intent(intent);
        if (result != FT_ERR_SUCCESS)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
        }
        return (FT_ERR_SUCCESS);
    }
    if (hand_full == FT_TRUE)
    {
        ft_bzero(&entry, sizeof(entry));
        deck_card_popped = FT_FALSE;
        result = this->_decks[player_id].get_entry(0U, &entry);
        if (result == FT_ERR_SUCCESS)
            result = this->find_card_type_id(entry.card_id, &card_type_id);
        if (result == FT_ERR_SUCCESS)
        {
            result = this->_decks[player_id].pop_top_entry(&entry);
            if (result == FT_ERR_SUCCESS)
                deck_card_popped = FT_TRUE;
            if (result == FT_ERR_SUCCESS)
                result = this->_zone_store.insert_top(player_id,
                    this->_draw_policy.burned_zone_id, entry, card_type_id);
            if (result != FT_ERR_SUCCESS)
            {
                if (deck_card_popped == FT_TRUE)
                    (void)this->_decks[player_id].push_top_entry(entry);
                int32_t restore_error =
                    this->apply_snapshot_internal(before_state);

                return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
            }
            this->_state_sequence += 1U;
        }
        if (result != FT_ERR_SUCCESS)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
        }
        ft_bzero(&intent, sizeof(intent));
        intent.type_id = this->_draw_policy.overflow_event_type_id;
        intent.version = this->_draw_policy.overflow_event_version;
        intent.field_count = 2U;
        intent.fields[0].type = CARD_GAME_HOST_INTENT_U32;
        intent.fields[0].value = player_id;
        intent.fields[1].type = CARD_GAME_HOST_INTENT_U32;
        intent.fields[1].value = entry.instance_id;
        result = this->append_draw_host_intent(intent);
        if (result != FT_ERR_SUCCESS)
        {
            int32_t restore_error = this->apply_snapshot_internal(before_state);

            return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
        }
        return (FT_ERR_SUCCESS);
    }
    result = this->draw_to_hand(player_id, &drawn_card);
    if (result != FT_ERR_SUCCESS)
    {
        int32_t restore_error = this->apply_snapshot_internal(before_state);

        return (restore_error == FT_ERR_SUCCESS ? result : restore_error);
    }
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
    if (this->_pending_discard_count != 0U
        && (command.type != CARD_GAME_INTENT_DISCARD_HAND
            || command.player_id != this->_pending_discard_player_id))
        return (FT_ERR_PERMISSION_DENIED);
    if (command.player_id >= this->_player_count
        || command.player_id != this->_active_player)
        return (FT_ERR_PERMISSION_DENIED);
    if (command.type == CARD_GAME_INTENT_DRAW_CARD
        && (command.draw_count == 0U
            || command.draw_count > FT_CARD_GAME_MAX_DRAW_COUNT
            || command.required_discard_count
                > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT))
        return (FT_ERR_INVALID_ARGUMENT);
    if (command.type != CARD_GAME_INTENT_DRAW_CARD
        && (command.draw_count != 0U
            || command.required_discard_count != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
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
    else if (command.type == CARD_GAME_INTENT_DISCARD_HAND)
    {
        if (this->_pending_discard_count != 0U
            && (command.card_id != this->_pending_discard_zone_id
                || command.player_id != this->_pending_discard_player_id))
            return (FT_ERR_PERMISSION_DENIED);
        if (this->_pending_discard_count == 0U
            && this->is_command_allowed(CARD_GAME_COMMAND_DISCARD_HAND)
                == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        command_error = this->discard_hand_instance(command.player_id,
            command.target_instance, command.card_id);
        if (command_error == FT_ERR_SUCCESS
            && this->_pending_discard_count != 0U)
        {
            this->_pending_discard_count -= 1U;
            if (this->_pending_discard_count == 0U)
            {
                this->_pending_discard_player_id = 0U;
                this->_pending_discard_zone_id = 0U;
            }
        }
    }
    else if (command.type == CARD_GAME_INTENT_DRAW_CARD)
    {
        if (command.card_id != 0U || command.target_instance != 0U
            || command.draw_count == 0U
            || command.draw_count > FT_CARD_GAME_MAX_DRAW_COUNT
            || command.required_discard_count
                > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->is_command_allowed(CARD_GAME_COMMAND_DRAW_CARD)
            == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        command_error = this->resolve_draw_command(command.player_id,
            command.draw_count, command.required_discard_count);
    }
    else
        return (FT_ERR_INVALID_ARGUMENT);
    if (command_error != FT_ERR_SUCCESS)
        return (command_error);
    if (this->get_state_hash(&state_hash_after) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    this->_command_records[this->_command_record_count].command = command;
    if (command.type != CARD_GAME_INTENT_DRAW_CARD)
    {
        this->_command_records[this->_command_record_count].command.draw_count = 0U;
        this->_command_records[this->_command_record_count].command
            .required_discard_count = 0U;
    }
    this->_command_records[this->_command_record_count].request_id = 0U;
    this->_command_records[this->_command_record_count].rules_hash = rules_hash;
    this->_command_records[this->_command_record_count].state_hash_before =
        state_hash_before;
    this->_command_records[this->_command_record_count].state_hash_after =
        state_hash_after;
    this->_command_record_count += 1U;
    this->_last_command_sequence = command.command_sequence;
    return (FT_ERR_SUCCESS);
}

static ft_bool card_game_action_commands_equal(
    const card_game_command &first, const card_game_command &second) noexcept
{
    return (first.command_sequence == second.command_sequence
            && first.expected_state_sequence == second.expected_state_sequence
            && first.player_id == second.player_id
            && first.type == second.type && first.card_id == second.card_id
            && first.target_instance == second.target_instance
            && (first.type != CARD_GAME_INTENT_DRAW_CARD
                || (first.draw_count == second.draw_count
                    && first.required_discard_count
                        == second.required_discard_count))
        ? FT_TRUE : FT_FALSE);
}

int32_t card_game_engine::submit_action(const card_game_action &action,
    card_game_action_result *result, void *context) noexcept
{
    uint32_t index;
    int32_t error_code;

    if (this->_initialised_state != 2U || result == ft_nullptr
        || action.schema_version != 1U || action.request_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_command_record_count)
    {
        if (this->_command_records[index].request_id == action.request_id)
        {
            if (card_game_action_commands_equal(
                    this->_command_records[index].command, action.command)
                == FT_FALSE)
                return (FT_ERR_ALREADY_EXISTS);
            result->state_hash =
                this->_command_records[index].state_hash_after;
            result->replayed = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    error_code = this->submit_command(action.command, context);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_command_records[this->_command_record_count - 1U].request_id =
        action.request_id;
    result->state_hash = this->_command_records[
        this->_command_record_count - 1U].state_hash_after;
    result->replayed = FT_FALSE;
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
        + this->_command_record_count * FT_CARD_GAME_REPLAY_V6_RECORD_BYTES;
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
        card_game_replay_write_u64(output, &offset, record->request_id);
        card_game_replay_write_u64(output, &offset,
            record->command.expected_state_sequence);
        card_game_replay_write_u32(output, &offset, record->command.player_id);
        card_game_replay_write_u32(output, &offset,
            static_cast<uint32_t>(record->command.type));
        card_game_replay_write_u32(output, &offset, record->command.card_id);
        card_game_replay_write_u32(output, &offset,
            record->command.target_instance);
        card_game_replay_write_u32(output, &offset,
            record->command.draw_count);
        card_game_replay_write_u32(output, &offset,
            record->command.required_discard_count);
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
    uint32_t maximum_supported_type;
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
    if (read_error != FT_ERR_SUCCESS || (version < 1U
            || version > FT_CARD_GAME_REPLAY_VERSION))
        return (FT_ERR_INVALID_ARGUMENT);
    read_error = card_game_replay_read_u32(input, input_size, &offset,
        &record_count);
    if (read_error != FT_ERR_SUCCESS
        || record_count > FT_CARD_GAME_MAX_COMMAND_RECORDS)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = FT_CARD_GAME_REPLAY_HEADER_BYTES + record_count
        * (version >= 6U ? FT_CARD_GAME_REPLAY_V6_RECORD_BYTES
            : (version >= 5U ? FT_CARD_GAME_REPLAY_V5_RECORD_BYTES
                : (version >= 4U ? FT_CARD_GAME_REPLAY_RECORD_BYTES
                    : FT_CARD_GAME_REPLAY_LEGACY_RECORD_BYTES)));
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
    maximum_supported_type = 3U;
    if (version >= 2U)
        maximum_supported_type = 4U;
    if (version >= 3U)
        maximum_supported_type = 5U;
    index = 0U;
    while (index < record_count)
    {
        card_game_command_record *record;

        record = &records[index];
        ft_bzero(record, sizeof(*record));
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.command_sequence);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        record->request_id = 0U;
        if (version >= 4U)
        {
            read_error = card_game_replay_read_u64(input, input_size,
                &offset, &record->request_id);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
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
        if (read_error != FT_ERR_SUCCESS || type < 1U
            || type > maximum_supported_type
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
        record->command.draw_count = 0U;
        if (version >= 5U)
        {
            read_error = card_game_replay_read_u32(input, input_size,
                &offset, &record->command.draw_count);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
        }
        else if (record->command.type == CARD_GAME_INTENT_DRAW_CARD)
            record->command.draw_count = 1U;
        record->command.required_discard_count = 0U;
        if (version >= 6U)
        {
            read_error = card_game_replay_read_u32(input, input_size,
                &offset, &record->command.required_discard_count);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
        }
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->rules_hash);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
        }
        if ((record->command.type == CARD_GAME_INTENT_DRAW_CARD
                && (record->command.draw_count == 0U
                    || record->command.draw_count
                        > FT_CARD_GAME_MAX_DRAW_COUNT))
            || (record->command.type != CARD_GAME_INTENT_DRAW_CARD
                && (record->command.draw_count != 0U
                    || record->command.required_discard_count != 0U))
            || record->command.required_discard_count
                > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT)
        {
            cma_free(records);
            return (FT_ERR_INVALID_ARGUMENT);
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
        if (record->request_id != 0U)
        {
            uint32_t previous_index;

            previous_index = 0U;
            while (previous_index < index)
            {
                if (records[previous_index].request_id == record->request_id)
                {
                    cma_free(records);
                    return (FT_ERR_INVALID_ARGUMENT);
                }
                previous_index += 1U;
            }
        }
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
        uint32_t previous_index;

        previous_index = 0U;
        while (records[index].request_id != 0U
            && previous_index < index)
        {
            if (records[previous_index].request_id
                == records[index].request_id)
            {
                restore_error = this->apply_snapshot_internal(starting_snapshot);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (FT_ERR_INVALID_ARGUMENT);
            }
            previous_index += 1U;
        }
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
        this->_command_records[this->_command_record_count - 1U].request_id =
            records[index].request_id;
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

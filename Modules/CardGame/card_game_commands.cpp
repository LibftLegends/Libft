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
    this->_host_intent_records[this->_host_intent_record_count]
        .action_request_id = this->_active_action_request_id;
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
    uint32_t command_result_value;
    uint32_t hand_index;
    uint32_t hand_instance_id;
    uint32_t resolved_choice_count;
    uint32_t pending_choice_count;
    uint32_t pending_global_choice_count;
    ft_bool system_expiry_command;

    if (this->_initialised_state != 2U
        || command.command_sequence == 0U
        || command.command_sequence <= this->_last_command_sequence)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_command_record_count >= FT_CARD_GAME_MAX_COMMAND_RECORDS)
        return (FT_ERR_FULL);
    if (command.expected_state_sequence != 0U
        && command.expected_state_sequence != this->_state_sequence)
        return (FT_ERR_INVALID_STATE);
    system_expiry_command = command.type
            == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
        ? FT_TRUE : FT_FALSE;
    if (system_expiry_command == FT_TRUE
        && command.player_id != CARD_GAME_SERVER_ACTOR_ID)
        return (FT_ERR_PERMISSION_DENIED);
    if (system_expiry_command != FT_TRUE
        && command.player_id >= this->_player_count)
        return (FT_ERR_PERMISSION_DENIED);
    pending_choice_count = 0U;
    pending_global_choice_count = 0U;
    if (system_expiry_command != FT_TRUE)
    {
        command_error = this->_choices.pending_count_for_player(
            command.player_id, &pending_choice_count);
        if (command_error != FT_ERR_SUCCESS)
            return (command_error);
        command_error = this->_choices.pending_count(
            &pending_global_choice_count);
        if (command_error != FT_ERR_SUCCESS)
            return (command_error);
        if (this->_pending_discard_count != 0U
            && (command.type != CARD_GAME_INTENT_DISCARD_HAND
                || command.player_id != this->_pending_discard_player_id))
            return (FT_ERR_PERMISSION_DENIED);
        if (command.type == CARD_GAME_INTENT_CHOOSE_OPTION)
        {
            if (pending_choice_count == 0U)
                return (FT_ERR_PERMISSION_DENIED);
        }
        else
        {
            if (this->_pending_discard_count == 0U
                && pending_global_choice_count != 0U)
                return (FT_ERR_PERMISSION_DENIED);
            if (command.player_id != this->_active_player)
                return (FT_ERR_PERMISSION_DENIED);
        }
    }
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
    if (command.type == CARD_GAME_INTENT_PLAY_EFFECT
        && (command.destination_zone_id == 0U
            || command.required_discard_count != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (command.type != CARD_GAME_INTENT_PLAY_CARD
        && command.type != CARD_GAME_INTENT_PLAY_EFFECT
        && command.source_instance_id != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (command.type != CARD_GAME_INTENT_CHOOSE_OPTION
        && command.type != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
        && command.authoritative_epoch != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (command.type == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
        && (command.card_id != 0U || command.target_instance != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    command_result_value = 0U;
    ft_bzero(this->_active_play_payload,
        FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
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
            if (command.source_instance_id != 0U
                && this->_hand[command.player_id][hand_index].instance_id
                    == command.source_instance_id
                && this->_hand[command.player_id][hand_index].card_id
                    == command.card_id)
            {
                hand_instance_id = this->_hand[command.player_id]
                    [hand_index].instance_id;
                break ;
            }
            if (command.source_instance_id == 0U
                && this->_hand[command.player_id][hand_index].card_id
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
    else if (command.type == CARD_GAME_INTENT_PLAY_EFFECT)
    {
        card_game_snapshot before_state = {};
        const uint32_t discard_zone_id = command.destination_zone_id;
        ft_bool have_before_state = FT_FALSE;

        if (this->is_command_allowed(CARD_GAME_COMMAND_PLAY_CARD)
            == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        hand_index = 0U;
        while (hand_index < this->_hand_count[command.player_id]
            && this->_hand[command.player_id][hand_index].instance_id
                != command.source_instance_id)
            ++hand_index;
        if (command.card_id == 0U || command.target_instance == 0U
            || discard_zone_id == 0U
            || hand_index >= this->_hand_count[command.player_id]
            || this->_hand[command.player_id][hand_index].card_id
                != command.card_id)
            return (FT_ERR_NOT_FOUND);
        command_error = this->get_snapshot(&before_state);
        if (command_error == FT_ERR_SUCCESS)
        {
            have_before_state = FT_TRUE;
            ft_memcpy(this->_active_play_payload, command.play_payload,
                FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
            command_error = this->emit_event(command.target_instance,
                command.source_instance_id, command.card_id);
        }
        if (command_error == FT_ERR_SUCCESS)
            command_error = this->resolve_events();
        if (command_error == FT_ERR_SUCCESS)
            command_error = this->discard_hand_instance(command.player_id,
                command.source_instance_id, discard_zone_id);
        if (command_error != FT_ERR_SUCCESS
            && have_before_state == FT_TRUE)
        {
            const int32_t restore_error =
                this->apply_snapshot_internal(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
        }
        ft_bzero(this->_active_play_payload,
            FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
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
        if (command.card_id != 0U || command.source_instance_id != 0U
            || command.target_instance != 0U
            || command.draw_count == 0U
            || command.required_discard_count
                > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->is_command_allowed(CARD_GAME_COMMAND_DRAW_CARD)
            == FT_FALSE)
            return (FT_ERR_PERMISSION_DENIED);
        command_error = this->resolve_draw_command(command.player_id,
            command.draw_count, command.required_discard_count);
    }
    else if (command.type == CARD_GAME_INTENT_CHOOSE_OPTION)
    {
        if (command.card_id == 0U || command.source_instance_id != 0U
            || command.target_instance == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        command_error = this->choose_option(command.card_id,
            command.player_id, command.target_instance,
            command.authoritative_epoch);
    }
    else if (command.type == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES)
    {
        resolved_choice_count = 0U;
        command_error = this->resolve_expired_choices(
            command.authoritative_epoch, &resolved_choice_count);
        if (command_error == FT_ERR_SUCCESS)
            command_result_value = resolved_choice_count;
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
    if (command.type != CARD_GAME_INTENT_CHOOSE_OPTION
        && command.type != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES)
        this->_command_records[this->_command_record_count].command
            .authoritative_epoch = 0U;
    if (command.type != CARD_GAME_INTENT_PLAY_EFFECT)
    {
        ft_bzero(this->_command_records[this->_command_record_count]
                .command.play_payload,
            FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
        this->_command_records[this->_command_record_count]
            .command.destination_zone_id = 0U;
    }
    this->_command_records[this->_command_record_count].session_id = 0U;
    this->_command_records[this->_command_record_count].request_id = 0U;
    this->_command_records[this->_command_record_count].rules_hash = rules_hash;
    this->_command_records[this->_command_record_count].state_hash_before =
        state_hash_before;
    this->_command_records[this->_command_record_count].state_hash_after =
        state_hash_after;
    this->_command_records[this->_command_record_count].result_value =
        command_result_value;
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
            && first.source_instance_id == second.source_instance_id
            && first.target_instance == second.target_instance
        && ((first.type != CARD_GAME_INTENT_CHOOSE_OPTION
                && first.type != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES)
            || first.authoritative_epoch == second.authoritative_epoch)
            && ((first.type != CARD_GAME_INTENT_DRAW_CARD
                    && first.type != CARD_GAME_INTENT_PLAY_EFFECT)
                || (first.draw_count == second.draw_count
                    && first.required_discard_count
                        == second.required_discard_count))
            && (first.type != CARD_GAME_INTENT_PLAY_EFFECT
                || ft_memcmp(first.play_payload, second.play_payload,
                    FT_CARD_GAME_COMMAND_PAYLOAD_BYTES) == 0)
            && (first.type != CARD_GAME_INTENT_PLAY_EFFECT
                || first.destination_zone_id == second.destination_zone_id)
        ? FT_TRUE : FT_FALSE);
}

int32_t card_game_engine::submit_action(const card_game_action &action,
    card_game_action_result *result, void *context) noexcept
{
    return (this->submit_action_internal(action, result, context, 0U,
        FT_FALSE));
}

int32_t card_game_engine::submit_host_action(const card_game_action &action,
    card_game_action_result *result, void *context,
    uint64_t authoritative_epoch) noexcept
{
    return (this->submit_action_internal(action, result, context,
        authoritative_epoch, FT_TRUE));
}

int32_t card_game_engine::submit_action_internal(
    const card_game_action &action, card_game_action_result *result,
    void *context, uint64_t authoritative_epoch, ft_bool allow_choice) noexcept
{
    card_game_action effective_action;
    card_game_command prior_command;
    card_game_command requested_command;
    uint32_t index;
    uint64_t bound_session_id;
    int32_t error_code;

    if (this->_initialised_state != 2U || result == ft_nullptr
        || action.schema_version == 0U
        || action.schema_version > FT_CARD_GAME_ACTION_SCHEMA_VERSION
        || action.request_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if ((action.schema_version >= 3U && action.session_id == 0U)
        || (action.schema_version < 3U && action.session_id != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (action.command.authoritative_epoch != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if ((action.schema_version >= 2U
            && (action.command.type == CARD_GAME_INTENT_PLAY_CARD
                || action.command.type == CARD_GAME_INTENT_PLAY_EFFECT)
            && action.command.source_instance_id == 0U)
        || (action.schema_version == 1U
            && action.command.source_instance_id != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (action.command.type == CARD_GAME_INTENT_CHOOSE_OPTION
        || action.command.type == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES)
    {
        if (allow_choice != FT_TRUE)
            return (FT_ERR_PERMISSION_DENIED);
    }
    else if (authoritative_epoch != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    bound_session_id = 0U;
    index = 0U;
    while (index < this->_command_record_count)
    {
        const uint64_t record_session_id =
            this->_command_records[index].session_id;

        if (record_session_id != 0U)
        {
            if (bound_session_id != 0U
                && bound_session_id != record_session_id)
                return (FT_ERR_INVALID_STATE);
            bound_session_id = record_session_id;
        }
        index += 1U;
    }
    if (bound_session_id != 0U && bound_session_id != action.session_id)
        return (FT_ERR_INVALID_STATE);
    effective_action = action;
    if (effective_action.command.type == CARD_GAME_INTENT_CHOOSE_OPTION)
        effective_action.command.authoritative_epoch = authoritative_epoch;
    else if (effective_action.command.type
        == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES)
    {
        if (action.command.card_id != 0U
            || action.command.source_instance_id != 0U
            || action.command.target_instance != 0U
            || action.command.draw_count != 0U
            || action.command.required_discard_count != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        effective_action.command.player_id = CARD_GAME_SERVER_ACTOR_ID;
        effective_action.command.authoritative_epoch = authoritative_epoch;
    }
    index = 0U;
    while (index < this->_command_record_count)
    {
        if (this->_command_records[index].session_id == action.session_id
            && this->_command_records[index].request_id == action.request_id)
        {
            prior_command = this->_command_records[index].command;
            prior_command.authoritative_epoch = 0U;
            requested_command = effective_action.command;
            requested_command.authoritative_epoch = 0U;
            if (card_game_action_commands_equal(
                    prior_command, requested_command)
                == FT_FALSE)
                return (FT_ERR_ALREADY_EXISTS);
            result->state_hash =
                this->_command_records[index].state_hash_after;
            result->session_id = action.session_id;
            result->request_id = action.request_id;
            result->result_value =
                this->_command_records[index].result_value;
            result->replayed = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    this->_active_action_request_id = action.request_id;
    error_code = this->submit_command(effective_action.command, context);
    this->_active_action_request_id = 0U;
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_command_records[this->_command_record_count - 1U].session_id =
        action.session_id;
    this->_command_records[this->_command_record_count - 1U].request_id =
        action.request_id;
    result->session_id = action.session_id;
    result->request_id = action.request_id;
    result->state_hash = this->_command_records[
        this->_command_record_count - 1U].state_hash_after;
    result->result_value = this->_command_records[
        this->_command_record_count - 1U].result_value;
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
        + this->_command_record_count * FT_CARD_GAME_REPLAY_V13_RECORD_BYTES;
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
        card_game_replay_write_u64(output, &offset, record->session_id);
        card_game_replay_write_u64(output, &offset,
            record->command.expected_state_sequence);
        card_game_replay_write_u32(output, &offset, record->command.player_id);
        card_game_replay_write_u32(output, &offset,
            static_cast<uint32_t>(record->command.type));
        card_game_replay_write_u32(output, &offset, record->command.card_id);
        card_game_replay_write_u32(output, &offset,
            record->command.source_instance_id);
        card_game_replay_write_u32(output, &offset,
            record->command.target_instance);
        card_game_replay_write_u32(output, &offset,
            record->command.draw_count);
        card_game_replay_write_u32(output, &offset,
            record->command.required_discard_count);
        card_game_replay_write_u64(output, &offset,
            record->command.authoritative_epoch);
        card_game_replay_write_u64(output, &offset, record->rules_hash);
        card_game_replay_write_u64(output, &offset, record->state_hash_before);
        card_game_replay_write_u64(output, &offset, record->state_hash_after);
        card_game_replay_write_u32(output, &offset, record->result_value);
        ft_memcpy(output + offset, record->command.play_payload,
            FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
        offset += FT_CARD_GAME_COMMAND_PAYLOAD_BYTES;
        card_game_replay_write_u32(output, &offset,
            record->command.destination_zone_id);
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
    uint32_t record_size;
    uint32_t offset;
    uint32_t index;
    uint32_t type;
    uint32_t maximum_supported_type;
    uint64_t previous_sequence;
    uint64_t bound_session_id;
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
    record_size = FT_CARD_GAME_REPLAY_LEGACY_RECORD_BYTES;
    if (version >= 4U)
        record_size = FT_CARD_GAME_REPLAY_RECORD_BYTES;
    if (version >= 5U)
        record_size = FT_CARD_GAME_REPLAY_V5_RECORD_BYTES;
    if (version >= 6U)
        record_size = FT_CARD_GAME_REPLAY_V6_RECORD_BYTES;
    if (version >= 7U)
        record_size = FT_CARD_GAME_REPLAY_V7_RECORD_BYTES;
    if (version >= 8U)
        record_size = FT_CARD_GAME_REPLAY_V8_RECORD_BYTES;
    if (version >= 9U)
        record_size = FT_CARD_GAME_REPLAY_V9_RECORD_BYTES;
    if (version >= 10U)
        record_size = FT_CARD_GAME_REPLAY_V10_RECORD_BYTES;
    if (version >= 11U)
        record_size = FT_CARD_GAME_REPLAY_V11_RECORD_BYTES;
    if (version >= 12U)
        record_size = FT_CARD_GAME_REPLAY_V12_RECORD_BYTES;
    if (version >= 13U)
        record_size = FT_CARD_GAME_REPLAY_V13_RECORD_BYTES;
    required_size = FT_CARD_GAME_REPLAY_HEADER_BYTES
        + record_count * record_size;
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
    bound_session_id = 0U;
    maximum_supported_type = 3U;
    if (version >= 2U)
        maximum_supported_type = 4U;
    if (version >= 3U)
        maximum_supported_type = 5U;
    if (version >= 7U)
        maximum_supported_type = 6U;
    if (version >= 8U)
        maximum_supported_type = 7U;
    if (version >= 12U)
        maximum_supported_type = 8U;
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
        record->session_id = 0U;
        if (version >= 10U)
        {
            read_error = card_game_replay_read_u64(input, input_size,
                &offset, &record->session_id);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
            if (record->session_id != 0U)
            {
                if (bound_session_id != 0U
                    && bound_session_id != record->session_id)
                {
                    cma_free(records);
                    return (FT_ERR_INVALID_ARGUMENT);
                }
                bound_session_id = record->session_id;
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
        record->command.source_instance_id = 0U;
        if (version >= 9U)
        {
            read_error = card_game_replay_read_u32(input, input_size,
                &offset, &record->command.source_instance_id);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
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
        record->command.authoritative_epoch = 0U;
        if (version >= 7U)
        {
            read_error = card_game_replay_read_u64(input, input_size,
                &offset, &record->command.authoritative_epoch);
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
                && record->command.type != CARD_GAME_INTENT_PLAY_EFFECT
                && (record->command.draw_count != 0U
                    || record->command.required_discard_count != 0U))
            || (record->command.type == CARD_GAME_INTENT_PLAY_EFFECT
                && (record->command.card_id == 0U
                    || record->command.source_instance_id == 0U
                    || record->command.target_instance == 0U
                    || (version <= 12U && record->command.draw_count == 0U)
                    || record->command.required_discard_count != 0U))
            || (record->command.type != CARD_GAME_INTENT_PLAY_CARD
                && record->command.type != CARD_GAME_INTENT_PLAY_EFFECT
                && record->command.source_instance_id != 0U)
            || record->command.required_discard_count
                > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT
            || (record->session_id != 0U && record->request_id == 0U)
            || (record->command.type == CARD_GAME_INTENT_CHOOSE_OPTION
                && (record->command.card_id == 0U
                    || record->command.target_instance == 0U))
            || (record->command.type != CARD_GAME_INTENT_CHOOSE_OPTION
                && record->command.type
                    != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
                && record->command.authoritative_epoch != 0U))
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
        record->result_value = 0U;
        if (version >= 8U)
        {
            read_error = card_game_replay_read_u32(input, input_size,
                &offset, &record->result_value);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
        }
        if (version >= 11U)
        {
            if (input_size - offset < FT_CARD_GAME_COMMAND_PAYLOAD_BYTES)
            {
                cma_free(records);
                return (FT_ERR_INVALID_ARGUMENT);
            }
            ft_memcpy(record->command.play_payload, input + offset,
                FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
            offset += FT_CARD_GAME_COMMAND_PAYLOAD_BYTES;
        }
        record->command.destination_zone_id = 0U;
        if (version >= 13U)
        {
            read_error = card_game_replay_read_u32(input, input_size,
                &offset, &record->command.destination_zone_id);
            if (read_error != FT_ERR_SUCCESS)
            {
                cma_free(records);
                return (read_error);
            }
        }
        else if (version == 12U
            && record->command.type == CARD_GAME_INTENT_PLAY_EFFECT)
        {
            record->command.destination_zone_id =
                record->command.draw_count;
            record->command.draw_count = 0U;
        }
        if ((record->command.type == CARD_GAME_INTENT_PLAY_EFFECT
                && (record->command.destination_zone_id == 0U
                    || record->command.draw_count != 0U))
            || (record->command.type != CARD_GAME_INTENT_PLAY_EFFECT
                && record->command.destination_zone_id != 0U))
        {
            cma_free(records);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        if ((record->command.type
                == CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
                && (record->command.player_id != CARD_GAME_SERVER_ACTOR_ID
                    || record->command.card_id != 0U
                    || record->command.target_instance != 0U
                    || record->result_value > FT_CARD_GAME_MAX_CHOICES))
            || (record->command.type
                    != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
                && record->result_value != 0U)
            || (record->command.type
                    != CARD_GAME_INTENT_CHOOSE_OPTION
                && record->command.type
                    != CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES
                && record->command.authoritative_epoch != 0U))
        {
            cma_free(records);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        previous_sequence = record->command.command_sequence;
        if (record->request_id != 0U)
        {
            uint32_t previous_index;

            previous_index = 0U;
            while (previous_index < index)
            {
                if (records[previous_index].session_id == record->session_id
                    && records[previous_index].request_id == record->request_id)
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
    uint64_t bound_session_id;
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
    bound_session_id = 0U;
    index = 0U;
    while (index < record_count)
    {
        uint32_t previous_index;

        previous_index = 0U;
        while (records[index].request_id != 0U
            && previous_index < index)
        {
            if (records[previous_index].session_id
                    == records[index].session_id
                && records[previous_index].request_id
                    == records[index].request_id)
            {
                restore_error = this->apply_snapshot_internal(starting_snapshot);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (FT_ERR_INVALID_ARGUMENT);
            }
            previous_index += 1U;
        }
        if (records[index].session_id != 0U)
        {
            if (records[index].request_id == 0U
                || (bound_session_id != 0U
                    && bound_session_id != records[index].session_id))
            {
                restore_error = this->apply_snapshot_internal(starting_snapshot);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (FT_ERR_INVALID_ARGUMENT);
            }
            bound_session_id = records[index].session_id;
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
        this->_active_action_request_id = records[index].request_id;
        command_error = this->submit_command(records[index].command, context);
        this->_active_action_request_id = 0U;
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
        this->_command_records[this->_command_record_count - 1U].session_id =
            records[index].session_id;
        command_error = this->get_command_record(
            this->_command_record_count - 1U, &executed_record);
        if (command_error != FT_ERR_SUCCESS
            || executed_record.state_hash_after
                != records[index].state_hash_after
            || executed_record.result_value != records[index].result_value)
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

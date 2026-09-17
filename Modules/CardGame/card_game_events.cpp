#include "card_game_internal.hpp"

int32_t card_game_engine::emit_event(uint32_t event_type,
    uint32_t source_instance, uint32_t target_instance) noexcept
{
    int32_t grow_error;

    if (this->_initialised_state != 2U || event_type == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_event_count >= FT_CARD_GAME_MAX_EVENTS)
        return (FT_ERR_FULL);
    if (this->_event_count == this->_event_capacity)
    {
        grow_error = this->grow_events();
        if (grow_error != FT_ERR_SUCCESS)
            return (grow_error);
    }
    this->_events[this->_event_count].sequence = this->_event_sequence;
    this->_events[this->_event_count].event_type = event_type;
    this->_events[this->_event_count].source_instance = source_instance;
    this->_events[this->_event_count].target_instance = target_instance;
    this->_event_count += 1U;
    this->_event_sequence += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::grow_events() noexcept
{
    card_game_event *events;
    uint32_t capacity;

    if (this->_event_capacity >= FT_CARD_GAME_MAX_EVENTS)
        return (FT_ERR_FULL);
    capacity = this->_event_capacity * 2U;
    if (capacity < this->_event_capacity
        || capacity > FT_CARD_GAME_MAX_EVENTS)
        capacity = FT_CARD_GAME_MAX_EVENTS;
    events = static_cast<card_game_event *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(card_game_event)));
    if (events == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_event_count != 0U)
        ft_memcpy(events, this->_events,
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_event));
    if (this->_events != ft_nullptr)
        cma_free(this->_events);
    this->_events = events;
    this->_event_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_operation(
    const card_game_operation &operation) noexcept
{
    if (operation.type == CARD_GAME_OPERATION_HEALTH)
        return (this->modify_player_health(operation.player_id,
            operation.amount));
    if (operation.type == CARD_GAME_OPERATION_MANA)
    {
        if (operation.player_id >= FT_CARD_GAME_MAX_PLAYERS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (operation.amount < 0)
        {
            int64_t absolute_amount = -static_cast<int64_t>(operation.amount);

            if (this->_mana[operation.player_id]
                < static_cast<uint32_t>(absolute_amount))
                return (FT_ERR_OUT_OF_RANGE);
        }
        return (this->set_player_mana(operation.player_id,
            this->_mana[operation.player_id] + operation.amount));
    }
    if (operation.type == CARD_GAME_OPERATION_EMIT_EVENT)
        return (this->emit_event(operation.event_type,
            operation.source_instance, operation.target_instance));
    if (operation.type == CARD_GAME_OPERATION_DAMAGE_INSTANCE
        || operation.type == CARD_GAME_OPERATION_HEAL_INSTANCE)
    {
        if (operation.player_id >= this->_player_count
            || operation.target_instance >= this->_board_count[operation.player_id]
            || operation.amount <= 0)
            return (FT_ERR_INVALID_ARGUMENT);
        if (operation.type == CARD_GAME_OPERATION_DAMAGE_INSTANCE)
        {
            if (this->_instances[operation.player_id][operation.target_instance]
                    .damage_taken > INT32_MAX - operation.amount)
                this->_instances[operation.player_id]
                    [operation.target_instance].damage_taken = INT32_MAX;
            else
                this->_instances[operation.player_id]
                    [operation.target_instance].damage_taken += operation.amount;
        }
        else if (operation.amount
            >= this->_instances[operation.player_id][operation.target_instance]
                .damage_taken)
            this->_instances[operation.player_id]
                [operation.target_instance].damage_taken = 0;
        else
            this->_instances[operation.player_id]
                [operation.target_instance].damage_taken -= operation.amount;
        this->_state_sequence += 1U;
        return (FT_ERR_SUCCESS);
    }
    if (operation.type == CARD_GAME_OPERATION_MODIFY_INSTANCE_STATS)
    {
        uint32_t modifier_id;

        if (operation.player_id >= this->_player_count
            || operation.target_instance >= this->_board_count[operation.player_id]
            || (operation.duration != CARD_GAME_MODIFIER_PERMANENT
                && operation.duration != CARD_GAME_MODIFIER_UNTIL_END_TURN))
            return (FT_ERR_INVALID_ARGUMENT);
        return (this->add_card_modifier(operation.player_id,
            operation.target_instance, operation.attack_delta,
            operation.health_delta, operation.duration,
            operation.source_effect_id, &modifier_id));
    }
    return (FT_ERR_INVALID_ARGUMENT);
}

int32_t card_game_engine::resolve_events() noexcept
{
    uint32_t event_index;
    card_game_operation_buffer operations;
    card_game_effect_context context;
    card_game_operation operation;
    int32_t error_code;
    int32_t snapshot_error;
    int32_t restore_error;
    card_game_snapshot before_state;
    uint32_t operation_index;
    uint32_t processed_effect_count;
    uint32_t selected_effect_index;
    uint32_t candidate_index;
    uint32_t selected_priority;
    card_game_usage_limit usage_limit;
    ft_bool usage_limit_bound;
    int32_t usage_error;
    ft_bool processed_effects[FT_CARD_GAME_MAX_EFFECTS];

    error_code = operations.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    snapshot_error = this->get_snapshot(&before_state);
    if (snapshot_error != FT_ERR_SUCCESS)
        return (snapshot_error);
    event_index = 0U;
    while (event_index < this->_event_count)
    {
        context.event_type = this->_events[event_index].event_type;
        context.source_instance = this->_events[event_index].source_instance;
        context.target_instance = this->_events[event_index].target_instance;
        context.active_player = this->_active_player;
        context.turn_number = this->_turn_number;
        ft_bzero(processed_effects, sizeof(processed_effects));
        processed_effect_count = 0U;
        while (processed_effect_count < this->_effect_count)
        {
            selected_effect_index = UINT32_MAX;
            selected_priority = UINT32_MAX;
            candidate_index = 0U;
            while (candidate_index < this->_effect_count)
            {
                if (processed_effects[candidate_index] == FT_FALSE
                    && this->_effect_callbacks[candidate_index] != ft_nullptr
                    && this->_effect_event_types[candidate_index]
                        == context.event_type
                    && (selected_effect_index == UINT32_MAX
                        || this->_effect_priorities[candidate_index]
                            < selected_priority))
                {
                    selected_effect_index = candidate_index;
                    selected_priority =
                        this->_effect_priorities[candidate_index];
                }
                candidate_index += 1U;
            }
            if (selected_effect_index == UINT32_MAX)
                break ;
            processed_effects[selected_effect_index] = FT_TRUE;
            processed_effect_count += 1U;
            {
                usage_limit_bound = FT_FALSE;
                if (this->_effect_usage_limit_ids[selected_effect_index] != 0U)
                {
                    usage_error = this->_usage_limits.get(
                        this->_effect_usage_limit_ids[selected_effect_index],
                        &usage_limit);
                    if (usage_error != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (usage_error);
                    }
                    usage_limit_bound = FT_TRUE;
                    usage_error = this->_usage_limits.can_consume(
                        usage_limit.limit_id, 1U, context.turn_number);
                    if (usage_error != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (usage_error);
                    }
                    if (usage_limit.attempt_policy
                        == CARD_GAME_USAGE_ON_ATTEMPT)
                    {
                        usage_error = this->_usage_limits.consume(
                            usage_limit.limit_id, 1U, context.turn_number);
                        if (usage_error != FT_ERR_SUCCESS)
                            return (usage_error);
                    }
                }
                error_code = operations.clear();
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                error_code = this->_effect_callbacks[selected_effect_index](
                    *this, context, operations,
                    this->_effect_user_data[selected_effect_index]);
                if (error_code != FT_ERR_SUCCESS)
                {
                    restore_error = this->apply_snapshot_internal(before_state);
                    if (restore_error != FT_ERR_SUCCESS)
                        return (restore_error);
                    if (usage_limit_bound == FT_TRUE
                        && usage_limit.attempt_policy
                            == CARD_GAME_USAGE_ON_ATTEMPT)
                    {
                        usage_error = this->_usage_limits.consume(
                            usage_limit.limit_id, 1U, context.turn_number);
                        if (usage_error != FT_ERR_SUCCESS)
                            return (usage_error);
                    }
                    return (error_code);
                }
                if (usage_limit_bound == FT_TRUE
                    && usage_limit.attempt_policy
                        == CARD_GAME_USAGE_ON_ACTIVATION)
                {
                    usage_error = this->_usage_limits.consume(
                        usage_limit.limit_id, 1U, context.turn_number);
                    if (usage_error != FT_ERR_SUCCESS)
                    {
                    restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (usage_error);
                    }
                }
                operation_index = 0U;
                while (operation_index < operations.size())
                {
                    if (operations.get(operation_index, &operation)
                        != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        if (usage_limit_bound == FT_TRUE
                            && usage_limit.attempt_policy
                                == CARD_GAME_USAGE_ON_ATTEMPT)
                        {
                            usage_error = this->_usage_limits.consume(
                                usage_limit.limit_id, 1U,
                                context.turn_number);
                            if (usage_error != FT_ERR_SUCCESS)
                                return (usage_error);
                        }
                        return (FT_ERR_INVALID_STATE);
                    }
                    error_code = this->apply_operation(operation);
                    if (error_code != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        if (usage_limit_bound == FT_TRUE
                            && usage_limit.attempt_policy
                                == CARD_GAME_USAGE_ON_ATTEMPT)
                        {
                            usage_error = this->_usage_limits.consume(
                                usage_limit.limit_id, 1U,
                                context.turn_number);
                            if (usage_error != FT_ERR_SUCCESS)
                                return (usage_error);
                        }
                        return (error_code);
                    }
                    operation_index += 1U;
                }
                if (usage_limit_bound == FT_TRUE
                    && usage_limit.attempt_policy
                        == CARD_GAME_USAGE_ON_RESOLUTION)
                {
                    usage_error = this->_usage_limits.consume(
                        usage_limit.limit_id, 1U, context.turn_number);
                    if (usage_error != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot_internal(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (usage_error);
                    }
                }
            }
        }
        event_index += 1U;
    }
    this->_event_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::advance_phase() noexcept
{
    card_game_snapshot before_state;
    int32_t emit_error;
    int32_t resolve_error;
    int32_t restore_error;
    int32_t snapshot_error;
    ft_bool phase_found;
    uint32_t index;

    if (this->_initialised_state != 2U || this->_phase_count == 0U)
        return (FT_ERR_INVALID_STATE);
    if (this->is_command_allowed(CARD_GAME_COMMAND_ADVANCE_PHASE)
        == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    snapshot_error = this->get_snapshot(&before_state);
    if (snapshot_error != FT_ERR_SUCCESS)
        return (snapshot_error);
    phase_found = FT_FALSE;
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == this->_current_phase_id)
        {
            if (this->_phases[index].exit_event_type != 0U)
            {
                emit_error = this->emit_event(
                    this->_phases[index].exit_event_type, 0U, 0U);
                if (emit_error != FT_ERR_SUCCESS)
                {
                    restore_error = this->apply_snapshot_internal(before_state);
                    if (restore_error != FT_ERR_SUCCESS)
                        return (restore_error);
                    return (emit_error);
                }
            }
            this->_current_phase_id = this->_phases[index].next_phase_id;
            phase_found = FT_TRUE;
            break ;
        }
        index += 1U;
    }
    if (phase_found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == this->_current_phase_id)
        {
            if (this->_phases[index].entry_event_type != 0U)
            {
                emit_error = this->emit_event(
                    this->_phases[index].entry_event_type, 0U, 0U);
                if (emit_error != FT_ERR_SUCCESS)
                {
                    restore_error = this->apply_snapshot_internal(before_state);
                    if (restore_error != FT_ERR_SUCCESS)
                        return (restore_error);
                    return (emit_error);
                }
            }
            resolve_error = this->resolve_events();
            if (resolve_error != FT_ERR_SUCCESS)
            {
                restore_error = this->apply_snapshot_internal(before_state);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (resolve_error);
            }
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    restore_error = this->apply_snapshot_internal(before_state);
    if (restore_error != FT_ERR_SUCCESS)
        return (restore_error);
    return (FT_ERR_NOT_FOUND);
}

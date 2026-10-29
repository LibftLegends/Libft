#include "card_game_internal.hpp"

int32_t card_game_engine::find_host_intent_definition(uint32_t type_id,
    uint32_t version,
    const card_game_host_intent_definition **definition) const noexcept
{
    uint32_t index;

    if (definition == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_host_intent_type_count)
    {
        if (this->_host_intent_types[index].type_id == type_id
            && this->_host_intent_types[index].version == version)
        {
            *definition = &this->_host_intent_types[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::validate_host_intent(
    const card_game_host_intent &intent) const noexcept
{
    const card_game_host_intent_definition *definition;
    uint32_t index;
    int32_t error_code;

    if (intent.type_id == 0U || intent.version == 0U
        || intent.field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->find_host_intent_definition(intent.type_id,
        intent.version, &definition);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (intent.field_count != definition->field_count)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < intent.field_count)
    {
        if (intent.fields[index].type != definition->field_types[index])
            return (FT_ERR_INVALID_ARGUMENT);
        if (intent.fields[index].type == CARD_GAME_HOST_INTENT_U32
            && intent.fields[index].value > UINT32_MAX)
            return (FT_ERR_OUT_OF_RANGE);
        if (intent.fields[index].type == CARD_GAME_HOST_INTENT_I32)
        {
            int32_t signed_value;
            uint64_t canonical_value;

            signed_value = static_cast<int32_t>(
                static_cast<uint32_t>(intent.fields[index].value));
            canonical_value = static_cast<uint64_t>(
                static_cast<int64_t>(signed_value));
            if (canonical_value != intent.fields[index].value)
                return (FT_ERR_OUT_OF_RANGE);
        }
        if (intent.fields[index].type == CARD_GAME_HOST_INTENT_BOOL
            && intent.fields[index].value > 1U)
            return (FT_ERR_OUT_OF_RANGE);
        if (intent.fields[index].type != CARD_GAME_HOST_INTENT_U32
            && intent.fields[index].type != CARD_GAME_HOST_INTENT_I32
            && intent.fields[index].type != CARD_GAME_HOST_INTENT_U64
            && intent.fields[index].type != CARD_GAME_HOST_INTENT_I64
            && intent.fields[index].type != CARD_GAME_HOST_INTENT_BOOL)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_host_intent_type(
    const card_game_host_intent_definition &definition) noexcept
{
    const card_game_host_intent_definition *existing_definition;
    card_game_host_intent_definition normalized_definition;
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_player_count != 0U || definition.type_id == 0U
        || definition.version == 0U
        || definition.field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_host_intent_type_count >= FT_CARD_GAME_MAX_HOST_INTENT_TYPES)
        return (FT_ERR_FULL);
    index = 0U;
    while (index < definition.field_count)
    {
        if (definition.field_types[index] != CARD_GAME_HOST_INTENT_U32
            && definition.field_types[index] != CARD_GAME_HOST_INTENT_I32
            && definition.field_types[index] != CARD_GAME_HOST_INTENT_U64
            && definition.field_types[index] != CARD_GAME_HOST_INTENT_I64
            && definition.field_types[index] != CARD_GAME_HOST_INTENT_BOOL)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    if (this->find_host_intent_definition(definition.type_id,
            definition.version, &existing_definition) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    ft_bzero(&normalized_definition, sizeof(normalized_definition));
    normalized_definition.type_id = definition.type_id;
    normalized_definition.version = definition.version;
    normalized_definition.field_count = definition.field_count;
    index = 0U;
    while (index < definition.field_count)
    {
        normalized_definition.field_types[index]
            = definition.field_types[index];
        index += 1U;
    }
    this->_host_intent_types[this->_host_intent_type_count]
        = normalized_definition;
    this->_host_intent_type_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::configure_draw_policy(
    const card_game_draw_policy &policy) noexcept
{
    const card_game_host_intent_definition *definition;
    card_game_zone_store_definition zone;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_player_count != 0U || policy.discard_zone_id == 0U
        || policy.burned_zone_id == 0U
        || (policy.reshuffle_discard != FT_TRUE
            && policy.reshuffle_discard != FT_FALSE)
        || policy.discard_zone_id == policy.burned_zone_id
        || policy.empty_draw_event_type_id == 0U
        || policy.empty_draw_event_version == 0U
        || policy.overflow_event_type_id == 0U
        || policy.overflow_event_version == 0U
        || (policy.empty_draw_event_type_id == policy.overflow_event_type_id
            && policy.empty_draw_event_version
                == policy.overflow_event_version))
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.get_zone(policy.burned_zone_id, &zone);
    if (result != FT_ERR_SUCCESS || zone.owner_scoped != FT_TRUE
        || zone.capacity == 0U || zone.allowed_card_type_mask == 0U)
        return (FT_ERR_CONFIGURATION);
    result = this->_zone_store.get_zone(policy.discard_zone_id, &zone);
    if (result != FT_ERR_SUCCESS || zone.owner_scoped != FT_TRUE
        || zone.capacity == 0U || zone.allowed_card_type_mask == 0U)
        return (FT_ERR_CONFIGURATION);
    result = this->find_host_intent_definition(
        policy.empty_draw_event_type_id,
        policy.empty_draw_event_version, &definition);
    if (result != FT_ERR_SUCCESS || definition->field_count != 2U
        || definition->field_types[0] != CARD_GAME_HOST_INTENT_U32
        || definition->field_types[1] != CARD_GAME_HOST_INTENT_U32)
        return (FT_ERR_CONFIGURATION);
    result = this->find_host_intent_definition(
        policy.overflow_event_type_id,
        policy.overflow_event_version, &definition);
    if (result != FT_ERR_SUCCESS || definition->field_count != 2U
        || definition->field_types[0] != CARD_GAME_HOST_INTENT_U32
        || definition->field_types[1] != CARD_GAME_HOST_INTENT_U32)
        return (FT_ERR_CONFIGURATION);
    this->_draw_policy = policy;
    this->_draw_policy_configured = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_pending_host_intent_count(
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_host_intent_record_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_next_host_intent_sequence(
    uint64_t *sequence) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || sequence == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *sequence = this->_next_host_intent_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::inspect_pending_host_intent(uint32_t index,
    card_game_host_intent_record *record) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || record == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (index >= this->_host_intent_record_count)
        return (FT_ERR_NOT_FOUND);
    *record = this->_host_intent_records[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::peek_pending_host_intent(
    card_game_host_intent_record *record) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || record == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_host_intent_record_count == 0U)
        return (FT_ERR_NOT_FOUND);
    *record = this->_host_intent_records[0U];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::acknowledge_host_intent(uint64_t sequence) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_host_intent_record_count == 0U)
        return (FT_ERR_NOT_FOUND);
    if (this->_host_intent_records[0U].sequence != sequence)
        return (FT_ERR_INVALID_STATE);
    if (this->_state_sequence == UINT64_MAX)
        return (FT_ERR_FULL);
    this->_host_intent_record_count -= 1U;
    if (this->_host_intent_record_count != 0U)
        ft_memmove(this->_host_intent_records,
            this->_host_intent_records + 1U,
            static_cast<ft_size_t>(this->_host_intent_record_count)
                * sizeof(card_game_host_intent_record));
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::reserve_host_intent_records(
    uint32_t required_count) noexcept
{
    card_game_host_intent_record *records;
    uint32_t capacity;

    if (required_count > FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_FULL);
    if (required_count <= this->_host_intent_record_capacity)
        return (FT_ERR_SUCCESS);
    capacity = this->_host_intent_record_capacity;
    if (capacity == 0U)
        capacity = FT_CARD_GAME_HOST_INTENT_INITIAL_CAPACITY;
    while (capacity < required_count)
    {
        if (capacity >= FT_CARD_GAME_MAX_OPERATIONS / 2U)
        {
            capacity = FT_CARD_GAME_MAX_OPERATIONS;
            break ;
        }
        capacity *= 2U;
    }
    records = static_cast<card_game_host_intent_record *>(cma_malloc(
        static_cast<ft_size_t>(capacity)
            * sizeof(card_game_host_intent_record)));
    if (records == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_host_intent_record_count != 0U)
        ft_memcpy(records, this->_host_intent_records,
            static_cast<ft_size_t>(this->_host_intent_record_count)
                * sizeof(card_game_host_intent_record));
    if (this->_host_intent_records != ft_nullptr)
        cma_free(this->_host_intent_records);
    this->_host_intent_records = records;
    this->_host_intent_record_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

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
    if (operation.type == CARD_GAME_OPERATION_DRAW_CARDS)
    {
        if (operation.amount <= 0
            || operation.amount > static_cast<int32_t>(
                FT_CARD_GAME_MAX_DRAW_COUNT))
            return (FT_ERR_INVALID_ARGUMENT);
        return (this->resolve_draw_command(operation.player_id,
            static_cast<uint32_t>(operation.amount), 0U));
    }
    if (operation.type == CARD_GAME_OPERATION_REQUIRE_DISCARD)
    {
        if (operation.player_id >= this->_player_count
            || operation.player_id != this->_active_player
            || operation.amount <= 0
            || operation.amount > static_cast<int32_t>(
                FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT))
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->_pending_discard_count != 0U
            || this->_draw_policy_configured != FT_TRUE
            || this->_draw_policy.discard_zone_id == 0U)
            return (FT_ERR_CONFIGURATION);
        if (this->_hand_count[operation.player_id] == 0U)
            return (FT_ERR_SUCCESS);
        if (this->_state_sequence == UINT64_MAX)
            return (FT_ERR_FULL);
        this->_pending_discard_player_id = operation.player_id;
        this->_pending_discard_count = static_cast<uint32_t>(operation.amount);
        if (this->_pending_discard_count > this->_hand_count[operation.player_id])
            this->_pending_discard_count = this->_hand_count[operation.player_id];
        this->_pending_discard_zone_id = this->_draw_policy.discard_zone_id;
        this->_state_sequence += 1U;
        return (FT_ERR_SUCCESS);
    }
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
    card_game_operation_buffer pending_host_intents;
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
    error_code = pending_host_intents.initialize();
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
        context.action_request_id = this->_active_action_request_id;
        ft_memcpy(context.play_payload, this->_active_play_payload,
            FT_CARD_GAME_COMMAND_PAYLOAD_BYTES);
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
                    if (operation.type == CARD_GAME_OPERATION_HOST_INTENT)
                    {
                        error_code = this->validate_host_intent(
                            operation.host_intent);
                        if (error_code == FT_ERR_SUCCESS)
                            error_code = pending_host_intents.append(
                                operation);
                    }
                    else
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
    if (pending_host_intents.size() > 0U)
    {
        uint32_t required_count;

        if (pending_host_intents.size() > UINT32_MAX
                - this->_host_intent_record_count
            || pending_host_intents.size() > UINT64_MAX
                - this->_next_host_intent_sequence)
        {
            restore_error = this->apply_snapshot_internal(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (FT_ERR_FULL);
        }
        required_count = this->_host_intent_record_count
            + pending_host_intents.size();
        error_code = this->reserve_host_intent_records(required_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot_internal(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (error_code);
        }
        operation_index = 0U;
        while (operation_index < pending_host_intents.size())
        {
            if (pending_host_intents.get(operation_index, &operation)
                != FT_ERR_SUCCESS)
            {
                restore_error = this->apply_snapshot_internal(before_state);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (FT_ERR_INVALID_STATE);
            }
            this->_host_intent_records[this->_host_intent_record_count]
                .sequence = this->_next_host_intent_sequence;
            this->_host_intent_records[this->_host_intent_record_count]
                .action_request_id = this->_active_action_request_id;
            this->_host_intent_records[this->_host_intent_record_count]
                .intent = operation.host_intent;
            this->_host_intent_record_count += 1U;
            this->_next_host_intent_sequence += 1U;
            operation_index += 1U;
        }
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

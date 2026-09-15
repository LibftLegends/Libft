#include "card_game_internal.hpp"

int32_t card_game_engine::start_match(uint32_t player_count) noexcept
{
    int32_t event_error;
    uint32_t index;

    if (this->_initialised_state != 2U || player_count == 0U
        || player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_phase_count != 0U
        && this->_phases[0].entry_event_type != 0U
        && this->_event_capacity == 0U
        && this->grow_events() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < player_count)
    {
        this->_board_count[index] = 0U;
        this->_hand_count[index] = 0U;
        this->_health[index] = this->_rules.starting_health;
        this->_mana[index] = this->_rules.starting_mana;
        index += 1U;
    }
    this->_modifier_count = 0U;
    this->_next_modifier_id = 1U;
    this->_event_count = 0U;
    this->_event_sequence = 0U;
    this->_command_record_count = 0U;
    this->_last_command_sequence = 0U;
    this->_turn_number = 1U;
    this->_active_player = 0U;
    this->_player_count = player_count;
    this->_state_sequence += 1U;
    this->_current_phase_id = 0U;
    if (this->_phase_count != 0U)
    {
        this->_current_phase_id = this->_phases[0].phase_id;
        if (this->_phases[0].entry_event_type != 0U)
        {
            event_error = this->emit_event(
                this->_phases[0].entry_event_type, 0U, 0U);
            if (event_error != FT_ERR_SUCCESS)
                return (event_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::start_match(uint32_t player_count,
    const card_game_match_start_config &config) noexcept
{
    uint32_t player_id;
    uint32_t deck_count;
    uint32_t resolved_first_player;
    uint32_t resolved_health[FT_CARD_GAME_MAX_PLAYERS];
    uint32_t resolved_mana[FT_CARD_GAME_MAX_PLAYERS];
    uint32_t resolved_opening_hand[FT_CARD_GAME_MAX_PLAYERS];
    card_game_deck_card card;
    uint64_t candidate_random_state;
    uint64_t random_value;
    uint64_t random_limit;
    int32_t result;

    candidate_random_state = this->_random_state;
    if (this->_initialised_state != 2U || player_count == 0U
        || player_count > FT_CARD_GAME_MAX_PLAYERS
        || (config.random_first_player == FT_FALSE
            && config.first_player >= player_count)
        || config.opening_hand_size > this->_rules.max_hand_size)
        return (FT_ERR_INVALID_ARGUMENT);
    if (config.random_first_player != FT_FALSE
        && config.random_first_player != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (config.deal_opening_hand != FT_FALSE
        && config.deal_opening_hand != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    resolved_first_player = config.first_player;
    if (config.random_first_player != FT_FALSE)
    {
        random_limit = (UINT64_MAX / player_count) * player_count;
        do
        {
            random_value = card_game_match_random_next(
                &candidate_random_state);
        }
        while (random_value >= random_limit);
        resolved_first_player = static_cast<uint32_t>(random_value
            % player_count);
    }
    result = this->resolve_start_override(CARD_GAME_START_FIRST_PLAYER,
        CARD_GAME_START_ALL_PLAYERS, resolved_first_player,
        &resolved_first_player);
    if (result != FT_ERR_SUCCESS || resolved_first_player >= player_count)
    {
        if (result != FT_ERR_SUCCESS)
            return (result);
        return (FT_ERR_INVALID_STATE);
    }
    player_id = 0U;
    while (player_id < player_count)
    {
        result = this->resolve_start_override(CARD_GAME_START_HEALTH,
            player_id, config.starting_health,
            &resolved_health[player_id]);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = this->resolve_start_override(CARD_GAME_START_MANA,
            player_id, config.starting_mana,
            &resolved_mana[player_id]);
        if (result != FT_ERR_SUCCESS
            || resolved_mana[player_id] > this->_rules.max_mana)
        {
            if (result != FT_ERR_SUCCESS)
                return (result);
            return (FT_ERR_OUT_OF_RANGE);
        }
        result = this->resolve_start_override(CARD_GAME_START_OPENING_HAND,
            player_id, config.opening_hand_size,
            &resolved_opening_hand[player_id]);
        if (result != FT_ERR_SUCCESS
            || resolved_opening_hand[player_id] > this->_rules.max_hand_size)
        {
            if (result != FT_ERR_SUCCESS)
                return (result);
            return (FT_ERR_OUT_OF_RANGE);
        }
        deck_count = this->_decks[player_id].size();
        if (config.deal_opening_hand != FT_FALSE
            && deck_count < resolved_opening_hand[player_id])
            return (FT_ERR_FULL);
        player_id += 1U;
    }
    result = this->start_match(player_count);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_active_player = resolved_first_player;
    this->_state_sequence += 1U;
    player_id = 0U;
    while (player_id < player_count)
    {
        this->_health[player_id] = resolved_health[player_id];
        this->_mana[player_id] = resolved_mana[player_id];
        player_id += 1U;
    }
    if (config.deal_opening_hand != FT_FALSE)
    {
        player_id = 0U;
        while (player_id < player_count)
        {
            uint32_t draw_index;

            draw_index = 0U;
            while (draw_index < resolved_opening_hand[player_id])
            {
                result = this->draw_to_hand(player_id, &card);
                if (result != FT_ERR_SUCCESS)
                    return (result);
                draw_index += 1U;
            }
            player_id += 1U;
        }
    }
    if (config.random_first_player != FT_FALSE)
    {
        this->_random_state = candidate_random_state;
        this->_state_sequence += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_resource_pool(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t maximum_amount,
    uint32_t *pool_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.register_pool(owner_id, resource_type_id,
        maximum_amount, pool_id));
}

int32_t card_game_engine::add_resource_units(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount, uint32_t tags,
    uint64_t expiry_epoch, ft_bool temporary, uint32_t *unit_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.add_units(owner_id, resource_type_id, amount,
        tags, expiry_epoch, temporary, unit_id));
}

int32_t card_game_engine::get_resource_pool(uint32_t owner_id,
    uint32_t resource_type_id, card_game_resource_pool *pool) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.get_pool(owner_id, resource_type_id, pool));
}

int32_t card_game_engine::lock_resource_units(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount,
    uint64_t unlock_epoch) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (unlock_epoch == 0U)
        return (this->_resources.lock_units(owner_id, resource_type_id,
            amount));
    return (this->_resources.lock_units_until(owner_id, resource_type_id,
        amount, unlock_epoch));
}

int32_t card_game_engine::create_resource_payment_plan(uint32_t owner_id,
    const card_game_resource_requirement &requirement,
    card_game_payment_plan *plan) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.create_payment_plan(owner_id, requirement, plan));
}

int32_t card_game_engine::spend_resource_payment(
    const card_game_payment_plan &plan) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.spend(plan));
}

int32_t card_game_engine::create_resource_cost_plan(uint32_t owner_id,
    const card_game_cost &cost, uint32_t variable_amount,
    card_game_cost_plan *plan) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.create_cost_plan(owner_id, cost, variable_amount,
        plan));
}

int32_t card_game_engine::spend_resource_cost(
    const card_game_cost_plan &plan) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_resources.spend_cost(plan));
}

int32_t card_game_engine::register_allowance_predicate(uint32_t predicate_id,
    card_game_allowance_predicate predicate, void *user_data) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_allowances.register_predicate(predicate_id, predicate,
        user_data));
}

int32_t card_game_engine::grant_action_allowance(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint32_t uses,
    uint64_t expiry_epoch, uint32_t source_instance, uint32_t source_effect_id,
    uint32_t predicate_id, uint32_t predicate_context_id,
    uint32_t *allowance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_allowances.grant(owner_id, action_id, action_tags, uses,
        expiry_epoch, source_instance, source_effect_id, predicate_id,
        predicate_context_id, allowance_id));
}

int32_t card_game_engine::consume_action_allowance(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch,
    uint32_t *allowance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_allowances.consume_first(owner_id, action_id, action_tags,
        epoch, allowance_id));
}

int32_t card_game_engine::register_usage_limit(uint32_t key_id,
    uint32_t subject_id, card_game_usage_scope scope, uint64_t window_epoch,
    uint32_t maximum_uses, card_game_usage_attempt_policy attempt_policy,
    uint32_t source_instance, uint32_t *limit_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_usage_limits.register_limit(key_id, subject_id, scope,
        window_epoch, maximum_uses, attempt_policy, source_instance,
        limit_id));
}

int32_t card_game_engine::get_usage_limit(uint32_t limit_id,
    card_game_usage_limit *limit) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_usage_limits.get(limit_id, limit));
}

int32_t card_game_engine::consume_usage_limit(uint32_t limit_id,
    uint32_t amount, uint64_t current_epoch) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_usage_limits.consume(limit_id, amount, current_epoch));
}

int32_t card_game_engine::reset_usage_limits(uint64_t current_epoch) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_usage_limits.reset_epoch(current_epoch));
}

int32_t card_game_engine::open_choice(uint32_t player_id,
    card_game_choice_kind kind, uint64_t deadline_epoch,
    uint32_t default_option_id, uint32_t *choice_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_choices.open(player_id, kind, deadline_epoch,
        default_option_id, choice_id));
}

int32_t card_game_engine::add_choice_option(uint32_t choice_id,
    const card_game_choice_option &option) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_choices.add_option(choice_id, option));
}

int32_t card_game_engine::choose_option(uint32_t choice_id,
    uint32_t player_id, uint32_t option_id, uint64_t epoch) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_choices.choose(choice_id, player_id, option_id, epoch));
}

int32_t card_game_engine::get_choice(uint32_t choice_id,
    card_game_choice *choice) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_choices.get(choice_id, choice));
}

int32_t card_game_engine::set_player_mana(uint32_t player_id,
    uint32_t mana) noexcept
{
    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || mana > this->_rules.max_mana)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_mana[player_id] = mana;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::modify_player_health(uint32_t player_id,
    int32_t delta) noexcept
{
    int64_t health;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    health = static_cast<int64_t>(this->_health[player_id]) + delta;
    if (health < 0)
        health = 0;
    if (health > static_cast<int64_t>(UINT32_MAX))
        health = static_cast<int64_t>(UINT32_MAX);
    this->_health[player_id] = static_cast<uint32_t>(health);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::play_card(uint32_t player_id, uint32_t card_id,
    uint32_t target_instance, void *context) noexcept
{
    card_game_card_definition *definition;
    card_game_card_type_definition type_definition;
    uint32_t instance_index;
    uint32_t type_id;
    uint32_t existing_type_id;
    uint32_t board_index;
    uint32_t type_copy_count;
    uint32_t board_capacity;
    uint32_t zone_index;
    card_game_snapshot before_state;
    int32_t snapshot_error;
    int32_t restore_error;
    int32_t effect_error;

    if (this->_initialised_state != 2U || player_id != this->_active_player
        || this->is_command_allowed(CARD_GAME_COMMAND_PLAY_CARD) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    board_capacity = this->get_board_capacity();
    if (player_id >= FT_CARD_GAME_MAX_PLAYERS
        || this->_board_count[player_id] >= board_capacity)
        return (FT_ERR_FULL);
    if (this->find_card(card_id, &definition) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (type_id >= 32U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_card_type(type_id, &type_definition) == FT_ERR_SUCCESS)
    {
        if ((type_definition.allowed_zone_mask
            & (1U << CARD_GAME_BOARD_ZONE_ID)) == 0U)
            return (FT_ERR_PERMISSION_DENIED);
        type_copy_count = 0U;
        board_index = 0U;
        while (board_index < this->_board_count[player_id])
        {
            if (this->find_card_type_id(
                this->_instances[player_id][board_index].definition_id,
                &existing_type_id) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            if (existing_type_id == type_definition.type_id)
                type_copy_count += 1U;
            board_index += 1U;
        }
        if (type_copy_count >= type_definition.max_copies_per_player)
            return (FT_ERR_FULL);
    }
    zone_index = 0U;
    while (zone_index < this->_zone_count)
    {
        if (this->_zones[zone_index].zone_id == CARD_GAME_BOARD_ZONE_ID
            && (this->_zones[zone_index].allowed_card_type_mask
                & (1U << (type_id & 31U))) == 0U)
            return (FT_ERR_PERMISSION_DENIED);
        zone_index += 1U;
    }
    if (this->_mana[player_id] < definition->cost)
        return (FT_ERR_OUT_OF_RANGE);
    snapshot_error = this->get_snapshot(&before_state);
    if (snapshot_error != FT_ERR_SUCCESS)
        return (snapshot_error);
    this->_mana[player_id] -= definition->cost;
    instance_index = this->_board_count[player_id];
    this->_board[player_id][instance_index] = instance_index;
    this->_instances[player_id][instance_index].definition_id = card_id;
    this->_instances[player_id][instance_index].owner_id = player_id;
    this->_instances[player_id][instance_index].attack = definition->attack;
    this->_instances[player_id][instance_index].health = definition->health;
    this->_instances[player_id][instance_index].damage_taken = 0;
    this->_instances[player_id][instance_index].on_board = FT_TRUE;
    this->_board_count[player_id] += 1U;
    this->_state_sequence += 1U;
    if (definition->effect_id != CARD_GAME_NO_EFFECT
        && definition->effect_id < this->_effect_count
        && this->_effect_callbacks[definition->effect_id] != ft_nullptr)
    {
        card_game_operation_buffer operations;
        card_game_effect_context effect_context;
        card_game_operation operation;
        uint32_t operation_index;
        effect_context.event_type = 0U;
        effect_context.source_instance = instance_index;
        effect_context.target_instance = target_instance;
        effect_context.active_player = this->_active_player;
        effect_context.turn_number = this->_turn_number;
        effect_error = operations.initialize();
        if (effect_error != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (effect_error);
        }
        effect_error = this->_effect_callbacks[definition->effect_id](*this,
            effect_context, operations,
            this->_effect_user_data[definition->effect_id]);
        if (effect_error != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (effect_error);
        }
        operation_index = 0U;
        while (operation_index < operations.size())
        {
            if (operations.get(operation_index, &operation)
                != FT_ERR_SUCCESS)
            {
                restore_error = this->apply_snapshot(before_state);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (FT_ERR_INVALID_STATE);
            }
            effect_error = this->apply_operation(operation);
            if (effect_error != FT_ERR_SUCCESS)
            {
                restore_error = this->apply_snapshot(before_state);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (effect_error);
            }
            operation_index += 1U;
        }
        return (FT_ERR_SUCCESS);
    }
    if (definition->effect_id != CARD_GAME_NO_EFFECT
        && definition->effect_id < this->_effect_count
        && this->_effects[definition->effect_id] != ft_nullptr)
    {
        effect_error = this->_effects[definition->effect_id](*this,
            instance_index, target_instance, context);
        if (effect_error != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot(before_state);
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (effect_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::end_turn() noexcept
{
    card_game_snapshot before_state;
    int32_t snapshot_error;
    int32_t resolve_error;
    int32_t restore_error;

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_player_count == 0U)
        return (FT_ERR_INVALID_STATE);
    if (this->is_command_allowed(CARD_GAME_COMMAND_END_TURN) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    snapshot_error = this->get_snapshot(&before_state);
    if (snapshot_error != FT_ERR_SUCCESS)
        return (snapshot_error);
    this->_active_player = (this->_active_player + 1U)
        % this->_player_count;
    this->_turn_number += 1U;
    resolve_error = this->resolve_events();
    if (resolve_error != FT_ERR_SUCCESS)
    {
        restore_error = this->apply_snapshot(before_state);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (resolve_error);
    }
    resolve_error = this->expire_turn_modifiers();
    if (resolve_error != FT_ERR_SUCCESS)
    {
        restore_error = this->apply_snapshot(before_state);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (resolve_error);
    }
    this->_state_sequence += 1U;
    return (resolve_error);
}


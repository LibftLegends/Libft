#include "card_game_internal.hpp"

int32_t card_game_engine::get_snapshot(
    card_game_snapshot *snapshot) const noexcept
{
    card_game_snapshot *candidate;
    uint32_t player_id;
    int32_t result;

    if (this->_initialised_state != 2U || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    candidate = card_game_create_snapshot();
    if (candidate == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_event_count != 0U)
    {
        candidate->events = static_cast<card_game_event *>(cma_malloc(
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_event)));
        if (candidate->events == ft_nullptr)
        {
            card_game_destroy_snapshot(candidate);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(candidate->events, this->_events,
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_event));
    }
    candidate->format_version = FT_CARD_GAME_STATE_FORMAT_VERSION;
    candidate->state_sequence = this->_state_sequence;
    candidate->player_count = this->_player_count;
    candidate->turn_number = this->_turn_number;
    candidate->active_player = this->_active_player;
    candidate->current_phase_id = this->_current_phase_id;
    candidate->event_count = this->_event_count;
    candidate->event_sequence = this->_event_sequence;
    candidate->random_state = this->_random_state;
    candidate->next_deck_instance_id = this->_next_deck_instance_id;
    candidate->next_modifier_id = this->_next_modifier_id;
    candidate->modifier_count = this->_modifier_count;
    candidate->event_capacity = this->_event_count;
    ft_memcpy(candidate->modifiers, this->_modifiers,
        sizeof(candidate->modifiers));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (card_game_copy_player_snapshot(&candidate->players[player_id],
            this->_board[player_id], this->_instances[player_id],
            this->_board_count[player_id], this->_decks[player_id],
            this->_hand[player_id], this->_hand_count[player_id],
            this->_health[player_id], this->_mana[player_id])
            != FT_ERR_SUCCESS)
        {
            card_game_destroy_snapshot(candidate);
            return (FT_ERR_INVALID_STATE);
        }
        player_id += 1U;
    }
    result = this->_zone_store.get_snapshot(&candidate->zones);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
    result = this->_resources.get_snapshot(&candidate->resources);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
    result = this->_allowances.get_snapshot(&candidate->allowances);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
    result = this->_choices.get_snapshot(&candidate->choices);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
    result = this->_usage_limits.get_snapshot(&candidate->usage_limits);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
    if (snapshot->events != ft_nullptr)
        cma_free(snapshot->events);
    (void)card_game_zone_store::release_snapshot(&snapshot->zones);
    (void)card_game_resource_ledger::release_snapshot(&snapshot->resources);
    (void)card_game_allowance_ledger::release_snapshot(&snapshot->allowances);
    (void)card_game_choice_ledger::release_snapshot(&snapshot->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(
        &snapshot->usage_limits);
    snapshot->format_version = candidate->format_version;
    snapshot->state_sequence = candidate->state_sequence;
    snapshot->player_count = candidate->player_count;
    snapshot->turn_number = candidate->turn_number;
    snapshot->active_player = candidate->active_player;
    snapshot->current_phase_id = candidate->current_phase_id;
    snapshot->event_count = candidate->event_count;
    snapshot->event_sequence = candidate->event_sequence;
    snapshot->random_state = candidate->random_state;
    snapshot->next_deck_instance_id = candidate->next_deck_instance_id;
    snapshot->next_modifier_id = candidate->next_modifier_id;
    snapshot->modifier_count = candidate->modifier_count;
    snapshot->event_capacity = candidate->event_capacity;
    snapshot->events = candidate->events;
    ft_memcpy(snapshot->modifiers, candidate->modifiers,
        sizeof(snapshot->modifiers));
    ft_memcpy(snapshot->players, candidate->players,
        sizeof(snapshot->players));
    snapshot->zones = candidate->zones;
    snapshot->resources = candidate->resources;
    snapshot->allowances = candidate->allowances;
    snapshot->choices = candidate->choices;
    snapshot->usage_limits = candidate->usage_limits;
    candidate->events = ft_nullptr;
    ft_bzero(&candidate->zones, sizeof(candidate->zones));
    ft_bzero(&candidate->resources, sizeof(candidate->resources));
    ft_bzero(&candidate->allowances, sizeof(candidate->allowances));
    ft_bzero(&candidate->choices, sizeof(candidate->choices));
    ft_bzero(&candidate->usage_limits, sizeof(candidate->usage_limits));
    card_game_destroy_snapshot(candidate);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_rules_hash(uint64_t *hash) const noexcept
{
    uint64_t calculated_hash;
    uint32_t index;

    if (this->_initialised_state != 2U || hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    calculated_hash = 1469598103934665603ULL;
    card_game_hash_u32(&calculated_hash, this->_rules.max_board_spaces);
    card_game_hash_u32(&calculated_hash, this->_rules.max_hand_size);
    card_game_hash_u32(&calculated_hash, this->_rules.starting_health);
    card_game_hash_u32(&calculated_hash, this->_rules.starting_mana);
    card_game_hash_u32(&calculated_hash, this->_rules.max_mana);
    card_game_hash_u32(&calculated_hash, this->_rules.max_turns);
    card_game_hash_u32(&calculated_hash, this->_start_override_count);
    index = 0U;
    while (index < this->_start_override_count)
    {
        card_game_hash_u32(&calculated_hash,
            this->_start_overrides[index].source_id);
        card_game_hash_u32(&calculated_hash,
            this->_start_overrides[index].player_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_start_overrides[index].field));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_start_overrides[index].operation));
        card_game_hash_u32(&calculated_hash,
            this->_start_overrides[index].value);
        card_game_hash_u32(&calculated_hash,
            this->_start_overrides[index].priority);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_effect_count);
    index = 0U;
    while (index < this->_effect_count)
    {
        card_game_hash_u32(&calculated_hash,
            this->_effect_event_types[index]);
        card_game_hash_u32(&calculated_hash,
            this->_effect_priorities[index]);
        card_game_hash_u32(&calculated_hash,
            this->_effect_usage_limit_ids[index]);
        if (this->_effects[index] != ft_nullptr)
            card_game_hash_u32(&calculated_hash, 1U);
        else
            card_game_hash_u32(&calculated_hash, 2U);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_card_count);
    index = 0U;
    while (index < this->_card_count)
    {
        card_game_hash_u32(&calculated_hash, this->_cards[index].card_id);
        card_game_hash_u32(&calculated_hash, this->_card_type_ids[index]);
        card_game_hash_u32(&calculated_hash, this->_cards[index].cost);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_cards[index].attack));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_cards[index].health));
        card_game_hash_u32(&calculated_hash, this->_cards[index].effect_id);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_card_type_count);
    index = 0U;
    while (index < this->_card_type_count)
    {
        card_game_hash_u32(&calculated_hash,
            this->_card_types[index].type_id);
        card_game_hash_u32(&calculated_hash,
            this->_card_types[index].allowed_zone_mask);
        card_game_hash_u32(&calculated_hash,
            this->_card_types[index].max_copies_per_player);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_phase_count);
    index = 0U;
    while (index < this->_phase_count)
    {
        card_game_hash_u32(&calculated_hash, this->_phases[index].phase_id);
        card_game_hash_u32(&calculated_hash,
            this->_phases[index].next_phase_id);
        card_game_hash_u32(&calculated_hash,
            this->_phases[index].entry_event_type);
        card_game_hash_u32(&calculated_hash,
            this->_phases[index].exit_event_type);
        card_game_hash_u32(&calculated_hash,
            this->_phases[index].allowed_command_mask);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_zone_count);
    index = 0U;
    while (index < this->_zone_count)
    {
        card_game_hash_u32(&calculated_hash, this->_zones[index].zone_id);
        card_game_hash_u32(&calculated_hash, this->_zones[index].capacity);
        card_game_hash_u32(&calculated_hash,
            this->_zones[index].allowed_card_type_mask);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_zones[index].owner_scoped));
        index += 1U;
    }
    *hash = calculated_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_state_hash(uint64_t *hash) const noexcept
{
    card_game_snapshot *snapshot;
    uint64_t calculated_hash;
    uint32_t player_id;
    uint32_t event_index;
    uint32_t board_index;
    uint32_t deck_index;
    uint32_t hand_index;
    uint32_t index;

    if (this->_initialised_state != 2U || hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot = card_game_create_snapshot();
    if (snapshot == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->get_snapshot(snapshot) != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(snapshot);
        return (FT_ERR_INVALID_STATE);
    }
    calculated_hash = 1469598103934665603ULL;
    card_game_hash_u32(&calculated_hash, snapshot->format_version);
    card_game_hash_u64(&calculated_hash, snapshot->state_sequence);
    card_game_hash_u32(&calculated_hash, snapshot->player_count);
    card_game_hash_u32(&calculated_hash, snapshot->turn_number);
    card_game_hash_u32(&calculated_hash, snapshot->active_player);
    card_game_hash_u32(&calculated_hash, snapshot->current_phase_id);
    card_game_hash_u32(&calculated_hash, snapshot->event_count);
    card_game_hash_u64(&calculated_hash, snapshot->event_sequence);
    card_game_hash_u64(&calculated_hash, snapshot->random_state);
    card_game_hash_u32(&calculated_hash, snapshot->next_deck_instance_id);
    card_game_hash_u32(&calculated_hash, snapshot->next_modifier_id);
    card_game_hash_u32(&calculated_hash, snapshot->modifier_count);
    index = 0U;
    while (index < snapshot->modifier_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].modifier_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].source_effect_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].target_player_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].target_instance_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].target_instance_index);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->modifiers[index].attack_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->modifiers[index].health_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->modifiers[index].duration));
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].created_turn);
        card_game_hash_u32(&calculated_hash,
            snapshot->modifiers[index].created_phase_id);
        index += 1U;
    }
    event_index = 0U;
    while (event_index < snapshot->event_count)
    {
        card_game_hash_u64(&calculated_hash,
            snapshot->events[event_index].sequence);
        card_game_hash_u32(&calculated_hash,
            snapshot->events[event_index].event_type);
        card_game_hash_u32(&calculated_hash,
            snapshot->events[event_index].source_instance);
        card_game_hash_u32(&calculated_hash,
            snapshot->events[event_index].target_instance);
        event_index += 1U;
    }
    player_id = 0U;
    while (player_id < snapshot->player_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->players[player_id].board_count);
        card_game_hash_u32(&calculated_hash,
            snapshot->players[player_id].deck_count);
        card_game_hash_u32(&calculated_hash,
            snapshot->players[player_id].hand_count);
        card_game_hash_u32(&calculated_hash,
            snapshot->players[player_id].health);
        card_game_hash_u32(&calculated_hash,
            snapshot->players[player_id].mana);
        board_index = 0U;
        while (board_index < snapshot->players[player_id].board_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot->players[player_id].board[board_index]);
            card_game_hash_instance(&calculated_hash,
                snapshot->players[player_id].instances[board_index]);
            board_index += 1U;
        }
        deck_index = 0U;
        while (deck_index < snapshot->players[player_id].deck_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot->players[player_id].deck[deck_index]);
            card_game_hash_u32(&calculated_hash,
                snapshot->players[player_id].deck_instance_ids[deck_index]);
            deck_index += 1U;
        }
        hand_index = 0U;
        while (hand_index < snapshot->players[player_id].hand_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot->players[player_id].hand[hand_index]);
            card_game_hash_u32(&calculated_hash,
                snapshot->players[player_id]
                    .hand_instance_ids[hand_index]);
            hand_index += 1U;
        }
        player_id += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->zones.definition_count);
    index = 0U;
    while (index < snapshot->zones.definition_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->zones.definitions[index].zone_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->zones.definitions[index].capacity);
        card_game_hash_u32(&calculated_hash,
            snapshot->zones.definitions[index].allowed_card_type_mask);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->zones.definitions[index]
                .owner_scoped));
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->zones.entry_count);
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        index = 0U;
        while (index < FT_CARD_GAME_MAX_ZONES)
        {
            card_game_hash_u32(&calculated_hash,
            snapshot->zones.counts[player_id][index]);
            card_game_hash_u32(&calculated_hash,
            snapshot->zones.offsets[player_id][index]);
            index += 1U;
        }
        player_id += 1U;
    }
    index = 0U;
    while (index < snapshot->zones.entry_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->zones.entries[index].instance_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->zones.entries[index].card_id);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->resources.pool_count);
    card_game_hash_u32(&calculated_hash, snapshot->resources.unit_count);
    card_game_hash_u32(&calculated_hash, snapshot->resources.next_unit_id);
    index = 0U;
    while (index < snapshot->resources.pool_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].pool_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].resource_type_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].maximum_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].current_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].locked_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.pools[index].temporary_amount);
        index += 1U;
    }
    index = 0U;
    while (index < snapshot->resources.unit_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].unit_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].resource_type_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].amount);
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].tags);
        card_game_hash_u64(&calculated_hash,
            snapshot->resources.units[index].expiry_epoch);
        card_game_hash_u64(&calculated_hash,
            snapshot->resources.units[index].unlock_epoch);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->resources.units[index].temporary));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->resources.units[index].locked));
        card_game_hash_u32(&calculated_hash,
            snapshot->resources.units[index].locked_amount);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->allowances.count);
    card_game_hash_u32(&calculated_hash, snapshot->allowances.next_id);
    index = 0U;
    while (index < snapshot->allowances.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].allowance_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].action_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].action_tags);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].remaining_uses);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].maximum_uses);
        card_game_hash_u64(&calculated_hash,
            snapshot->allowances.allowances[index].expiry_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].source_instance);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].source_effect_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].predicate_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->allowances.allowances[index].predicate_context_id);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->choices.count);
    card_game_hash_u32(&calculated_hash, snapshot->choices.next_id);
    index = 0U;
    while (index < snapshot->choices.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->choices.choices[index].choice_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->choices.choices[index].player_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->choices.choices[index].kind));
        card_game_hash_u32(&calculated_hash,
            snapshot->choices.choices[index].option_count);
        card_game_hash_u64(&calculated_hash,
            snapshot->choices.choices[index].deadline_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot->choices.choices[index].default_option_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->choices.choices[index].selected_option_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->choices.choices[index].resolved));
        uint32_t option_index = 0U;
        while (option_index < snapshot->choices.choices[index].option_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot->choices.choices[index].options[option_index].option_id);
            card_game_hash_u32(&calculated_hash,
                snapshot->choices.choices[index].options[option_index].value_a);
            card_game_hash_u32(&calculated_hash,
                snapshot->choices.choices[index].options[option_index].value_b);
            option_index += 1U;
        }
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot->usage_limits.count);
    card_game_hash_u32(&calculated_hash, snapshot->usage_limits.next_id);
    index = 0U;
    while (index < snapshot->usage_limits.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].limit_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].key_id);
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].subject_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->usage_limits.limits[index].scope));
        card_game_hash_u64(&calculated_hash,
            snapshot->usage_limits.limits[index].window_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].maximum_uses);
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].used_uses);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot->usage_limits.limits[index]
                .attempt_policy));
        card_game_hash_u32(&calculated_hash,
            snapshot->usage_limits.limits[index].source_instance);
        index += 1U;
    }
    *hash = calculated_hash;
    return (card_game_destroy_snapshot(snapshot));
}

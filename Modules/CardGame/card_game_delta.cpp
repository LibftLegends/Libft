#include "card_game_internal.hpp"

int32_t card_game_engine::apply_snapshot_internal(
    const card_game_snapshot &snapshot) noexcept
{
    uint32_t player_id;
    uint32_t phase_index;
    uint32_t deck_index;
    uint32_t hand_index;
    uint32_t instance_id;
    uint32_t previous_index;
    uint32_t index;
    uint32_t next_deck_instance_id;
    uint32_t next_modifier_id;
    uint32_t previous_player_id;
    uint32_t target_index;
    ft_bool target_found;
    ft_bool phase_found;

    if (this->_initialised_state != 2U
        || snapshot.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || snapshot.player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.event_count > FT_CARD_GAME_MAX_EVENTS
        || (snapshot.event_count != 0U && snapshot.events == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.random_state == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.next_deck_instance_id == 0U
        || snapshot.next_modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.usage_limits.count > FT_CARD_GAME_MAX_USAGE_LIMITS
        || snapshot.usage_limits.capacity < snapshot.usage_limits.count
        || snapshot.usage_limits.next_id == 0U
        || (snapshot.usage_limits.count != 0U
            && snapshot.usage_limits.limits == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.resources.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || snapshot.resources.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || snapshot.resources.next_unit_id == 0U
        || (snapshot.resources.pool_count != 0U
            && snapshot.resources.pools == ft_nullptr)
        || (snapshot.resources.unit_count != 0U
            && snapshot.resources.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.allowances.count > FT_CARD_GAME_MAX_ALLOWANCES
        || snapshot.allowances.next_id == 0U
        || (snapshot.allowances.count != 0U
            && snapshot.allowances.allowances == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.choices.count > FT_CARD_GAME_MAX_CHOICES
        || snapshot.choices.next_id == 0U
        || (snapshot.choices.count != 0U
            && snapshot.choices.choices == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.modifier_count > FT_CARD_GAME_MAX_MODIFIERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.modifier_count)
    {
        target_found = FT_FALSE;
        target_index = 0U;
        if (snapshot.modifiers[index].target_player_id < snapshot.player_count)
        {
            while (target_index < snapshot.players[snapshot.modifiers[index]
                .target_player_id].board_count)
            {
                if ((snapshot.modifiers[index].target_instance_id != 0U
                    && snapshot.players[snapshot.modifiers[index]
                        .target_player_id].instances[target_index].instance_id
                        == snapshot.modifiers[index].target_instance_id)
                    || (snapshot.modifiers[index].target_instance_id == 0U
                        && target_index
                            == snapshot.modifiers[index].target_instance_index))
                {
                    target_found = FT_TRUE;
                    break ;
                }
                target_index += 1U;
            }
        }
        if (snapshot.modifiers[index].modifier_id == 0U
            || target_found == FT_FALSE
            || (snapshot.modifiers[index].duration
                != CARD_GAME_MODIFIER_PERMANENT
                && snapshot.modifiers[index].duration
                    != CARD_GAME_MODIFIER_UNTIL_END_TURN))
            return (FT_ERR_INVALID_ARGUMENT);
        previous_index = 0U;
        while (previous_index < index)
        {
            if (snapshot.modifiers[previous_index].modifier_id
                == snapshot.modifiers[index].modifier_id)
                return (FT_ERR_INVALID_ARGUMENT);
            previous_index += 1U;
        }
        index += 1U;
    }
    if (snapshot.player_count == 0U)
    {
        if (snapshot.turn_number != 0U || snapshot.active_player != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else if (snapshot.active_player >= snapshot.player_count
        || snapshot.turn_number == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.current_phase_id != 0U)
    {
        phase_found = FT_FALSE;
        phase_index = 0U;
        while (phase_index < this->_phase_count)
        {
            if (this->_phases[phase_index].phase_id
                == snapshot.current_phase_id)
                phase_found = FT_TRUE;
            phase_index += 1U;
        }
        if (phase_found == FT_FALSE)
            return (FT_ERR_NOT_FOUND);
    }
    player_id = 0U;
    while (player_id < snapshot.player_count)
    {
        if (snapshot.players[player_id].mana > this->_rules.max_mana
            || card_game_validate_player_snapshot(snapshot.players[player_id],
                player_id, this->_rules.max_board_spaces,
                this->_rules.max_hand_size) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        deck_index = 0U;
        while (deck_index < snapshot.players[player_id].deck_count)
        {
            if (this->is_card_registered(
                    snapshot.players[player_id].deck[deck_index]) == FT_FALSE)
                return (FT_ERR_NOT_FOUND);
            instance_id = snapshot.players[player_id]
                .deck_instance_ids[deck_index];
            if (instance_id == 0U)
                instance_id = snapshot.players[player_id].deck[deck_index];
            previous_index = 0U;
            while (previous_index < deck_index)
            {
                uint32_t previous_instance_id;

                previous_instance_id = snapshot.players[player_id]
                    .deck_instance_ids[previous_index];
                if (previous_instance_id == 0U)
                    previous_instance_id = snapshot.players[player_id]
                        .deck[previous_index];
                if (previous_instance_id == instance_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                previous_index += 1U;
            }
            previous_player_id = 0U;
            while (previous_player_id < player_id)
            {
                uint32_t previous_deck_index;

                previous_deck_index = 0U;
                while (previous_deck_index
                    < snapshot.players[previous_player_id].deck_count)
                {
                    uint32_t previous_instance_id;

                    previous_instance_id = snapshot.players[previous_player_id]
                        .deck_instance_ids[previous_deck_index];
                    if (previous_instance_id == 0U)
                        previous_instance_id = snapshot.players[previous_player_id]
                            .deck[previous_deck_index];
                    if (previous_instance_id == instance_id)
                        return (FT_ERR_INVALID_ARGUMENT);
                    previous_deck_index += 1U;
                }
                previous_player_id += 1U;
            }
            deck_index += 1U;
        }
        hand_index = 0U;
        while (hand_index < snapshot.players[player_id].hand_count)
        {
            if (this->is_card_registered(
                    snapshot.players[player_id].hand[hand_index]) == FT_FALSE
                || snapshot.players[player_id]
                    .hand_instance_ids[hand_index] == 0U)
                return (FT_ERR_NOT_FOUND);
            previous_index = 0U;
            while (previous_index < hand_index)
            {
                if (snapshot.players[player_id]
                        .hand_instance_ids[previous_index]
                    == snapshot.players[player_id]
                        .hand_instance_ids[hand_index])
                    return (FT_ERR_INVALID_ARGUMENT);
                previous_index += 1U;
            }
            hand_index += 1U;
        }
        player_id += 1U;
    }
    next_deck_instance_id = snapshot.next_deck_instance_id;
    next_modifier_id = snapshot.next_modifier_id;
    if (this->_event_capacity < snapshot.event_count)
    {
        while (this->_event_capacity < snapshot.event_count)
        {
            if (this->grow_events() != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
        }
    }
    if (this->_usage_limits.apply_snapshot(snapshot.usage_limits)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_zone_store.apply_snapshot(snapshot.zones)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_resources.apply_snapshot(snapshot.resources)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_allowances.apply_snapshot(snapshot.allowances)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_choices.apply_snapshot(snapshot.choices)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.event_count != 0U)
        ft_memcpy(this->_events, snapshot.events,
            static_cast<ft_size_t>(snapshot.event_count)
                * sizeof(card_game_event));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        ft_memcpy(this->_board[player_id], snapshot.players[player_id].board,
            sizeof(this->_board[player_id]));
        ft_memcpy(this->_instances[player_id],
            snapshot.players[player_id].instances,
            sizeof(this->_instances[player_id]));
        this->_board_count[player_id] = snapshot.players[player_id].board_count;
        if (this->_decks[player_id].clear() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        deck_index = 0U;
        while (deck_index < snapshot.players[player_id].deck_count)
        {
            card_game_zone_entry entry;

            entry.card_id = snapshot.players[player_id].deck[deck_index];
            entry.instance_id = snapshot.players[player_id]
                .deck_instance_ids[deck_index];
            if (entry.instance_id == 0U)
                entry.instance_id = entry.card_id;
            if (this->_decks[player_id].push_bottom_entry(entry)
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            if (entry.instance_id >= next_deck_instance_id
                && entry.instance_id != UINT32_MAX)
                next_deck_instance_id = entry.instance_id + 1U;
            deck_index += 1U;
        }
        this->_hand_count[player_id] = snapshot.players[player_id].hand_count;
        hand_index = 0U;
        while (hand_index < this->_hand_count[player_id])
        {
            this->_hand[player_id][hand_index].card_id =
                snapshot.players[player_id].hand[hand_index];
            this->_hand[player_id][hand_index].instance_id =
                snapshot.players[player_id].hand_instance_ids[hand_index];
            if (this->_hand[player_id][hand_index].instance_id
                >= next_deck_instance_id
                && this->_hand[player_id][hand_index].instance_id != UINT32_MAX)
                next_deck_instance_id =
                    this->_hand[player_id][hand_index].instance_id + 1U;
            hand_index += 1U;
        }
        this->_health[player_id] = snapshot.players[player_id].health;
        this->_mana[player_id] = snapshot.players[player_id].mana;
        player_id += 1U;
    }
    this->_player_count = snapshot.player_count;
    this->_turn_number = snapshot.turn_number;
    this->_active_player = snapshot.active_player;
    this->_current_phase_id = snapshot.current_phase_id;
    this->_event_count = snapshot.event_count;
    this->_event_sequence = snapshot.event_sequence;
    this->_random_state = snapshot.random_state;
    this->_modifier_count = snapshot.modifier_count;
    ft_memcpy(this->_modifiers, snapshot.modifiers,
        sizeof(this->_modifiers));
    this->_next_deck_instance_id = next_deck_instance_id;
    this->_next_modifier_id = next_modifier_id;
    this->_state_sequence = snapshot.state_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_snapshot(
    const card_game_snapshot &snapshot) noexcept
{
    card_game_snapshot before_state;
    int32_t result;
    int32_t restore_error;

    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->apply_snapshot_internal(snapshot);
    if (result != FT_ERR_SUCCESS)
    {
        restore_error = this->apply_snapshot_internal(before_state);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
    }
    return (result);
}

int32_t card_game_engine::create_delta(const card_game_snapshot &baseline,
    card_game_delta *delta) const noexcept
{
    card_game_snapshot current_snapshot;
    card_game_delta candidate;
    uint32_t player_id;
    uint32_t event_bytes;
    int32_t result;

    if (this->_initialised_state != 2U || delta == ft_nullptr
        || baseline.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || baseline.player_count != this->_player_count)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->get_snapshot(&current_snapshot);
    if (result != FT_ERR_SUCCESS)
        return (result);
    event_bytes = current_snapshot.event_count * sizeof(card_game_event);
    if (event_bytes != 0U)
    {
        candidate.events = static_cast<card_game_event *>(cma_malloc(
            event_bytes));
        if (candidate.events == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(candidate.events, current_snapshot.events, event_bytes);
    }
    candidate.format_version = FT_CARD_GAME_STATE_FORMAT_VERSION;
    candidate.base_state_sequence = baseline.state_sequence;
    candidate.target_state_sequence = current_snapshot.state_sequence;
    candidate.player_count = current_snapshot.player_count;
    if (baseline.turn_number != current_snapshot.turn_number
        || baseline.active_player != current_snapshot.active_player
        || baseline.current_phase_id != current_snapshot.current_phase_id)
        candidate.global_state_changed = FT_TRUE;
    candidate.turn_number = current_snapshot.turn_number;
    candidate.active_player = current_snapshot.active_player;
    candidate.current_phase_id = current_snapshot.current_phase_id;
    candidate.event_count = current_snapshot.event_count;
    candidate.event_sequence = current_snapshot.event_sequence;
    candidate.random_state = current_snapshot.random_state;
    candidate.next_deck_instance_id = current_snapshot.next_deck_instance_id;
    candidate.next_modifier_id = current_snapshot.next_modifier_id;
    candidate.modifier_count = current_snapshot.modifier_count;
    candidate.event_capacity = current_snapshot.event_count;
    if (card_game_zone_store::clone_snapshot(current_snapshot.zones,
            &candidate.zones) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (card_game_resource_ledger::clone_snapshot(current_snapshot.resources,
            &candidate.resources) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (card_game_allowance_ledger::clone_snapshot(
            current_snapshot.allowances, &candidate.allowances)
        != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (card_game_choice_ledger::clone_snapshot(current_snapshot.choices,
            &candidate.choices) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (card_game_usage_limit_ledger::clone_snapshot(
            current_snapshot.usage_limits, &candidate.usage_limits)
        != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    ft_memcpy(candidate.modifiers, current_snapshot.modifiers,
        sizeof(candidate.modifiers));
    if (baseline.event_count != current_snapshot.event_count
        || baseline.event_sequence != current_snapshot.event_sequence
        || (baseline.event_count != 0U
            && (baseline.events == ft_nullptr
                || ft_memcmp(baseline.events, current_snapshot.events,
                    static_cast<ft_size_t>(baseline.event_count)
                        * sizeof(card_game_event)) != 0)))
        candidate.global_state_changed = FT_TRUE;
    if (baseline.random_state != current_snapshot.random_state)
        candidate.global_state_changed = FT_TRUE;
    if (baseline.next_deck_instance_id
            != current_snapshot.next_deck_instance_id
        || baseline.next_modifier_id != current_snapshot.next_modifier_id)
        candidate.global_state_changed = FT_TRUE;
    if (baseline.modifier_count != current_snapshot.modifier_count
        || ft_memcmp(baseline.modifiers, current_snapshot.modifiers,
            sizeof(baseline.modifiers)) != 0)
        candidate.global_state_changed = FT_TRUE;
    if (card_game_zone_store::snapshots_equal(baseline.zones,
            current_snapshot.zones) == FT_FALSE)
        candidate.global_state_changed = FT_TRUE;
    if (card_game_resource_ledger::snapshots_equal(baseline.resources,
            current_snapshot.resources) == FT_FALSE)
        candidate.global_state_changed = FT_TRUE;
    if (card_game_allowance_ledger::snapshots_equal(baseline.allowances,
            current_snapshot.allowances) == FT_FALSE)
        candidate.global_state_changed = FT_TRUE;
    if (card_game_choice_ledger::snapshots_equal(baseline.choices,
            current_snapshot.choices) == FT_FALSE)
        candidate.global_state_changed = FT_TRUE;
    if (card_game_usage_limit_ledger::snapshots_equal(
            baseline.usage_limits, current_snapshot.usage_limits) == FT_FALSE)
        candidate.global_state_changed = FT_TRUE;
    player_id = 0U;
    while (player_id < current_snapshot.player_count)
    {
        if (card_game_player_snapshots_equal(baseline.players[player_id],
                current_snapshot.players[player_id]) == FT_FALSE)
        {
            candidate.changed_player_mask |= (static_cast<uint64_t>(1U)
                << player_id);
            candidate.players[player_id] = current_snapshot.players[player_id];
        }
        player_id += 1U;
    }
    if (delta->events != ft_nullptr)
        cma_free(delta->events);
    (void)card_game_zone_store::release_snapshot(&delta->zones);
    (void)card_game_resource_ledger::release_snapshot(&delta->resources);
    (void)card_game_allowance_ledger::release_snapshot(&delta->allowances);
    (void)card_game_choice_ledger::release_snapshot(&delta->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(
        &delta->usage_limits);
    delta->format_version = candidate.format_version;
    delta->base_state_sequence = candidate.base_state_sequence;
    delta->target_state_sequence = candidate.target_state_sequence;
    delta->changed_player_mask = candidate.changed_player_mask;
    delta->global_state_changed = candidate.global_state_changed;
    delta->player_count = candidate.player_count;
    delta->turn_number = candidate.turn_number;
    delta->active_player = candidate.active_player;
    delta->current_phase_id = candidate.current_phase_id;
    delta->event_count = candidate.event_count;
    delta->event_sequence = candidate.event_sequence;
    delta->random_state = candidate.random_state;
    delta->next_deck_instance_id = candidate.next_deck_instance_id;
    delta->next_modifier_id = candidate.next_modifier_id;
    delta->modifier_count = candidate.modifier_count;
    delta->event_capacity = candidate.event_capacity;
    delta->events = candidate.events;
    ft_memcpy(delta->modifiers, candidate.modifiers,
        sizeof(delta->modifiers));
    ft_memcpy(delta->players, candidate.players, sizeof(delta->players));
    delta->zones = candidate.zones;
    delta->resources = candidate.resources;
    delta->allowances = candidate.allowances;
    delta->choices = candidate.choices;
    delta->usage_limits = candidate.usage_limits;
    candidate.events = ft_nullptr;
    ft_bzero(&candidate.zones, sizeof(candidate.zones));
    ft_bzero(&candidate.resources, sizeof(candidate.resources));
    ft_bzero(&candidate.allowances, sizeof(candidate.allowances));
    ft_bzero(&candidate.choices, sizeof(candidate.choices));
    ft_bzero(&candidate.usage_limits, sizeof(candidate.usage_limits));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_delta_internal(
    const card_game_delta &delta) noexcept
{
    uint32_t player_id;
    uint32_t deck_index;
    uint32_t hand_index;
    uint32_t next_deck_instance_id;
    uint32_t next_modifier_id;
    uint32_t target_index;
    card_game_zone_store_snapshot current_zones;
    card_game_resource_snapshot current_resources;
    card_game_allowance_snapshot current_allowances;
    card_game_choice_snapshot current_choices;
    card_game_usage_limit_snapshot current_usage_limits;
    ft_bool zones_match;
    ft_bool resources_match;
    ft_bool allowances_match;
    ft_bool choices_match;
    ft_bool usage_limits_match;
    ft_bool target_found;
    int32_t release_error;

    ft_bzero(&current_zones, sizeof(current_zones));
    ft_bzero(&current_resources, sizeof(current_resources));
    ft_bzero(&current_allowances, sizeof(current_allowances));
    ft_bzero(&current_choices, sizeof(current_choices));
    ft_bzero(&current_usage_limits, sizeof(current_usage_limits));

    if (this->_initialised_state != 2U
        || delta.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || delta.base_state_sequence != this->_state_sequence
        || delta.target_state_sequence < delta.base_state_sequence
        || delta.player_count != this->_player_count
        || delta.event_count > FT_CARD_GAME_MAX_EVENTS
        || (delta.event_count != 0U && delta.events == ft_nullptr)
        || (delta.changed_player_mask >> FT_CARD_GAME_MAX_PLAYERS) != 0U
        || (delta.global_state_changed != FT_FALSE
            && delta.global_state_changed != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.player_count != 0U
        && (delta.active_player >= delta.player_count
            || delta.turn_number == 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.modifier_count > FT_CARD_GAME_MAX_MODIFIERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.next_deck_instance_id == 0U
        || delta.next_modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.global_state_changed == FT_FALSE
        && (delta.modifier_count != this->_modifier_count
            || delta.next_deck_instance_id != this->_next_deck_instance_id
            || delta.next_modifier_id != this->_next_modifier_id
            || ft_memcmp(delta.modifiers, this->_modifiers,
                sizeof(this->_modifiers)) != 0))
        return (FT_ERR_INVALID_ARGUMENT);
    zones_match = FT_FALSE;
    if (this->_zone_store.get_snapshot(&current_zones) == FT_ERR_SUCCESS)
    {
        zones_match = card_game_zone_store::snapshots_equal(
            current_zones, delta.zones);
        release_error = card_game_zone_store::release_snapshot(&current_zones);
        if (release_error != FT_ERR_SUCCESS)
            return (release_error);
    }
    else
        return (FT_ERR_INVALID_STATE);
    if (delta.global_state_changed == FT_FALSE
        && zones_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_resources.get_snapshot(&current_resources)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    resources_match = card_game_resource_ledger::snapshots_equal(
        current_resources, delta.resources);
    release_error = card_game_resource_ledger::release_snapshot(
        &current_resources);
    if (release_error != FT_ERR_SUCCESS)
        return (release_error);
    if (delta.global_state_changed == FT_FALSE
        && resources_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_allowances.get_snapshot(&current_allowances)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    allowances_match = card_game_allowance_ledger::snapshots_equal(
        current_allowances, delta.allowances);
    release_error = card_game_allowance_ledger::release_snapshot(
        &current_allowances);
    if (release_error != FT_ERR_SUCCESS)
        return (release_error);
    if (delta.global_state_changed == FT_FALSE
        && allowances_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_choices.get_snapshot(&current_choices)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    choices_match = card_game_choice_ledger::snapshots_equal(
        current_choices, delta.choices);
    release_error = card_game_choice_ledger::release_snapshot(&current_choices);
    if (release_error != FT_ERR_SUCCESS)
        return (release_error);
    if (delta.global_state_changed == FT_FALSE
        && choices_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_usage_limits.get_snapshot(&current_usage_limits)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    usage_limits_match = card_game_usage_limit_ledger::snapshots_equal(
        current_usage_limits, delta.usage_limits);
    release_error = card_game_usage_limit_ledger::release_snapshot(
        &current_usage_limits);
    if (release_error != FT_ERR_SUCCESS)
        return (release_error);
    if (delta.global_state_changed == FT_FALSE
        && usage_limits_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    player_id = 0U;
    while (player_id < delta.modifier_count)
    {
        uint32_t target_player_id;
        uint32_t previous_modifier_index;
        uint32_t target_board_count;
        const card_game_card_instance *target_instances;

        target_player_id = delta.modifiers[player_id].target_player_id;
        target_index = 0U;
        target_found = FT_FALSE;
        if (target_player_id < delta.player_count)
        {
            if ((delta.changed_player_mask & (static_cast<uint64_t>(1U)
                    << target_player_id)) != 0U)
            {
                target_board_count =
                    delta.players[target_player_id].board_count;
                target_instances =
                    delta.players[target_player_id].instances;
            }
            else
            {
                target_board_count = this->_board_count[target_player_id];
                target_instances = this->_instances[target_player_id];
            }
            while (target_index < target_board_count)
            {
                if ((delta.modifiers[player_id].target_instance_id != 0U
                    && target_instances[target_index].instance_id
                        == delta.modifiers[player_id].target_instance_id)
                    || (delta.modifiers[player_id].target_instance_id == 0U
                        && target_index
                            == delta.modifiers[player_id].target_instance_index))
                {
                    target_found = FT_TRUE;
                    break ;
                }
                target_index += 1U;
            }
        }
        if (delta.modifiers[player_id].modifier_id == 0U
            || target_found == FT_FALSE
            || (delta.modifiers[player_id].duration
                != CARD_GAME_MODIFIER_PERMANENT
                && delta.modifiers[player_id].duration
                    != CARD_GAME_MODIFIER_UNTIL_END_TURN))
            return (FT_ERR_INVALID_ARGUMENT);
        if (target_found == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        previous_modifier_index = 0U;
        while (previous_modifier_index < player_id)
        {
            if (delta.modifiers[previous_modifier_index].modifier_id
                == delta.modifiers[player_id].modifier_id)
                return (FT_ERR_INVALID_ARGUMENT);
            previous_modifier_index += 1U;
        }
        player_id += 1U;
    }
    player_id = 0U;
    while (player_id < delta.player_count)
    {
        if ((delta.changed_player_mask & (static_cast<uint64_t>(1U)
                << player_id)) != 0U
            && (delta.players[player_id].mana > this->_rules.max_mana
                || card_game_validate_player_snapshot(delta.players[player_id],
                    player_id, this->_rules.max_board_spaces,
                    this->_rules.max_hand_size)
                    != FT_ERR_SUCCESS))
            return (FT_ERR_INVALID_ARGUMENT);
        deck_index = 0U;
        while (deck_index < delta.players[player_id].deck_count)
        {
            if (this->is_card_registered(
                    delta.players[player_id].deck[deck_index]) == FT_FALSE)
                return (FT_ERR_NOT_FOUND);
            deck_index += 1U;
        }
        hand_index = 0U;
        while (hand_index < delta.players[player_id].hand_count)
        {
            if (this->is_card_registered(
                    delta.players[player_id].hand[hand_index]) == FT_FALSE
                || delta.players[player_id]
                    .hand_instance_ids[hand_index] == 0U)
                return (FT_ERR_NOT_FOUND);
            hand_index += 1U;
        }
        player_id += 1U;
    }
    if (delta.global_state_changed != FT_FALSE
        && delta.random_state == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.global_state_changed != FT_FALSE)
    {
        if (this->_event_capacity < delta.event_count)
        {
            while (this->_event_capacity < delta.event_count)
            {
                if (this->grow_events() != FT_ERR_SUCCESS)
                    return (FT_ERR_NO_MEMORY);
            }
        }
        if (this->_usage_limits.apply_snapshot(delta.usage_limits)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->_zone_store.apply_snapshot(delta.zones)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->_resources.apply_snapshot(delta.resources)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->_allowances.apply_snapshot(delta.allowances)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (this->_choices.apply_snapshot(delta.choices)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        if (delta.event_count != 0U)
            ft_memcpy(this->_events, delta.events,
                static_cast<ft_size_t>(delta.event_count)
                    * sizeof(card_game_event));
        next_deck_instance_id = delta.next_deck_instance_id;
        next_modifier_id = delta.next_modifier_id;
        this->_turn_number = delta.turn_number;
        this->_active_player = delta.active_player;
        this->_current_phase_id = delta.current_phase_id;
        this->_event_count = delta.event_count;
        this->_event_sequence = delta.event_sequence;
        this->_random_state = delta.random_state;
        this->_modifier_count = delta.modifier_count;
        this->_next_deck_instance_id = next_deck_instance_id;
        this->_next_modifier_id = next_modifier_id;
        ft_memcpy(this->_modifiers, delta.modifiers,
            sizeof(this->_modifiers));
    }
    player_id = 0U;
    while (player_id < delta.player_count)
    {
        if ((delta.changed_player_mask & (static_cast<uint64_t>(1U)
                << player_id)) != 0U)
        {
            ft_memcpy(this->_board[player_id], delta.players[player_id].board,
                sizeof(this->_board[player_id]));
            ft_memcpy(this->_instances[player_id],
                delta.players[player_id].instances,
                sizeof(this->_instances[player_id]));
            this->_board_count[player_id] = delta.players[player_id].board_count;
            if (this->_decks[player_id].clear() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            deck_index = 0U;
            while (deck_index < delta.players[player_id].deck_count)
            {
                card_game_zone_entry entry;

                entry.card_id = delta.players[player_id].deck[deck_index];
                entry.instance_id = delta.players[player_id]
                    .deck_instance_ids[deck_index];
                if (entry.instance_id == 0U)
                    entry.instance_id = entry.card_id;
                if (this->_decks[player_id].push_bottom_entry(entry)
                    != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_STATE);
                if (entry.instance_id >= this->_next_deck_instance_id
                    && entry.instance_id != UINT32_MAX)
                    this->_next_deck_instance_id = entry.instance_id + 1U;
                deck_index += 1U;
            }
            this->_hand_count[player_id] =
                delta.players[player_id].hand_count;
            hand_index = 0U;
            while (hand_index < this->_hand_count[player_id])
            {
                this->_hand[player_id][hand_index].card_id =
                    delta.players[player_id].hand[hand_index];
                this->_hand[player_id][hand_index].instance_id =
                    delta.players[player_id]
                        .hand_instance_ids[hand_index];
                if (this->_hand[player_id][hand_index].instance_id
                    >= this->_next_deck_instance_id
                    && this->_hand[player_id][hand_index].instance_id
                        != UINT32_MAX)
                    this->_next_deck_instance_id =
                        this->_hand[player_id][hand_index].instance_id + 1U;
                hand_index += 1U;
            }
            this->_health[player_id] = delta.players[player_id].health;
            this->_mana[player_id] = delta.players[player_id].mana;
        }
        player_id += 1U;
    }
    this->_state_sequence = delta.target_state_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_delta(const card_game_delta &delta) noexcept
{
    card_game_snapshot before_state;
    int32_t result;
    int32_t restore_error;

    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->apply_delta_internal(delta);
    if (result != FT_ERR_SUCCESS)
    {
        restore_error = this->apply_snapshot_internal(before_state);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
    }
    return (result);
}

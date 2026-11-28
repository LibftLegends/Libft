#include "card_game_internal.hpp"

static ft_bool card_game_host_intents_equal(
    const card_game_host_intent_record &left,
    const card_game_host_intent_record &right) noexcept
{
    uint32_t field_index;

    if (left.sequence != right.sequence
        || left.action_request_id != right.action_request_id
        || left.intent.type_id != right.intent.type_id
        || left.intent.version != right.intent.version
        || left.intent.field_count != right.intent.field_count)
        return (FT_FALSE);
    field_index = 0U;
    while (field_index < left.intent.field_count
        && field_index < FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
    {
        if (left.intent.fields[field_index].type
                != right.intent.fields[field_index].type
            || left.intent.fields[field_index].value
                != right.intent.fields[field_index].value)
            return (FT_FALSE);
        field_index += 1U;
    }
    return (FT_TRUE);
}

static ft_bool card_game_host_intent_snapshots_equal(
    const card_game_host_intent_snapshot &left,
    uint64_t left_next_sequence,
    const card_game_host_intent_snapshot &right,
    uint64_t right_next_sequence) noexcept
{
    uint32_t left_count;
    uint32_t right_count;
    uint32_t index;
    card_game_host_intent_record left_record;
    card_game_host_intent_record right_record;

    if (left_next_sequence != right_next_sequence
        || left.get_count(&left_count) != FT_ERR_SUCCESS
        || right.get_count(&right_count) != FT_ERR_SUCCESS
        || left_count != right_count)
        return (FT_FALSE);
    index = 0U;
    while (index < left_count)
    {
        if (left.get_record(index, &left_record) != FT_ERR_SUCCESS
            || right.get_record(index, &right_record) != FT_ERR_SUCCESS
            || card_game_host_intents_equal(left_record, right_record)
                == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

static ft_bool card_game_modifier_matches_board(const card_game_card_modifier &modifier,
    const card_game_card_instance *instances, uint32_t count) noexcept
{
    if (modifier.modifier_id == 0U || count > FT_CARD_GAME_MAX_CARDS
        || (modifier.duration != CARD_GAME_MODIFIER_PERMANENT
            && modifier.duration != CARD_GAME_MODIFIER_UNTIL_END_TURN))
        return (FT_FALSE);
    const uint32_t hint = modifier.target_instance_index;
    if (modifier.target_instance_id == 0U)
        return (hint < count);
    if (hint < count && instances[hint].instance_id == modifier.target_instance_id)
        return (FT_TRUE);
    uint32_t index = 0U;
    while (index < count)
    {
        if (instances[index].instance_id == modifier.target_instance_id)
            return (FT_TRUE);
        ++index;
    }
    return (FT_FALSE);
}

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
    uint32_t host_intent_count;
    uint32_t host_intent_index;
    ft_bool phase_found;
    card_game_host_intent_record host_intent_record;
    card_game_host_intent_record previous_host_intent_record;
    int32_t host_intent_error;

    if (this->_initialised_state != 2U
        || snapshot.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || snapshot.player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.event_count > FT_CARD_GAME_MAX_EVENTS
        || (snapshot.event_count != 0U && snapshot.events == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.next_host_intent_sequence == 0U
        || snapshot.host_intents.get_count(&host_intent_count)
            != FT_ERR_SUCCESS
        || host_intent_count > FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_INVALID_ARGUMENT);
    if ((snapshot.pending_discard_count == 0U
            && (snapshot.pending_discard_player_id != 0U
                || snapshot.pending_discard_zone_id != 0U))
        || (snapshot.pending_discard_count != 0U
            && (snapshot.pending_discard_count
                    > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT
                || snapshot.pending_discard_player_id
                    >= snapshot.player_count
                || snapshot.pending_discard_player_id
                    != snapshot.active_player
                || snapshot.pending_discard_zone_id == 0U
                || snapshot.players[snapshot.pending_discard_player_id]
                    .hand_count < snapshot.pending_discard_count
                || this->_draw_policy_configured != FT_TRUE
                || snapshot.pending_discard_zone_id
                    != this->_draw_policy.discard_zone_id)))
        return (FT_ERR_INVALID_ARGUMENT);
    host_intent_index = 0U;
    while (host_intent_index < host_intent_count)
    {
        if (snapshot.host_intents.get_record(host_intent_index,
                &host_intent_record) != FT_ERR_SUCCESS
            || host_intent_record.sequence == 0U
            || host_intent_record.sequence
                >= snapshot.next_host_intent_sequence)
            return (FT_ERR_INVALID_ARGUMENT);
        if (host_intent_index != 0U)
        {
            if (snapshot.host_intents.get_record(host_intent_index - 1U,
                    &previous_host_intent_record) != FT_ERR_SUCCESS
                || host_intent_record.sequence
                    <= previous_host_intent_record.sequence)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        host_intent_error = this->validate_host_intent(
            host_intent_record.intent);
        if (host_intent_error != FT_ERR_SUCCESS)
            return (host_intent_error);
        host_intent_index += 1U;
    }
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
    uint32_t modifier_ids[FT_CARD_GAME_MAX_MODIFIERS];
    index = 0U;
    while (index < snapshot.modifier_count)
    {
        const card_game_card_modifier &modifier = snapshot.modifiers[index];
        if (modifier.target_player_id >= snapshot.player_count)
            return (FT_ERR_INVALID_ARGUMENT);
        const card_game_player_snapshot &player = snapshot.players[modifier.target_player_id];
        if (!card_game_modifier_matches_board(modifier, player.instances, player.board_count))
            return (FT_ERR_INVALID_ARGUMENT);
        modifier_ids[index] = modifier.modifier_id;
        ++index;
    }
    if (!card_game_snapshot_ids_are_unique(modifier_ids, snapshot.modifier_count))
        return (FT_ERR_INVALID_ARGUMENT);
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
    constexpr uint32_t DECK_ID_BUCKETS =
        2U * FT_CARD_GAME_MAX_PLAYERS * FT_CARD_GAME_MAX_CARDS;
    static_assert((DECK_ID_BUCKETS & (DECK_ID_BUCKETS - 1U)) == 0U);
    uint32_t deck_ids[DECK_ID_BUCKETS];
    ft_bool deck_ids_initialized = FT_FALSE;
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
            if (!deck_ids_initialized)
            {
                ft_bzero(deck_ids, sizeof(deck_ids));
                deck_ids_initialized = FT_TRUE;
            }
            uint32_t hash = instance_id ^ (instance_id >> 16U);
            hash *= 0x7FEB352DU;
            hash ^= hash >> 15U;
            constexpr uint32_t BUCKET_MASK = DECK_ID_BUCKETS - 1U;
            uint32_t bucket = hash & BUCKET_MASK;
            while (deck_ids[bucket] != 0U)
            {
                if (deck_ids[bucket] == instance_id)
                    return (FT_ERR_INVALID_ARGUMENT);
                bucket = (bucket + 1U) & BUCKET_MASK;
            }
            deck_ids[bucket] = instance_id;
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
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        const card_game_player_snapshot &player = snapshot.players[player_id];
        if (player.board_count > FT_CARD_GAME_MAX_CARDS
            || player.deck_count > FT_CARD_GAME_MAX_CARDS
            || player.hand_count > FT_CARD_GAME_MAX_CARDS)
            return (FT_ERR_INVALID_ARGUMENT);
        ++player_id;
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
    if (host_intent_count > this->_host_intent_record_capacity)
    {
        int32_t reserve_error;

        reserve_error = this->reserve_host_intent_records(host_intent_count);
        if (reserve_error != FT_ERR_SUCCESS)
            return (reserve_error);
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
        const card_game_player_snapshot &player = snapshot.players[player_id];
        if (player_id < snapshot.player_count)
        {
            card_game_zone_entry entries[FT_CARD_GAME_MAX_CARDS];
            deck_index = 0U;
            while (deck_index < player.deck_count)
            {
                entries[deck_index].card_id = player.deck[deck_index];
                entries[deck_index].instance_id = player.deck_instance_ids[deck_index];
                if (entries[deck_index].instance_id == 0U)
                    entries[deck_index].instance_id = entries[deck_index].card_id;
                ++deck_index;
            }
            if (this->_decks[player_id].replace_entries(entries, player.deck_count)
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
        }
        else
        {
            if (this->_decks[player_id].clear() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            deck_index = 0U;
            while (deck_index < player.deck_count)
            {
                card_game_zone_entry entry{player.deck_instance_ids[deck_index],
                    player.deck[deck_index]};
                if (entry.instance_id == 0U)
                    entry.instance_id = entry.card_id;
                if (this->_decks[player_id].push_bottom_entry(entry) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_STATE);
                ++deck_index;
            }
        }
        this->_hand_count[player_id] = snapshot.players[player_id].hand_count;
        hand_index = 0U;
        while (hand_index < this->_hand_count[player_id])
        {
            this->_hand[player_id][hand_index].card_id =
                snapshot.players[player_id].hand[hand_index];
            this->_hand[player_id][hand_index].instance_id =
                snapshot.players[player_id].hand_instance_ids[hand_index];
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
    this->_host_intent_record_count = host_intent_count;
    this->_host_intent_record_head = 0U;
    host_intent_index = 0U;
    while (host_intent_index < host_intent_count)
    {
        if (snapshot.host_intents.get_record(host_intent_index,
                &this->_host_intent_records[host_intent_index])
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        host_intent_index += 1U;
    }
    this->_next_host_intent_sequence = snapshot.next_host_intent_sequence;
    this->_pending_discard_player_id =
        snapshot.pending_discard_player_id;
    this->_pending_discard_count = snapshot.pending_discard_count;
    this->_pending_discard_zone_id = snapshot.pending_discard_zone_id;
    this->_random_state = snapshot.random_state;
    this->_modifier_count = snapshot.modifier_count;
    ft_memcpy(this->_modifiers, snapshot.modifiers,
        sizeof(this->_modifiers));
    this->_next_deck_instance_id = next_deck_instance_id;
    this->_next_modifier_id = next_modifier_id;
    this->_state_sequence = snapshot.state_sequence;
    return (this->validate_live_card_copy_limits());
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
    uint32_t player_id;
    uint32_t baseline_host_intent_count;
    int32_t result;

    if (this->_initialised_state != 2U || delta == ft_nullptr
        || baseline.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || baseline.player_count != this->_player_count
        || baseline.next_host_intent_sequence == 0U
        || baseline.host_intents.get_count(&baseline_host_intent_count)
            != FT_ERR_SUCCESS
        || baseline_host_intent_count > FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->get_snapshot(&current_snapshot);
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_bool global_state_changed = FT_FALSE;
    if (baseline.turn_number != current_snapshot.turn_number
        || baseline.active_player != current_snapshot.active_player
        || baseline.current_phase_id != current_snapshot.current_phase_id)
        global_state_changed = FT_TRUE;

    if (baseline.event_count != current_snapshot.event_count
        || baseline.event_sequence != current_snapshot.event_sequence
        || (baseline.event_count != 0U
            && (baseline.events == ft_nullptr
                || ft_memcmp(baseline.events, current_snapshot.events,
                    static_cast<ft_size_t>(baseline.event_count)
                        * sizeof(card_game_event)) != 0)))
        global_state_changed = FT_TRUE;
    if (baseline.random_state != current_snapshot.random_state)
        global_state_changed = FT_TRUE;
    if (card_game_host_intent_snapshots_equal(baseline.host_intents,
            baseline.next_host_intent_sequence,
            current_snapshot.host_intents,
            current_snapshot.next_host_intent_sequence) == FT_FALSE)
        global_state_changed = FT_TRUE;
    if (baseline.next_deck_instance_id
            != current_snapshot.next_deck_instance_id
        || baseline.next_modifier_id != current_snapshot.next_modifier_id)
        global_state_changed = FT_TRUE;
    if (baseline.pending_discard_player_id
            != current_snapshot.pending_discard_player_id
        || baseline.pending_discard_count
            != current_snapshot.pending_discard_count
        || baseline.pending_discard_zone_id
            != current_snapshot.pending_discard_zone_id)
        global_state_changed = FT_TRUE;
    if (baseline.modifier_count != current_snapshot.modifier_count
        || baseline.modifier_count > FT_CARD_GAME_MAX_MODIFIERS
        || ft_memcmp(baseline.modifiers, current_snapshot.modifiers,
            baseline.modifier_count * sizeof(*baseline.modifiers)) != 0)
        global_state_changed = FT_TRUE;
    if (card_game_zone_store::snapshots_equal(baseline.zones,
            current_snapshot.zones) == FT_FALSE)
        global_state_changed = FT_TRUE;
    if (card_game_resource_ledger::snapshots_equal(baseline.resources,
            current_snapshot.resources) == FT_FALSE)
        global_state_changed = FT_TRUE;
    if (card_game_allowance_ledger::snapshots_equal(baseline.allowances,
            current_snapshot.allowances) == FT_FALSE)
        global_state_changed = FT_TRUE;
    if (card_game_choice_ledger::snapshots_equal(baseline.choices,
            current_snapshot.choices) == FT_FALSE)
        global_state_changed = FT_TRUE;
    if (card_game_usage_limit_ledger::snapshots_equal(
            baseline.usage_limits, current_snapshot.usage_limits) == FT_FALSE)
        global_state_changed = FT_TRUE;
    uint64_t changed_player_mask = 0U;
    player_id = 0U;
    while (player_id < current_snapshot.player_count)
    {
        if (card_game_player_snapshots_equal(baseline.players[player_id],
                current_snapshot.players[player_id]) == FT_FALSE)
            changed_player_mask |= UINT64_C(1) << player_id;
        player_id += 1U;
    }
    result = delta->host_intents.move(current_snapshot.host_intents);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (delta->events != ft_nullptr)
        cma_free(delta->events);
    (void)card_game_zone_store::release_snapshot(&delta->zones);
    (void)card_game_resource_ledger::release_snapshot(&delta->resources);
    (void)card_game_allowance_ledger::release_snapshot(&delta->allowances);
    (void)card_game_choice_ledger::release_snapshot(&delta->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(&delta->usage_limits);
    delta->format_version = current_snapshot.format_version;
    delta->base_state_sequence = baseline.state_sequence;
    delta->target_state_sequence = current_snapshot.state_sequence;
    delta->changed_player_mask = changed_player_mask;
    delta->global_state_changed = global_state_changed;
    delta->player_count = current_snapshot.player_count;
    delta->turn_number = current_snapshot.turn_number;
    delta->active_player = current_snapshot.active_player;
    delta->current_phase_id = current_snapshot.current_phase_id;
    delta->event_count = current_snapshot.event_count;
    delta->event_sequence = current_snapshot.event_sequence;
    delta->random_state = current_snapshot.random_state;
    delta->next_deck_instance_id = current_snapshot.next_deck_instance_id;
    delta->next_modifier_id = current_snapshot.next_modifier_id;
    delta->modifier_count = current_snapshot.modifier_count;
    delta->event_capacity = current_snapshot.event_capacity;
    delta->events = current_snapshot.events;
    delta->next_host_intent_sequence = current_snapshot.next_host_intent_sequence;
    delta->pending_discard_player_id = current_snapshot.pending_discard_player_id;
    delta->pending_discard_count = current_snapshot.pending_discard_count;
    delta->pending_discard_zone_id = current_snapshot.pending_discard_zone_id;
    ft_memcpy(delta->modifiers, current_snapshot.modifiers, sizeof(delta->modifiers));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if ((changed_player_mask & (UINT64_C(1) << player_id)) != 0U)
            delta->players[player_id] = current_snapshot.players[player_id];
        else
            ft_bzero(&delta->players[player_id], sizeof(delta->players[player_id]));
        player_id += 1U;
    }
    delta->zones = current_snapshot.zones;
    delta->resources = current_snapshot.resources;
    delta->allowances = current_snapshot.allowances;
    delta->choices = current_snapshot.choices;
    delta->usage_limits = current_snapshot.usage_limits;
    current_snapshot.events = ft_nullptr;
    current_snapshot.zones.entries = ft_nullptr;
    current_snapshot.resources.pools = ft_nullptr;
    current_snapshot.resources.units = ft_nullptr;
    current_snapshot.allowances.allowances = ft_nullptr;
    current_snapshot.choices.choices = ft_nullptr;
    current_snapshot.usage_limits.limits = ft_nullptr;
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
    uint32_t host_intent_count;
    uint32_t host_intent_index;
    ft_bool host_intents_match;
    card_game_host_intent_record host_intent_record;
    card_game_host_intent_record previous_host_intent_record;
    int32_t host_intent_error;


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
    if (delta.next_host_intent_sequence == 0U
        || delta.host_intents.get_count(&host_intent_count)
            != FT_ERR_SUCCESS
        || host_intent_count > FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_INVALID_ARGUMENT);
    if ((delta.pending_discard_count == 0U
            && (delta.pending_discard_player_id != 0U
                || delta.pending_discard_zone_id != 0U))
        || (delta.pending_discard_count != 0U
            && (delta.pending_discard_count
                    > FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT
                || delta.pending_discard_player_id >= delta.player_count
                || delta.pending_discard_player_id != delta.active_player
                || delta.pending_discard_zone_id == 0U
                || this->_draw_policy_configured != FT_TRUE
                || delta.pending_discard_zone_id
                    != this->_draw_policy.discard_zone_id)))
        return (FT_ERR_INVALID_ARGUMENT);
    host_intent_index = 0U;
    while (host_intent_index < host_intent_count)
    {
        if (delta.host_intents.get_record(host_intent_index,
                &host_intent_record) != FT_ERR_SUCCESS
            || host_intent_record.sequence == 0U
            || host_intent_record.sequence
                >= delta.next_host_intent_sequence)
            return (FT_ERR_INVALID_ARGUMENT);
        if (host_intent_index != 0U)
        {
            if (delta.host_intents.get_record(host_intent_index - 1U,
                    &previous_host_intent_record) != FT_ERR_SUCCESS
                || host_intent_record.sequence
                    <= previous_host_intent_record.sequence)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        host_intent_error = this->validate_host_intent(
            host_intent_record.intent);
        if (host_intent_error != FT_ERR_SUCCESS)
            return (host_intent_error);
        host_intent_index += 1U;
    }
    host_intents_match = FT_FALSE;
    if (delta.global_state_changed == FT_FALSE
        && host_intent_count == this->_host_intent_record_count
        && delta.next_host_intent_sequence
            == this->_next_host_intent_sequence)
    {
        host_intents_match = FT_TRUE;
        host_intent_index = 0U;
        while (host_intent_index < host_intent_count)
        {
            if (delta.host_intents.get_record(host_intent_index,
                    &host_intent_record) != FT_ERR_SUCCESS
                || card_game_host_intents_equal(host_intent_record,
                    this->_host_intent_records[
                        this->host_intent_record_index(host_intent_index)])
                    == FT_FALSE)
            {
                host_intents_match = FT_FALSE;
                break ;
            }
            host_intent_index += 1U;
        }
    }
    if (delta.global_state_changed == FT_FALSE
        && host_intents_match == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.global_state_changed == FT_FALSE
        && (delta.pending_discard_player_id
                != this->_pending_discard_player_id
            || delta.pending_discard_count != this->_pending_discard_count
            || delta.pending_discard_zone_id != this->_pending_discard_zone_id))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.player_count != 0U
        && (delta.active_player >= delta.player_count
            || delta.turn_number == 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.pending_discard_count != 0U)
    {
        player_id = delta.pending_discard_player_id;
        if (((delta.changed_player_mask
                    & (static_cast<uint64_t>(1U) << player_id)) != 0U
                && delta.players[player_id].hand_count
                    < delta.pending_discard_count)
            || ((delta.changed_player_mask
                    & (static_cast<uint64_t>(1U) << player_id)) == 0U
                && this->_hand_count[player_id]
                    < delta.pending_discard_count))
            return (FT_ERR_INVALID_ARGUMENT);
    }
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
                delta.modifier_count * sizeof(*delta.modifiers)) != 0))
        return (FT_ERR_INVALID_ARGUMENT);
    if (delta.global_state_changed == FT_FALSE
        && (!this->_zone_store.matches_snapshot(delta.zones)
            || !this->_resources.matches_snapshot(delta.resources)
            || !this->_allowances.matches_snapshot(delta.allowances)
            || !this->_choices.matches_snapshot(delta.choices)
            || !this->_usage_limits.matches_snapshot(delta.usage_limits)))
        return (FT_ERR_INVALID_ARGUMENT);

    uint32_t modifier_ids[FT_CARD_GAME_MAX_MODIFIERS];
    uint32_t modifier_index = 0U;
    while (modifier_index < delta.modifier_count)
    {
        const card_game_card_modifier &modifier = delta.modifiers[modifier_index];
        const uint32_t target = modifier.target_player_id;
        if (target >= delta.player_count)
            return (FT_ERR_INVALID_ARGUMENT);
        const card_game_card_instance *instances = this->_instances[target];
        uint32_t count = this->_board_count[target];
        if ((delta.changed_player_mask & (UINT64_C(1) << target)) != 0U)
        {
            instances = delta.players[target].instances;
            count = delta.players[target].board_count;
        }
        if (!card_game_modifier_matches_board(modifier, instances, count))
            return (FT_ERR_INVALID_ARGUMENT);
        modifier_ids[modifier_index] = modifier.modifier_id;
        ++modifier_index;
    }
    if (!card_game_snapshot_ids_are_unique(modifier_ids, delta.modifier_count))
        return (FT_ERR_INVALID_ARGUMENT);
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
        if (delta.players[player_id].deck_count > FT_CARD_GAME_MAX_CARDS
            || delta.players[player_id].hand_count > FT_CARD_GAME_MAX_CARDS)
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
    if (delta.global_state_changed != FT_FALSE
        && host_intent_count > this->_host_intent_record_capacity)
    {
        int32_t reserve_error;

        reserve_error = this->reserve_host_intent_records(host_intent_count);
        if (reserve_error != FT_ERR_SUCCESS)
            return (reserve_error);
    }
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
        this->_host_intent_record_count = host_intent_count;
        this->_host_intent_record_head = 0U;
        host_intent_index = 0U;
        while (host_intent_index < host_intent_count)
        {
            if (delta.host_intents.get_record(host_intent_index,
                    &this->_host_intent_records[host_intent_index])
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            host_intent_index += 1U;
        }
        this->_next_host_intent_sequence =
            delta.next_host_intent_sequence;
        this->_pending_discard_player_id =
            delta.pending_discard_player_id;
        this->_pending_discard_count = delta.pending_discard_count;
        this->_pending_discard_zone_id = delta.pending_discard_zone_id;
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
    const int32_t result = this->validate_live_card_copy_limits();
    if (result == FT_ERR_SUCCESS)
        this->_state_sequence = delta.target_state_sequence;
    return (result);
}

int32_t card_game_engine::apply_delta(const card_game_delta &delta) noexcept
{
    if (delta.global_state_changed == FT_FALSE && delta.changed_player_mask == 0U)
        return (this->apply_delta_internal(delta));

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

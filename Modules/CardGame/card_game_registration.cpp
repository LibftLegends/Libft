#include "card_game_internal.hpp"

int32_t card_game_engine::find_card(uint32_t card_id,
    card_game_card_definition **definition) noexcept
{
    uint32_t index;

    if (definition == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_card_count)
    {
        if (this->_cards[index].card_id == card_id)
        {
            *definition = &this->_cards[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

ft_bool card_game_engine::is_card_registered(uint32_t card_id) const noexcept
{
    uint32_t index;

    index = 0U;
    while (index < this->_card_count)
    {
        if (this->_cards[index].card_id == card_id)
            return (FT_TRUE);
        index += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_engine::find_card_type_id(uint32_t card_id,
    uint32_t *type_id) const noexcept
{
    uint32_t index;

    if (type_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_card_count)
    {
        if (this->_cards[index].card_id == card_id)
        {
            *type_id = this->_card_type_ids[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

uint32_t card_game_engine::get_board_capacity() const noexcept
{
    uint32_t index;
    uint32_t capacity;

    capacity = this->_rules.max_board_spaces;
    index = 0U;
    while (index < this->_zone_count)
    {
        if (this->_zones[index].zone_id == CARD_GAME_BOARD_ZONE_ID
            && this->_zones[index].capacity < capacity)
            capacity = this->_zones[index].capacity;
        index += 1U;
    }
    return (capacity);
}

int32_t card_game_engine::register_card_internal(
    const card_game_card_definition &definition, uint32_t type_id) noexcept
{
    card_game_card_definition *existing;

    if (this->_initialised_state != 2U || definition.card_id == 0U
        || type_id >= FT_CARD_GAME_MAX_CARD_TYPES
        || (definition.effect_id != CARD_GAME_NO_EFFECT
            && definition.effect_id >= this->_effect_count))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card(definition.card_id, &existing) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_card_count >= FT_CARD_GAME_MAX_CARD_DEFINITIONS)
        return (FT_ERR_FULL);
    if (this->_card_count == this->_card_capacity)
    {
        int32_t grow_error = this->grow_card_definitions();

        if (grow_error != FT_ERR_SUCCESS)
            return (grow_error);
    }
    this->_cards[this->_card_count] = definition;
    this->_card_type_ids[this->_card_count] = type_id;
    this->_card_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::grow_card_definitions() noexcept
{
    card_game_card_definition *cards;
    uint32_t *card_type_ids;
    uint32_t capacity;

    if (this->_card_capacity >= FT_CARD_GAME_MAX_CARD_DEFINITIONS)
        return (FT_ERR_FULL);
    capacity = this->_card_capacity * 2U;
    if (capacity < this->_card_capacity
        || capacity > FT_CARD_GAME_MAX_CARD_DEFINITIONS)
        capacity = FT_CARD_GAME_MAX_CARD_DEFINITIONS;
    cards = static_cast<card_game_card_definition *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(card_game_card_definition)));
    if (cards == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    card_type_ids = static_cast<uint32_t *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(uint32_t)));
    if (card_type_ids == ft_nullptr)
    {
        cma_free(cards);
        return (FT_ERR_NO_MEMORY);
    }
    if (this->_card_count != 0U)
    {
        ft_memcpy(cards, this->_cards,
            static_cast<ft_size_t>(this->_card_count)
                * sizeof(card_game_card_definition));
        ft_memcpy(card_type_ids, this->_card_type_ids,
            static_cast<ft_size_t>(this->_card_count) * sizeof(uint32_t));
    }
    if (this->_cards != ft_nullptr)
        cma_free(this->_cards);
    if (this->_card_type_ids != ft_nullptr)
        cma_free(this->_card_type_ids);
    this->_cards = cards;
    this->_card_type_ids = card_type_ids;
    this->_card_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_card(
    const card_game_card_definition &definition) noexcept
{
    return (this->register_card_internal(definition,
        static_cast<uint32_t>(definition.type)));
}

int32_t card_game_engine::register_card_with_type(
    const card_game_card_definition &definition, uint32_t type_id) noexcept
{
    card_game_card_type_definition loaded_type;

    if (type_id < 4U || type_id >= FT_CARD_GAME_MAX_CARD_TYPES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_card_type(type_id, &loaded_type) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    return (this->register_card_internal(definition, type_id));
}

int32_t card_game_engine::register_card_type(
    const card_game_card_type_definition &type) noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || type.type_id < 4U
        || type.type_id >= FT_CARD_GAME_MAX_CARD_TYPES
        || type.allowed_zone_mask == 0U
        || type.max_copies_per_player == 0U
        || this->_card_type_count >= FT_CARD_GAME_MAX_CARD_TYPES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_card_type_count)
    {
        if (this->_card_types[index].type_id == type.type_id)
            return (FT_ERR_ALREADY_EXISTS);
        index += 1U;
    }
    this->_card_types[this->_card_type_count] = type;
    this->_card_type_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_card_type(uint32_t type_id,
    card_game_card_type_definition *type) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || type == ft_nullptr
        || type_id < 4U || type_id >= FT_CARD_GAME_MAX_CARD_TYPES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_card_type_count)
    {
        if (this->_card_types[index].type_id == type_id)
        {
            *type = this->_card_types[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

ft_bool card_game_engine::is_command_allowed(uint32_t command_mask) const noexcept
{
    uint32_t index;

    if (this->_phase_count == 0U)
        return (FT_TRUE);
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == this->_current_phase_id)
        {
            if (this->_phases[index].allowed_command_mask == 0U
                || (this->_phases[index].allowed_command_mask & command_mask)
                    != 0U)
                return (FT_TRUE);
            return (FT_FALSE);
        }
        index += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_engine::register_effect(card_game_effect_function effect,
    uint32_t *effect_id) noexcept
{
    if (this->_initialised_state != 2U || effect == ft_nullptr
        || effect_id == ft_nullptr || this->_effect_count
            >= FT_CARD_GAME_MAX_EFFECTS)
        return (FT_ERR_INVALID_ARGUMENT);
    *effect_id = this->_effect_count;
    this->_effects[this->_effect_count] = effect;
    this->_effect_callbacks[this->_effect_count] = ft_nullptr;
    this->_effect_user_data[this->_effect_count] = ft_nullptr;
    this->_effect_event_types[this->_effect_count] = 0U;
    this->_effect_priorities[this->_effect_count] = 0U;
    this->_effect_usage_limit_ids[this->_effect_count] = 0U;
    this->_effect_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_effect_callback(
    card_game_effect_callback callback, void *user_data, uint32_t event_type,
    uint32_t *effect_id) noexcept
{
    return (this->register_effect_callback_with_priority(callback, user_data,
        event_type, 0U, effect_id));
}

int32_t card_game_engine::register_effect_callback_with_priority(
    card_game_effect_callback callback, void *user_data, uint32_t event_type,
    uint32_t priority, uint32_t *effect_id) noexcept
{
    return (this->register_effect_callback_with_usage_limit(callback,
        user_data, event_type, priority, 0U, effect_id));
}

int32_t card_game_engine::register_effect_callback_with_usage_limit(
    card_game_effect_callback callback, void *user_data, uint32_t event_type,
    uint32_t priority, uint32_t usage_limit_id, uint32_t *effect_id) noexcept
{
    card_game_usage_limit usage_limit;

    if (this->_initialised_state != 2U || callback == ft_nullptr
        || effect_id == ft_nullptr || this->_effect_count
            >= FT_CARD_GAME_MAX_EFFECTS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (usage_limit_id != 0U
        && this->_usage_limits.get(usage_limit_id, &usage_limit)
            != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    *effect_id = this->_effect_count;
    this->_effects[this->_effect_count] = ft_nullptr;
    this->_effect_callbacks[this->_effect_count] = callback;
    this->_effect_user_data[this->_effect_count] = user_data;
    this->_effect_event_types[this->_effect_count] = event_type;
    this->_effect_priorities[this->_effect_count] = priority;
    this->_effect_usage_limit_ids[this->_effect_count] = usage_limit_id;
    this->_effect_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_phase(
    const card_game_phase_definition &phase) noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || phase.phase_id == 0U
        || this->_phase_count >= FT_CARD_GAME_MAX_PHASES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == phase.phase_id)
            return (FT_ERR_ALREADY_EXISTS);
        index += 1U;
    }
    this->_phases[this->_phase_count] = phase;
    this->_phase_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_zone(
    const card_game_zone_definition &zone) noexcept
{
    uint32_t index;
    card_game_zone_store_definition store_definition;
    int32_t result;

    if (this->_initialised_state != 2U || zone.zone_id == 0U
        || zone.capacity == 0U || zone.capacity > FT_CARD_GAME_MAX_CARDS
        || zone.allowed_card_type_mask == 0U
        || this->_zone_count >= FT_CARD_GAME_MAX_ZONES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_zone_count)
    {
        if (this->_zones[index].zone_id == zone.zone_id)
            return (FT_ERR_ALREADY_EXISTS);
        index += 1U;
    }
    store_definition.zone_id = zone.zone_id;
    store_definition.capacity = zone.capacity;
    store_definition.allowed_card_type_mask = zone.allowed_card_type_mask;
    store_definition.owner_scoped = zone.owner_scoped;
    result = this->_zone_store.register_zone(store_definition);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_zones[this->_zone_count] = zone;
    this->_zone_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_zone(uint32_t zone_id,
    card_game_zone_definition *zone) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || zone == ft_nullptr
        || zone_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_zone_count)
    {
        if (this->_zones[index].zone_id == zone_id)
        {
            *zone = this->_zones[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::register_start_override(
    const card_game_start_override &override_rule) noexcept
{
    if (this->_initialised_state != 2U || this->_player_count != 0U
        || override_rule.source_id == 0U
        || override_rule.field > CARD_GAME_START_FIRST_PLAYER
        || override_rule.operation > CARD_GAME_START_OVERRIDE_ADD
        || this->_start_override_count >= FT_CARD_GAME_MAX_START_OVERRIDES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (override_rule.field == CARD_GAME_START_FIRST_PLAYER
        && (override_rule.player_id != CARD_GAME_START_ALL_PLAYERS
            || override_rule.operation != CARD_GAME_START_OVERRIDE_SET))
        return (FT_ERR_INVALID_ARGUMENT);
    if (override_rule.operation == CARD_GAME_START_OVERRIDE_ADD
        && override_rule.field >= CARD_GAME_START_OPENING_HAND)
        return (FT_ERR_INVALID_ARGUMENT);
    if (override_rule.field == CARD_GAME_START_FIRST_PLAYER
        && override_rule.value >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_start_overrides[this->_start_override_count] = override_rule;
    this->_start_override_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::resolve_start_override(
    card_game_start_override_field field, uint32_t player_id,
    uint32_t baseline, uint32_t *value) const noexcept
{
    uint32_t index;
    uint32_t highest_set_priority;
    uint32_t resolved_value;
    ft_bool set_found;

    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    resolved_value = baseline;
    highest_set_priority = 0U;
    set_found = FT_FALSE;
    index = 0U;
    while (index < this->_start_override_count)
    {
        const card_game_start_override &rule = this->_start_overrides[index];
        if (rule.field == field
            && (rule.player_id == player_id
                || rule.player_id == CARD_GAME_START_ALL_PLAYERS))
        {
            if (rule.operation == CARD_GAME_START_OVERRIDE_SET)
            {
                if (set_found == FT_FALSE
                    || rule.priority > highest_set_priority)
                {
                    resolved_value = rule.value;
                    highest_set_priority = rule.priority;
                    set_found = FT_TRUE;
                }
                else if (rule.priority == highest_set_priority
                    && rule.value != resolved_value)
                    return (FT_ERR_INVALID_STATE);
            }
        }
        index += 1U;
    }
    index = 0U;
    while (index < this->_start_override_count)
    {
        const card_game_start_override &rule = this->_start_overrides[index];
        if (rule.field == field
            && (rule.player_id == player_id
                || rule.player_id == CARD_GAME_START_ALL_PLAYERS)
            && rule.operation == CARD_GAME_START_OVERRIDE_ADD)
        {
            if (UINT32_MAX - resolved_value < rule.value)
                return (FT_ERR_OUT_OF_RANGE);
            resolved_value += rule.value;
        }
        index += 1U;
    }
    *value = resolved_value;
    return (FT_ERR_SUCCESS);
}


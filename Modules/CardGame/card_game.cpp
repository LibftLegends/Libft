#include "card_game.hpp"

#include "../Basic/class_nullptr.hpp"

static void card_game_hash_u32(uint64_t *hash, uint32_t value) noexcept
{
    *hash ^= static_cast<uint64_t>(value);
    *hash *= 1099511628211ULL;
    return ;
}

static void card_game_hash_u64(uint64_t *hash, uint64_t value) noexcept
{
    *hash ^= value;
    *hash *= 1099511628211ULL;
    return ;
}

static void card_game_hash_instance(uint64_t *hash,
    const card_game_card_instance &instance) noexcept
{
    card_game_hash_u32(hash, instance.definition_id);
    card_game_hash_u32(hash, instance.owner_id);
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.attack));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.health));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.damage_taken));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.on_board));
    return ;
}

static void card_game_replay_write_u32(uint8_t *output, uint32_t *offset,
    uint32_t value) noexcept
{
    output[*offset] = static_cast<uint8_t>(value & 255U);
    output[*offset + 1U] = static_cast<uint8_t>((value >> 8U) & 255U);
    output[*offset + 2U] = static_cast<uint8_t>((value >> 16U) & 255U);
    output[*offset + 3U] = static_cast<uint8_t>((value >> 24U) & 255U);
    *offset += 4U;
    return ;
}

static void card_game_replay_write_u64(uint8_t *output, uint32_t *offset,
    uint64_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 8U)
    {
        output[*offset + index] = static_cast<uint8_t>(value & 255U);
        value >>= 8U;
        index += 1U;
    }
    *offset += 8U;
    return ;
}

static int32_t card_game_replay_read_u32(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint32_t *value) noexcept
{
    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    *value = static_cast<uint32_t>(input[*offset])
        | (static_cast<uint32_t>(input[*offset + 1U]) << 8U)
        | (static_cast<uint32_t>(input[*offset + 2U]) << 16U)
        | (static_cast<uint32_t>(input[*offset + 3U]) << 24U);
    *offset += 4U;
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_replay_read_u64(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint64_t *value) noexcept
{
    uint32_t index;
    uint64_t result;

    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 8U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = 0U;
    index = 0U;
    while (index < 8U)
    {
        result |= static_cast<uint64_t>(input[*offset + index])
            << (index * 8U);
        index += 1U;
    }
    *offset += 8U;
    *value = result;
    return (FT_ERR_SUCCESS);
}

card_game_operation_buffer::card_game_operation_buffer() noexcept
    : _operations(), _count(0U)
{
    return ;
}

card_game_operation_buffer::~card_game_operation_buffer() noexcept
{
    return ;
}

int32_t card_game_operation_buffer::clear() noexcept
{
    this->_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::append(
    const card_game_operation &operation) noexcept
{
    if (this->_count >= FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_FULL);
    this->_operations[this->_count] = operation;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_operation_buffer::size() const noexcept
{
    return (this->_count);
}

int32_t card_game_operation_buffer::get(uint32_t index,
    card_game_operation *operation) const noexcept
{
    if (operation == ft_nullptr || index >= this->_count)
        return (FT_ERR_INVALID_ARGUMENT);
    *operation = this->_operations[index];
    return (FT_ERR_SUCCESS);
}

card_game_engine::card_game_engine() noexcept
    : _initialised_state(0U), _rules(), _cards(), _card_type_ids(),
      _card_types(), _card_type_count(0U), _effects(),
      _effect_callbacks(), _effect_user_data(), _effect_event_types(),
      _phases(), _phase_count(0U), _zones(), _zone_count(0U),
      _current_phase_id(0U), _events(),
      _event_count(0U), _event_sequence(0U), _card_count(0U),
      _effect_count(0U), _board(), _instances(), _board_count(), _decks(),
      _health(), _mana(), _turn_number(0U), _active_player(0U),
      _player_count(0U),
      _next_deck_instance_id(1U),
      _modifiers(), _modifier_count(0U), _next_modifier_id(1U),
      _state_sequence(0U), _last_command_sequence(0U), _command_records(),
      _command_record_count(0U)
{
    return ;
}

card_game_engine::~card_game_engine() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_engine::initialize(const card_game_rules &rules) noexcept
{
    uint32_t deck_index;
    int32_t deck_error;

    if (this->_initialised_state == 2U)
        return (FT_ERR_ALREADY_INITIALISED);
    if (rules.max_board_spaces == 0U || rules.max_board_spaces
        > FT_CARD_GAME_MAX_CARDS || rules.max_hand_size == 0U
        || rules.max_turns == 0U || rules.max_mana == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_rules = rules;
    deck_index = 0U;
    while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
    {
        deck_error = this->_decks[deck_index].initialize(
            FT_CARD_GAME_MAX_CARDS, FT_TRUE);
        if (deck_error != FT_ERR_SUCCESS)
        {
            while (deck_index > 0U)
            {
                deck_index -= 1U;
                (void)this->_decks[deck_index].destroy();
            }
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (deck_error);
        }
        deck_index += 1U;
    }
    this->_card_count = 0U;
    this->_card_type_count = 0U;
    this->_effect_count = 0U;
    this->_phase_count = 0U;
    this->_zone_count = 0U;
    this->_event_count = 0U;
    this->_event_sequence = 0U;
    this->_turn_number = 0U;
    this->_active_player = 0U;
    this->_next_deck_instance_id = 1U;
    this->_modifier_count = 0U;
    this->_next_modifier_id = 1U;
    this->_state_sequence = 1U;
    this->_last_command_sequence = 0U;
    this->_command_record_count = 0U;
    this->_initialised_state = 2U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::destroy() noexcept
{
    uint32_t deck_index;

    if (this->_initialised_state != 2U)
        return (FT_ERR_SUCCESS);
    deck_index = 0U;
    while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
    {
        (void)this->_decks[deck_index].destroy();
        deck_index += 1U;
    }
    this->_initialised_state = 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::move(card_game_engine &other) noexcept
{
    uint32_t deck_index;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != 2U)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_rules = other._rules;
    ft_memcpy(this->_cards, other._cards, sizeof(this->_cards));
    ft_memcpy(this->_card_type_ids, other._card_type_ids,
        sizeof(this->_card_type_ids));
    ft_memcpy(this->_card_types, other._card_types,
        sizeof(this->_card_types));
    ft_memcpy(this->_effects, other._effects, sizeof(this->_effects));
    ft_memcpy(this->_effect_callbacks, other._effect_callbacks,
        sizeof(this->_effect_callbacks));
    ft_memcpy(this->_effect_user_data, other._effect_user_data,
        sizeof(this->_effect_user_data));
    ft_memcpy(this->_effect_event_types, other._effect_event_types,
        sizeof(this->_effect_event_types));
    ft_memcpy(this->_phases, other._phases, sizeof(this->_phases));
    ft_memcpy(this->_zones, other._zones, sizeof(this->_zones));
    ft_memcpy(this->_events, other._events, sizeof(this->_events));
    ft_memcpy(this->_board, other._board, sizeof(this->_board));
    ft_memcpy(this->_instances, other._instances, sizeof(this->_instances));
    ft_memcpy(this->_board_count, other._board_count, sizeof(this->_board_count));
    ft_memcpy(this->_health, other._health, sizeof(this->_health));
    deck_index = 0U;
    while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (this->_decks[deck_index].move(other._decks[deck_index])
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        deck_index += 1U;
    }
    ft_memcpy(this->_mana, other._mana, sizeof(this->_mana));
    this->_card_count = other._card_count;
    this->_card_type_count = other._card_type_count;
    this->_effect_count = other._effect_count;
    this->_turn_number = other._turn_number;
    this->_active_player = other._active_player;
    this->_player_count = other._player_count;
    this->_next_deck_instance_id = other._next_deck_instance_id;
    ft_memcpy(this->_modifiers, other._modifiers, sizeof(this->_modifiers));
    this->_modifier_count = other._modifier_count;
    this->_next_modifier_id = other._next_modifier_id;
    this->_state_sequence = other._state_sequence;
    this->_last_command_sequence = other._last_command_sequence;
    ft_memcpy(this->_command_records, other._command_records,
        sizeof(this->_command_records));
    this->_command_record_count = other._command_record_count;
    this->_phase_count = other._phase_count;
    this->_zone_count = other._zone_count;
    this->_current_phase_id = other._current_phase_id;
    this->_event_count = other._event_count;
    this->_event_sequence = other._event_sequence;
    this->_initialised_state = 2U;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

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
            && definition.effect_id >= this->_effect_count)
        || this->_card_count >= FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card(definition.card_id, &existing) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    this->_cards[this->_card_count] = definition;
    this->_card_type_ids[this->_card_count] = type_id;
    this->_card_count += 1U;
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
    this->_effect_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_effect_callback(
    card_game_effect_callback callback, void *user_data, uint32_t event_type,
    uint32_t *effect_id) noexcept
{
    if (this->_initialised_state != 2U || callback == ft_nullptr
        || effect_id == ft_nullptr || this->_effect_count
            >= FT_CARD_GAME_MAX_EFFECTS)
        return (FT_ERR_INVALID_ARGUMENT);
    *effect_id = this->_effect_count;
    this->_effects[this->_effect_count] = ft_nullptr;
    this->_effect_callbacks[this->_effect_count] = callback;
    this->_effect_user_data[this->_effect_count] = user_data;
    this->_effect_event_types[this->_effect_count] = event_type;
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

int32_t card_game_engine::start_match(uint32_t player_count) noexcept
{
    int32_t event_error;
    uint32_t index;

    if (this->_initialised_state != 2U || player_count == 0U
        || player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < player_count)
    {
        this->_board_count[index] = 0U;
        if (this->_decks[index].clear() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
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
    this->_next_deck_instance_id = 1U;
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

int32_t card_game_engine::emit_event(uint32_t event_type,
    uint32_t source_instance, uint32_t target_instance) noexcept
{
    if (this->_initialised_state != 2U || event_type == 0U
        || this->_event_count >= FT_CARD_GAME_MAX_EVENTS)
        return (FT_ERR_FULL);
    this->_events[this->_event_count].sequence = this->_event_sequence;
    this->_events[this->_event_count].event_type = event_type;
    this->_events[this->_event_count].source_instance = source_instance;
    this->_events[this->_event_count].target_instance = target_instance;
    this->_event_count += 1U;
    this->_event_sequence += 1U;
    this->_state_sequence += 1U;
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
    return (FT_ERR_INVALID_ARGUMENT);
}

int32_t card_game_engine::resolve_events() noexcept
{
    uint32_t event_index;
    uint32_t effect_index;
    card_game_operation_buffer operations;
    card_game_effect_context context;
    card_game_operation operation;
    int32_t error_code;
    int32_t snapshot_error;
    int32_t restore_error;
    card_game_snapshot before_state;
    uint32_t operation_index;

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
        effect_index = 0U;
        while (effect_index < this->_effect_count)
        {
            if (this->_effect_callbacks[effect_index] != ft_nullptr
                && this->_effect_event_types[effect_index] == context.event_type)
            {
                error_code = operations.clear();
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                error_code = this->_effect_callbacks[effect_index](*this,
                    context, operations, this->_effect_user_data[effect_index]);
                if (error_code != FT_ERR_SUCCESS)
                {
                    restore_error = this->apply_snapshot(before_state);
                    if (restore_error != FT_ERR_SUCCESS)
                        return (restore_error);
                    return (error_code);
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
                    error_code = this->apply_operation(operation);
                    if (error_code != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (error_code);
                    }
                    operation_index += 1U;
                }
            }
            effect_index += 1U;
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
                    restore_error = this->apply_snapshot(before_state);
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
                    restore_error = this->apply_snapshot(before_state);
                    if (restore_error != FT_ERR_SUCCESS)
                        return (restore_error);
                    return (emit_error);
                }
            }
            resolve_error = this->resolve_events();
            if (resolve_error != FT_ERR_SUCCESS)
            {
                restore_error = this->apply_snapshot(before_state);
                if (restore_error != FT_ERR_SUCCESS)
                    return (restore_error);
                return (resolve_error);
            }
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    restore_error = this->apply_snapshot(before_state);
    if (restore_error != FT_ERR_SUCCESS)
        return (restore_error);
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::get_player_health(uint32_t player_id,
    uint32_t *health) const noexcept
{
    if (this->_initialised_state != 2U || health == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *health = this->_health[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_current_phase(uint32_t *phase_id) const noexcept
{
    if (this->_initialised_state != 2U || phase_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_phase_count == 0U)
        return (FT_ERR_NOT_FOUND);
    *phase_id = this->_current_phase_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_phase(uint32_t phase_id,
    card_game_phase_definition *phase) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || phase == ft_nullptr
        || phase_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == phase_id)
        {
            *phase = this->_phases[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::get_board_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_board_count[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_deck_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_decks[player_id].size();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::allocate_deck_instance_id(
    uint32_t *instance_id) noexcept
{
    if (instance_id == ft_nullptr || this->_next_deck_instance_id == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    *instance_id = this->_next_deck_instance_id;
    this->_next_deck_instance_id += 1U;
    if (this->_next_deck_instance_id == 0U)
        this->_next_deck_instance_id = 1U;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_engine::deck_instance_exists(uint32_t instance_id) const noexcept
{
    uint32_t player_id;

    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (this->_decks[player_id].contains_instance(instance_id)
            != FT_FALSE)
            return (FT_TRUE);
        player_id += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_engine::deck_push_top_instance(uint32_t player_id,
    uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t push_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    push_error = this->_decks[player_id].push_top_entry(entry);
    if (push_error != FT_ERR_SUCCESS)
        return (push_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_push_bottom_instance(uint32_t player_id,
    uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t push_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    push_error = this->_decks[player_id].push_bottom_entry(entry);
    if (push_error != FT_ERR_SUCCESS)
        return (push_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_insert_instance_at(uint32_t player_id,
    uint32_t index, uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t insert_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    insert_error = this->_decks[player_id].insert_entry_at(index, entry);
    if (insert_error != FT_ERR_SUCCESS)
        return (insert_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_push_top(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_push_top_instance(player_id, instance_id, card_id));
}

int32_t card_game_engine::deck_push_bottom(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_push_bottom_instance(player_id, instance_id, card_id));
}

int32_t card_game_engine::deck_insert_at(uint32_t player_id, uint32_t index,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_insert_instance_at(player_id, index, instance_id,
        card_id));
}

int32_t card_game_engine::deck_peek_top(uint32_t player_id,
    uint32_t *card_id) const noexcept
{
    card_game_deck_card card;
    int32_t inspect_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    inspect_error = this->deck_inspect(player_id, 0U, &card);
    if (inspect_error != FT_ERR_SUCCESS)
        return (inspect_error);
    *card_id = card.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_peek_bottom(uint32_t player_id,
    uint32_t *card_id) const noexcept
{
    card_game_deck_card card;
    uint32_t count;
    int32_t inspect_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    count = this->_decks[player_id].size();
    if (count == 0U)
        return (FT_ERR_EMPTY);
    inspect_error = this->deck_inspect(player_id, count - 1U, &card);
    if (inspect_error != FT_ERR_SUCCESS)
        return (inspect_error);
    *card_id = card.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_top(uint32_t player_id,
    uint32_t *card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    if (card_id != ft_nullptr)
        *card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_bottom(uint32_t player_id,
    uint32_t *card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_bottom_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    if (card_id != ft_nullptr)
        *card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_top(uint32_t player_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_remove(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t index;
    card_game_zone_entry entry;
    int32_t get_error;
    int32_t remove_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_decks[player_id].size())
    {
        get_error = this->_decks[player_id].get_entry(index, &entry);
        if (get_error != FT_ERR_SUCCESS)
            return (get_error);
        if (entry.card_id == card_id)
            break ;
        index += 1U;
    }
    if (index >= this->_decks[player_id].size())
        return (FT_ERR_NOT_FOUND);
    remove_error = this->_decks[player_id].remove_entry(entry.instance_id,
        ft_nullptr);
    if (remove_error != FT_ERR_SUCCESS)
        return (remove_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_inspect(uint32_t player_id, uint32_t index,
    card_game_deck_card *card) const noexcept
{
    card_game_zone_entry entry;
    int32_t get_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    get_error = this->_decks[player_id].get_entry(index, &entry);
    if (get_error != FT_ERR_SUCCESS)
        return (get_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_get_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) const noexcept
{
    uint32_t index;
    int32_t inspect_error;
    card_game_deck_card candidate;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_decks[player_id].size())
    {
        inspect_error = this->deck_inspect(player_id, index, &candidate);
        if (inspect_error != FT_ERR_SUCCESS)
            return (inspect_error);
        if (candidate.instance_id == instance_id)
        {
            *card = candidate;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::deck_draw_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t remove_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    remove_error = this->_decks[player_id].remove_entry(instance_id, &entry);
    if (remove_error != FT_ERR_SUCCESS)
        return (remove_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::shuffle_deck(uint32_t player_id,
    uint64_t *random_state) noexcept
{
    int32_t shuffle_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    shuffle_error = this->_decks[player_id].shuffle(random_state);
    if (shuffle_error != FT_ERR_SUCCESS)
        return (shuffle_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_turn(uint32_t *turn_number,
    uint32_t *active_player) const noexcept
{
    if (this->_initialised_state != 2U || turn_number == ft_nullptr
        || active_player == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *turn_number = this->_turn_number;
    *active_player = this->_active_player;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_instance(uint32_t player_id, uint32_t index,
    card_game_card_instance *instance) const noexcept
{
    int32_t stats_error;

    if (this->_initialised_state != 2U || instance == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    *instance = this->_instances[player_id][index];
    stats_error = this->get_effective_instance_stats(player_id, index,
        &instance->attack, &instance->health);
    if (stats_error != FT_ERR_SUCCESS)
        return (stats_error);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::allocate_modifier_id(uint32_t *modifier_id) noexcept
{
    if (modifier_id == ft_nullptr || this->_next_modifier_id == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    *modifier_id = this->_next_modifier_id;
    this->_next_modifier_id += 1U;
    if (this->_next_modifier_id == 0U)
        this->_next_modifier_id = 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_effective_instance_stats(uint32_t player_id,
    uint32_t instance_index, int32_t *attack, int32_t *health) const noexcept
{
    const card_game_card_definition *definition;
    int64_t calculated_attack;
    int64_t calculated_health;
    uint32_t index;
    uint32_t definition_index;

    if (this->_initialised_state != 2U || attack == ft_nullptr
        || health == ft_nullptr || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    definition = ft_nullptr;
    definition_index = 0U;
    while (definition_index < this->_card_count)
    {
        if (this->_cards[definition_index].card_id
            == this->_instances[player_id][instance_index].definition_id)
        {
            definition = &this->_cards[definition_index];
            break ;
        }
        definition_index += 1U;
    }
    if (definition == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    calculated_attack = definition->attack;
    calculated_health = definition->health
        - this->_instances[player_id][instance_index].damage_taken;
    index = 0U;
    while (index < this->_modifier_count)
    {
        const card_game_card_modifier &modifier = this->_modifiers[index];

        if (modifier.target_player_id == player_id
            && modifier.target_instance_index == instance_index)
        {
            calculated_attack += modifier.attack_delta;
            calculated_health += modifier.health_delta;
        }
        index += 1U;
    }
    if (calculated_attack > static_cast<int64_t>(INT32_MAX))
        calculated_attack = INT32_MAX;
    if (calculated_attack < static_cast<int64_t>(INT32_MIN))
        calculated_attack = INT32_MIN;
    if (calculated_health > static_cast<int64_t>(INT32_MAX))
        calculated_health = INT32_MAX;
    if (calculated_health < static_cast<int64_t>(INT32_MIN))
        calculated_health = INT32_MIN;
    *attack = static_cast<int32_t>(calculated_attack);
    *health = static_cast<int32_t>(calculated_health);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::add_card_modifier(uint32_t player_id,
    uint32_t instance_index, int32_t attack_delta, int32_t health_delta,
    card_game_modifier_duration duration, uint32_t source_effect_id,
    uint32_t *modifier_id) noexcept
{
    card_game_card_modifier *modifier;
    int32_t allocation_error;

    if (this->_initialised_state != 2U || modifier_id == ft_nullptr
        || player_id >= this->_player_count
        || instance_index >= this->_board_count[player_id]
        || (duration != CARD_GAME_MODIFIER_PERMANENT
            && duration != CARD_GAME_MODIFIER_UNTIL_END_TURN))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_modifier_count >= FT_CARD_GAME_MAX_MODIFIERS)
        return (FT_ERR_FULL);
    modifier = &this->_modifiers[this->_modifier_count];
    allocation_error = this->allocate_modifier_id(modifier_id);
    if (allocation_error != FT_ERR_SUCCESS)
        return (allocation_error);
    modifier->modifier_id = *modifier_id;
    modifier->source_effect_id = source_effect_id;
    modifier->target_player_id = player_id;
    modifier->target_instance_index = instance_index;
    modifier->attack_delta = attack_delta;
    modifier->health_delta = health_delta;
    modifier->duration = duration;
    modifier->created_turn = this->_turn_number;
    modifier->created_phase_id = this->_current_phase_id;
    this->_modifier_count += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::remove_card_modifier(uint32_t modifier_id) noexcept
{
    uint32_t index;
    uint32_t move_index;

    if (this->_initialised_state != 2U || modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_modifier_count
        && this->_modifiers[index].modifier_id != modifier_id)
        index += 1U;
    if (index >= this->_modifier_count)
        return (FT_ERR_NOT_FOUND);
    move_index = index + 1U;
    while (move_index < this->_modifier_count)
    {
        this->_modifiers[move_index - 1U] = this->_modifiers[move_index];
        move_index += 1U;
    }
    this->_modifier_count -= 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_card_modifier(uint32_t modifier_id,
    card_game_card_modifier *modifier) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || modifier == ft_nullptr
        || modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].modifier_id == modifier_id)
        {
            *modifier = this->_modifiers[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::expire_turn_modifiers() noexcept
{
    uint32_t index;

    index = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].duration
            == CARD_GAME_MODIFIER_UNTIL_END_TURN)
        {
            if (this->remove_card_modifier(this->_modifiers[index].modifier_id)
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
        }
        else
            index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::remove_board_instance(uint32_t player_id,
    uint32_t instance_index) noexcept
{
    uint32_t move_index;

    if (player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    move_index = instance_index + 1U;
    while (move_index < this->_board_count[player_id])
    {
        this->_board[player_id][move_index - 1U] =
            this->_board[player_id][move_index];
        this->_instances[player_id][move_index - 1U] =
            this->_instances[player_id][move_index];
        move_index += 1U;
    }
    this->_board_count[player_id] -= 1U;
    this->_instances[player_id][this->_board_count[player_id]].on_board =
        FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::resolve_combat(uint32_t attacking_player,
    uint32_t attacker_index, uint32_t defending_player,
    uint32_t defender_index, card_game_combat_mode mode) noexcept
{
    card_game_snapshot before_state;
    int32_t attacker_attack;
    int32_t attacker_health;
    int32_t defender_attack;
    int32_t defender_health;
    int64_t attacker_damage;
    int64_t defender_damage;
    int32_t error_code;
    uint32_t adjusted_attacker_index;

    if (this->_initialised_state != 2U
        || (mode != CARD_GAME_COMBAT_ORDERED
            && mode != CARD_GAME_COMBAT_SIMULTANEOUS)
        || attacking_player >= this->_player_count
        || defending_player >= this->_player_count
        || attacking_player == defending_player
        || attacker_index >= this->_board_count[attacking_player]
        || defender_index >= this->_board_count[defending_player]
        || this->_instances[attacking_player][attacker_index].on_board == FT_FALSE
        || this->_instances[defending_player][defender_index].on_board
            == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_snapshot(&before_state) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    error_code = this->get_effective_instance_stats(attacking_player,
        attacker_index, &attacker_attack, &attacker_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->get_effective_instance_stats(defending_player,
        defender_index, &defender_attack, &defender_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    attacker_damage = attacker_attack;
    defender_damage = defender_attack;
    if (attacker_damage < 0)
        attacker_damage = 0;
    if (defender_damage < 0)
        defender_damage = 0;
    if (attacker_damage > static_cast<int64_t>(INT32_MAX)
        - this->_instances[defending_player][defender_index].damage_taken)
        this->_instances[defending_player][defender_index].damage_taken =
            INT32_MAX;
    else
        this->_instances[defending_player][defender_index].damage_taken +=
            static_cast<int32_t>(attacker_damage);
    if (mode == CARD_GAME_COMBAT_SIMULTANEOUS
        || attacker_damage < static_cast<int64_t>(defender_health))
    {
        if (defender_damage > static_cast<int64_t>(INT32_MAX)
            - this->_instances[attacking_player][attacker_index].damage_taken)
            this->_instances[attacking_player][attacker_index].damage_taken =
                INT32_MAX;
        else
            this->_instances[attacking_player][attacker_index]
                .damage_taken += static_cast<int32_t>(defender_damage);
    }
    if (this->get_effective_instance_stats(defending_player, defender_index,
            &defender_attack, &defender_health) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (this->get_effective_instance_stats(attacking_player, attacker_index,
            &attacker_attack, &attacker_health) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (defender_health <= 0)
    {
        if (this->remove_board_instance(defending_player, defender_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    if (attacker_health <= 0)
    {
        adjusted_attacker_index = attacker_index;
        if (defender_health <= 0 && attacking_player == defending_player
            && defender_index < attacker_index)
            adjusted_attacker_index -= 1U;
        if (this->remove_board_instance(attacking_player,
                adjusted_attacker_index) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_validate_player_snapshot(
    const card_game_player_snapshot &player, uint32_t player_id,
    uint32_t max_board_spaces) noexcept
{
    uint32_t index;

    if (player.board_count > max_board_spaces
        || player.board_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (player.deck_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < player.board_count)
    {
        if (player.board[index] >= player.board_count
            || player.instances[index].on_board == FT_FALSE
            || player.instances[index].owner_id != player_id
            || player.instances[index].damage_taken < 0)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t card_game_copy_player_snapshot(
    card_game_player_snapshot *destination, const uint32_t *board,
    const card_game_card_instance *instances, uint32_t board_count,
    const card_game_ordered_zone &deck, uint32_t health,
    uint32_t mana) noexcept
{
    uint32_t deck_index;

    ft_memcpy(destination->board, board,
        sizeof(destination->board));
    ft_memcpy(destination->instances, instances,
        sizeof(destination->instances));
    destination->board_count = board_count;
    destination->deck_count = deck.size();
    deck_index = 0U;
    while (deck_index < destination->deck_count)
    {
        card_game_zone_entry entry;

        if (deck.get_entry(deck_index, &entry)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        destination->deck[deck_index] = entry.card_id;
        destination->deck_instance_ids[deck_index] = entry.instance_id;
        deck_index += 1U;
    }
    destination->health = health;
    destination->mana = mana;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_snapshot(
    card_game_snapshot *snapshot) const noexcept
{
    uint32_t player_id;

    if (this->_initialised_state != 2U || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bzero(snapshot, sizeof(*snapshot));
    snapshot->format_version = FT_CARD_GAME_STATE_FORMAT_VERSION;
    snapshot->state_sequence = this->_state_sequence;
    snapshot->player_count = this->_player_count;
    snapshot->turn_number = this->_turn_number;
    snapshot->active_player = this->_active_player;
    snapshot->current_phase_id = this->_current_phase_id;
    snapshot->event_count = this->_event_count;
    snapshot->event_sequence = this->_event_sequence;
    snapshot->modifier_count = this->_modifier_count;
    ft_memcpy(snapshot->modifiers, this->_modifiers,
        sizeof(snapshot->modifiers));
    ft_memcpy(snapshot->events, this->_events, sizeof(snapshot->events));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (card_game_copy_player_snapshot(&snapshot->players[player_id],
            this->_board[player_id], this->_instances[player_id],
            this->_board_count[player_id], this->_decks[player_id],
            this->_health[player_id], this->_mana[player_id])
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        player_id += 1U;
    }
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
    card_game_snapshot snapshot;
    uint64_t calculated_hash;
    uint32_t player_id;
    uint32_t event_index;
    uint32_t board_index;
    uint32_t deck_index;
    uint32_t index;

    if (this->_initialised_state != 2U || hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    calculated_hash = 1469598103934665603ULL;
    card_game_hash_u32(&calculated_hash, snapshot.format_version);
    card_game_hash_u64(&calculated_hash, snapshot.state_sequence);
    card_game_hash_u32(&calculated_hash, snapshot.player_count);
    card_game_hash_u32(&calculated_hash, snapshot.turn_number);
    card_game_hash_u32(&calculated_hash, snapshot.active_player);
    card_game_hash_u32(&calculated_hash, snapshot.current_phase_id);
    card_game_hash_u32(&calculated_hash, snapshot.event_count);
    card_game_hash_u64(&calculated_hash, snapshot.event_sequence);
    card_game_hash_u32(&calculated_hash, snapshot.modifier_count);
    index = 0U;
    while (index < snapshot.modifier_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].modifier_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].source_effect_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].target_player_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].target_instance_index);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.modifiers[index].attack_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.modifiers[index].health_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.modifiers[index].duration));
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].created_turn);
        card_game_hash_u32(&calculated_hash,
            snapshot.modifiers[index].created_phase_id);
        index += 1U;
    }
    event_index = 0U;
    while (event_index < snapshot.event_count)
    {
        card_game_hash_u64(&calculated_hash,
            snapshot.events[event_index].sequence);
        card_game_hash_u32(&calculated_hash,
            snapshot.events[event_index].event_type);
        card_game_hash_u32(&calculated_hash,
            snapshot.events[event_index].source_instance);
        card_game_hash_u32(&calculated_hash,
            snapshot.events[event_index].target_instance);
        event_index += 1U;
    }
    player_id = 0U;
    while (player_id < snapshot.player_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.players[player_id].board_count);
        card_game_hash_u32(&calculated_hash,
            snapshot.players[player_id].deck_count);
        card_game_hash_u32(&calculated_hash,
            snapshot.players[player_id].health);
        card_game_hash_u32(&calculated_hash,
            snapshot.players[player_id].mana);
        board_index = 0U;
        while (board_index < snapshot.players[player_id].board_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot.players[player_id].board[board_index]);
            card_game_hash_instance(&calculated_hash,
                snapshot.players[player_id].instances[board_index]);
            board_index += 1U;
        }
        deck_index = 0U;
        while (deck_index < snapshot.players[player_id].deck_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot.players[player_id].deck[deck_index]);
            card_game_hash_u32(&calculated_hash,
                snapshot.players[player_id].deck_instance_ids[deck_index]);
            deck_index += 1U;
        }
        player_id += 1U;
    }
    *hash = calculated_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_snapshot(
    const card_game_snapshot &snapshot) noexcept
{
    uint32_t player_id;
    uint32_t phase_index;
    uint32_t deck_index;
    uint32_t instance_id;
    uint32_t previous_index;
    uint32_t index;
    uint32_t previous_player_id;
    ft_bool phase_found;

    if (this->_initialised_state != 2U
        || snapshot.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || snapshot.player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.event_count > FT_CARD_GAME_MAX_EVENTS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.modifier_count > FT_CARD_GAME_MAX_MODIFIERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.modifier_count)
    {
        if (snapshot.modifiers[index].modifier_id == 0U
            || snapshot.modifiers[index].target_player_id >= snapshot.player_count
            || snapshot.modifiers[index].target_instance_index
                >= snapshot.players[snapshot.modifiers[index]
                    .target_player_id].board_count
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
                player_id, this->_rules.max_board_spaces) != FT_ERR_SUCCESS)
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
        player_id += 1U;
    }
    this->_player_count = snapshot.player_count;
    this->_turn_number = snapshot.turn_number;
    this->_active_player = snapshot.active_player;
    this->_current_phase_id = snapshot.current_phase_id;
    this->_event_count = snapshot.event_count;
    this->_event_sequence = snapshot.event_sequence;
    this->_modifier_count = snapshot.modifier_count;
    ft_memcpy(this->_modifiers, snapshot.modifiers,
        sizeof(this->_modifiers));
    this->_next_modifier_id = 1U;
    index = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].modifier_id >= this->_next_modifier_id
            && this->_modifiers[index].modifier_id != UINT32_MAX)
            this->_next_modifier_id = this->_modifiers[index].modifier_id + 1U;
        index += 1U;
    }
    ft_memcpy(this->_events, snapshot.events, sizeof(this->_events));
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
            if (entry.instance_id >= this->_next_deck_instance_id
                && entry.instance_id != UINT32_MAX)
                this->_next_deck_instance_id = entry.instance_id + 1U;
            deck_index += 1U;
        }
        this->_health[player_id] = snapshot.players[player_id].health;
        this->_mana[player_id] = snapshot.players[player_id].mana;
        player_id += 1U;
    }
    this->_state_sequence = snapshot.state_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::create_delta(const card_game_snapshot &baseline,
    card_game_delta *delta) const noexcept
{
    card_game_snapshot current_snapshot;
    uint32_t player_id;

    if (this->_initialised_state != 2U || delta == ft_nullptr
        || baseline.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || baseline.player_count != this->_player_count)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_snapshot(&current_snapshot) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    ft_bzero(delta, sizeof(*delta));
    delta->format_version = FT_CARD_GAME_STATE_FORMAT_VERSION;
    delta->base_state_sequence = baseline.state_sequence;
    delta->target_state_sequence = current_snapshot.state_sequence;
    delta->player_count = current_snapshot.player_count;
    if (baseline.turn_number != current_snapshot.turn_number
        || baseline.active_player != current_snapshot.active_player
        || baseline.current_phase_id != current_snapshot.current_phase_id)
        delta->global_state_changed = FT_TRUE;
    delta->turn_number = current_snapshot.turn_number;
    delta->active_player = current_snapshot.active_player;
    delta->current_phase_id = current_snapshot.current_phase_id;
    delta->event_count = current_snapshot.event_count;
    delta->event_sequence = current_snapshot.event_sequence;
    delta->modifier_count = current_snapshot.modifier_count;
    ft_memcpy(delta->modifiers, current_snapshot.modifiers,
        sizeof(delta->modifiers));
    ft_memcpy(delta->events, current_snapshot.events, sizeof(delta->events));
    if (baseline.event_count != current_snapshot.event_count
        || baseline.event_sequence != current_snapshot.event_sequence
        || ft_memcmp(baseline.events, current_snapshot.events,
            sizeof(baseline.events)) != 0)
        delta->global_state_changed = FT_TRUE;
    if (baseline.modifier_count != current_snapshot.modifier_count
        || ft_memcmp(baseline.modifiers, current_snapshot.modifiers,
            sizeof(baseline.modifiers)) != 0)
        delta->global_state_changed = FT_TRUE;
    player_id = 0U;
    while (player_id < current_snapshot.player_count)
    {
        if (ft_memcmp(&baseline.players[player_id],
            &current_snapshot.players[player_id],
            sizeof(card_game_player_snapshot)) != 0)
        {
            delta->changed_player_mask |= (static_cast<uint64_t>(1U)
                << player_id);
            delta->players[player_id] = current_snapshot.players[player_id];
        }
        player_id += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::apply_delta(const card_game_delta &delta) noexcept
{
    uint32_t player_id;
    uint32_t deck_index;

    if (this->_initialised_state != 2U
        || delta.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || delta.base_state_sequence != this->_state_sequence
        || delta.player_count != this->_player_count
        || delta.event_count > FT_CARD_GAME_MAX_EVENTS
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
    if (delta.global_state_changed == FT_FALSE
        && (delta.modifier_count != this->_modifier_count
            || ft_memcmp(delta.modifiers, this->_modifiers,
                sizeof(this->_modifiers)) != 0))
        return (FT_ERR_INVALID_ARGUMENT);
    player_id = 0U;
    while (player_id < delta.modifier_count)
    {
        uint32_t target_player_id;
        uint32_t previous_modifier_index;

        target_player_id = delta.modifiers[player_id].target_player_id;
        if (delta.modifiers[player_id].modifier_id == 0U
            || target_player_id >= delta.player_count
            || (delta.modifiers[player_id].duration
                != CARD_GAME_MODIFIER_PERMANENT
                && delta.modifiers[player_id].duration
                    != CARD_GAME_MODIFIER_UNTIL_END_TURN))
            return (FT_ERR_INVALID_ARGUMENT);
        if ((delta.changed_player_mask & (static_cast<uint64_t>(1U)
                << target_player_id)) != 0U)
        {
            if (delta.modifiers[player_id].target_instance_index
                >= delta.players[target_player_id].board_count)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else if (delta.modifiers[player_id].target_instance_index
            >= this->_board_count[target_player_id])
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
                    player_id, this->_rules.max_board_spaces)
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
        player_id += 1U;
    }
    if (delta.global_state_changed != FT_FALSE)
    {
        this->_turn_number = delta.turn_number;
        this->_active_player = delta.active_player;
        this->_current_phase_id = delta.current_phase_id;
        this->_event_count = delta.event_count;
        this->_event_sequence = delta.event_sequence;
        ft_memcpy(this->_events, delta.events, sizeof(this->_events));
        this->_modifier_count = delta.modifier_count;
        ft_memcpy(this->_modifiers, delta.modifiers,
            sizeof(this->_modifiers));
        this->_next_modifier_id = 1U;
        player_id = 0U;
        while (player_id < this->_modifier_count)
        {
            if (this->_modifiers[player_id].modifier_id
                >= this->_next_modifier_id
                && this->_modifiers[player_id].modifier_id != UINT32_MAX)
                this->_next_modifier_id = this->_modifiers[player_id]
                    .modifier_id + 1U;
            player_id += 1U;
        }
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
            this->_health[player_id] = delta.players[player_id].health;
            this->_mana[player_id] = delta.players[player_id].mana;
        }
        player_id += 1U;
    }
    this->_state_sequence = delta.target_state_sequence;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::submit_command(
    const card_game_command &command, void *context) noexcept
{
    int32_t command_error;
    uint64_t rules_hash;
    uint64_t state_hash_before;
    uint64_t state_hash_after;

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
    if (this->get_rules_hash(&rules_hash) != FT_ERR_SUCCESS
        || this->get_state_hash(&state_hash_before) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (command.type == CARD_GAME_INTENT_PLAY_CARD)
        command_error = this->play_card(command.player_id, command.card_id,
            command.target_instance, context);
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
    card_game_command_record records[FT_CARD_GAME_MAX_COMMAND_RECORDS];
    uint32_t magic;
    uint32_t version;
    uint32_t record_count;
    uint32_t required_size;
    uint32_t offset;
    uint32_t index;
    uint32_t type;
    uint64_t previous_sequence;
    int32_t read_error;


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
    previous_sequence = 0U;
    index = 0U;
    while (index < record_count)
    {
        card_game_command_record *record;

        record = &records[index];
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.command_sequence);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.expected_state_sequence);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.player_id);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &type);
        if (read_error != FT_ERR_SUCCESS || type < 1U || type > 3U
            || record->command.command_sequence == 0U
            || record->command.command_sequence <= previous_sequence)
            return (FT_ERR_INVALID_ARGUMENT);
        record->command.type = static_cast<card_game_command_type>(type);
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.card_id);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u32(input, input_size, &offset,
            &record->command.target_instance);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->rules_hash);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->state_hash_before);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->state_hash_after);
        if (read_error != FT_ERR_SUCCESS)
            return (read_error);
        previous_sequence = record->command.command_sequence;
        index += 1U;
    }
    ft_memcpy(this->_command_records, records, sizeof(records));
    this->_command_record_count = record_count;
    if (record_count > 0U)
        this->_last_command_sequence = records[record_count - 1U].command
            .command_sequence;
    else
        this->_last_command_sequence = 0U;
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
            restore_error = this->apply_snapshot(starting_snapshot);
            this->_command_record_count = 0U;
            this->_last_command_sequence = 0U;
            if (restore_error != FT_ERR_SUCCESS)
                return (restore_error);
            return (FT_ERR_INVALID_STATE);
        }
        command_error = this->submit_command(records[index].command, context);
        if (command_error != FT_ERR_SUCCESS)
        {
            restore_error = this->apply_snapshot(starting_snapshot);
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
            restore_error = this->apply_snapshot(starting_snapshot);
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

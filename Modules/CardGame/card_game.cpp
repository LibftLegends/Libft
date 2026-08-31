#include "card_game.hpp"

static uint64_t card_game_match_random_next(uint64_t *state) noexcept
{
    uint64_t value;

    value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return (value * 2685821657736338717ULL);
}
#include "../CMA/CMA.hpp"

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

card_game_snapshot::card_game_snapshot() noexcept
    : format_version(0U), state_sequence(0U), player_count(0U),
      turn_number(0U), active_player(0U), current_phase_id(0U),
      event_count(0U), event_sequence(0U), random_state(0U),
      modifier_count(0U),
      event_capacity(0U), events(ft_nullptr), zones(), resources(),
      allowances(), choices(), usage_limits(), players()
{
    return ;
}

card_game_snapshot::~card_game_snapshot() noexcept
{
    if (this->events != ft_nullptr)
        cma_free(this->events);
    this->events = ft_nullptr;
    this->event_capacity = 0U;
    (void)card_game_zone_store::release_snapshot(&this->zones);
    (void)card_game_resource_ledger::release_snapshot(&this->resources);
    (void)card_game_allowance_ledger::release_snapshot(&this->allowances);
    (void)card_game_choice_ledger::release_snapshot(&this->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(
        &this->usage_limits);
    return ;
}

card_game_delta::card_game_delta() noexcept
    : format_version(0U), base_state_sequence(0U),
      target_state_sequence(0U), changed_player_mask(0U),
      global_state_changed(FT_FALSE), player_count(0U), turn_number(0U),
      active_player(0U), current_phase_id(0U), event_count(0U),
      event_sequence(0U), random_state(0U), modifier_count(0U),
      event_capacity(0U),
      events(ft_nullptr), zones(), resources(), allowances(), choices(),
      usage_limits(), players()
{
    return ;
}

card_game_delta::~card_game_delta() noexcept
{
    if (this->events != ft_nullptr)
        cma_free(this->events);
    this->events = ft_nullptr;
    this->event_capacity = 0U;
    (void)card_game_zone_store::release_snapshot(&this->zones);
    (void)card_game_resource_ledger::release_snapshot(&this->resources);
    (void)card_game_allowance_ledger::release_snapshot(&this->allowances);
    (void)card_game_choice_ledger::release_snapshot(&this->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(
        &this->usage_limits);
    return ;
}

card_game_operation_buffer::card_game_operation_buffer() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _operations(ft_nullptr),
      _capacity(0U), _count(0U)
{
    return ;
}

card_game_operation_buffer::~card_game_operation_buffer() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_operation_buffer::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_operations = static_cast<card_game_operation *>(cma_malloc(
        static_cast<ft_size_t>(FT_CARD_GAME_OPERATION_INITIAL_CAPACITY)
            * sizeof(card_game_operation)));
    if (this->_operations == ft_nullptr)
    {
        this->_capacity = 0U;
        this->_count = 0U;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_capacity = FT_CARD_GAME_OPERATION_INITIAL_CAPACITY;
    this->_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::destroy() noexcept
{
    if (this->_operations != ft_nullptr)
        cma_free(this->_operations);
    this->_operations = ft_nullptr;
    this->_capacity = 0U;
    this->_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::move(card_game_operation_buffer &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_operations = other._operations;
    this->_capacity = other._capacity;
    this->_count = other._count;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._operations = ft_nullptr;
    other._capacity = 0U;
    other._count = 0U;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::clear() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::append(
    const card_game_operation &operation) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_count >= FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_FULL);
    if (this->_count == this->_capacity)
    {
        int32_t grow_error = this->grow();

        if (grow_error != FT_ERR_SUCCESS)
            return (grow_error);
    }
    this->_operations[this->_count] = operation;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_operation_buffer::grow() noexcept
{
    card_game_operation *operations;
    uint32_t capacity;

    if (this->_capacity >= FT_CARD_GAME_MAX_OPERATIONS)
        return (FT_ERR_FULL);
    capacity = this->_capacity * 2U;
    if (capacity < this->_capacity || capacity > FT_CARD_GAME_MAX_OPERATIONS)
        capacity = FT_CARD_GAME_MAX_OPERATIONS;
    operations = static_cast<card_game_operation *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(card_game_operation)));
    if (operations == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_count != 0U)
        ft_memcpy(operations, this->_operations,
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_operation));
    if (this->_operations != ft_nullptr)
        cma_free(this->_operations);
    this->_operations = operations;
    this->_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_operation_buffer::size() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    return (this->_count);
}

int32_t card_game_operation_buffer::get(uint32_t index,
    card_game_operation *operation) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (operation == ft_nullptr || index >= this->_count)
        return (FT_ERR_INVALID_ARGUMENT);
    *operation = this->_operations[index];
    return (FT_ERR_SUCCESS);
}

card_game_engine::card_game_engine() noexcept
    : _initialised_state(0U), _rules(), _cards(ft_nullptr),
      _card_type_ids(ft_nullptr), _card_capacity(0U), _card_types(),
      _card_type_count(0U), _effects(),
      _effect_callbacks(), _effect_user_data(), _effect_event_types(),
      _effect_priorities(), _effect_usage_limit_ids(),
      _phases(), _phase_count(0U), _zones(), _zone_count(0U),
      _current_phase_id(0U), _events(ft_nullptr), _event_capacity(0U),
      _event_count(0U), _event_sequence(0U),
      _random_state(0x9E3779B97F4A7C15ULL), _card_count(0U),
      _effect_count(0U), _board(), _instances(), _board_count(), _hand(),
      _hand_count(), _decks(),
      _health(), _mana(), _turn_number(0U), _active_player(0U),
      _player_count(0U),
      _next_deck_instance_id(1U),
      _modifiers(), _modifier_count(0U), _next_modifier_id(1U),
      _state_sequence(0U), _last_command_sequence(0U),
      _command_records(ft_nullptr), _command_record_capacity(0U),
      _command_record_count(0U), _resources(), _allowances(), _choices(),
      _zone_store(), _usage_limits(),
      _start_overrides(), _start_override_count(0U)
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
    if (this->_resources.initialize() != FT_ERR_SUCCESS
        || this->_allowances.initialize() != FT_ERR_SUCCESS
        || this->_choices.initialize() != FT_ERR_SUCCESS
        || this->_zone_store.initialize() != FT_ERR_SUCCESS
        || this->_usage_limits.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_resources.destroy();
        (void)this->_allowances.destroy();
        (void)this->_choices.destroy();
        (void)this->_zone_store.destroy();
        (void)this->_usage_limits.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INTERNAL);
    }
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
            (void)this->_resources.destroy();
            (void)this->_allowances.destroy();
            (void)this->_choices.destroy();
            (void)this->_zone_store.destroy();
            (void)this->_usage_limits.destroy();
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
    this->_start_override_count = 0U;
    this->_event_count = 0U;
    this->_event_sequence = 0U;
    this->_random_state = 0x9E3779B97F4A7C15ULL;
    this->_turn_number = 0U;
    this->_active_player = 0U;
    this->_next_deck_instance_id = 1U;
    this->_modifier_count = 0U;
    this->_next_modifier_id = 1U;
    this->_state_sequence = 1U;
    this->_last_command_sequence = 0U;
    this->_card_capacity = FT_CARD_GAME_CARD_DEFINITION_INITIAL_CAPACITY;
    this->_cards = static_cast<card_game_card_definition *>(cma_malloc(
        static_cast<ft_size_t>(this->_card_capacity)
            * sizeof(card_game_card_definition)));
    this->_card_type_ids = static_cast<uint32_t *>(cma_malloc(
        static_cast<ft_size_t>(this->_card_capacity) * sizeof(uint32_t)));
    if (this->_cards == ft_nullptr || this->_card_type_ids == ft_nullptr)
    {
        if (this->_cards != ft_nullptr)
            cma_free(this->_cards);
        if (this->_card_type_ids != ft_nullptr)
            cma_free(this->_card_type_ids);
        this->_cards = ft_nullptr;
        this->_card_type_ids = ft_nullptr;
        this->_card_capacity = 0U;
        deck_index = 0U;
        while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
        {
            (void)this->_decks[deck_index].destroy();
            deck_index += 1U;
        }
        (void)this->_resources.destroy();
        (void)this->_allowances.destroy();
        (void)this->_choices.destroy();
        (void)this->_zone_store.destroy();
        (void)this->_usage_limits.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_command_record_capacity =
        FT_CARD_GAME_COMMAND_RECORD_INITIAL_CAPACITY;
    this->_command_records = static_cast<card_game_command_record *>(cma_malloc(
        static_cast<ft_size_t>(this->_command_record_capacity)
            * sizeof(card_game_command_record)));
    if (this->_command_records == ft_nullptr)
    {
        deck_index = 0U;
        while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
        {
            (void)this->_decks[deck_index].destroy();
            deck_index += 1U;
        }
        (void)this->_resources.destroy();
        (void)this->_allowances.destroy();
        (void)this->_choices.destroy();
        cma_free(this->_cards);
        cma_free(this->_card_type_ids);
        this->_cards = ft_nullptr;
        this->_card_type_ids = ft_nullptr;
        this->_card_capacity = 0U;
        this->_command_record_capacity = 0U;
        this->_initialised_state = 1U;
        return (FT_ERR_NO_MEMORY);
    }
    this->_command_record_count = 0U;
    this->_event_capacity = FT_CARD_GAME_EVENT_INITIAL_CAPACITY;
    this->_events = static_cast<card_game_event *>(cma_malloc(
        static_cast<ft_size_t>(this->_event_capacity)
            * sizeof(card_game_event)));
    if (this->_events == ft_nullptr)
    {
        deck_index = 0U;
        while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
        {
            (void)this->_decks[deck_index].destroy();
            deck_index += 1U;
        }
        (void)this->_resources.destroy();
        (void)this->_allowances.destroy();
        (void)this->_choices.destroy();
        cma_free(this->_cards);
        cma_free(this->_card_type_ids);
        this->_cards = ft_nullptr;
        this->_card_type_ids = ft_nullptr;
        this->_card_capacity = 0U;
        cma_free(this->_command_records);
        this->_command_records = ft_nullptr;
        this->_command_record_capacity = 0U;
        this->_event_capacity = 0U;
        this->_initialised_state = 1U;
        return (FT_ERR_NO_MEMORY);
    }
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
    (void)this->_resources.destroy();
    (void)this->_allowances.destroy();
    (void)this->_choices.destroy();
    (void)this->_zone_store.destroy();
    (void)this->_usage_limits.destroy();
    if (this->_command_records != ft_nullptr)
        cma_free(this->_command_records);
    this->_command_records = ft_nullptr;
    this->_command_record_capacity = 0U;
    if (this->_events != ft_nullptr)
        cma_free(this->_events);
    this->_events = ft_nullptr;
    this->_event_capacity = 0U;
    if (this->_cards != ft_nullptr)
        cma_free(this->_cards);
    if (this->_card_type_ids != ft_nullptr)
        cma_free(this->_card_type_ids);
    this->_cards = ft_nullptr;
    this->_card_type_ids = ft_nullptr;
    this->_card_capacity = 0U;
    this->_start_override_count = 0U;
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
    this->_cards = other._cards;
    this->_card_type_ids = other._card_type_ids;
    this->_card_capacity = other._card_capacity;
    ft_memcpy(this->_card_types, other._card_types,
        sizeof(this->_card_types));
    ft_memcpy(this->_effects, other._effects, sizeof(this->_effects));
    ft_memcpy(this->_effect_callbacks, other._effect_callbacks,
        sizeof(this->_effect_callbacks));
    ft_memcpy(this->_effect_user_data, other._effect_user_data,
        sizeof(this->_effect_user_data));
    ft_memcpy(this->_effect_event_types, other._effect_event_types,
        sizeof(this->_effect_event_types));
    ft_memcpy(this->_effect_priorities, other._effect_priorities,
        sizeof(this->_effect_priorities));
    ft_memcpy(this->_effect_usage_limit_ids, other._effect_usage_limit_ids,
        sizeof(this->_effect_usage_limit_ids));
    ft_memcpy(this->_phases, other._phases, sizeof(this->_phases));
    ft_memcpy(this->_zones, other._zones, sizeof(this->_zones));
    this->_events = other._events;
    this->_event_capacity = other._event_capacity;
    ft_memcpy(this->_board, other._board, sizeof(this->_board));
    ft_memcpy(this->_instances, other._instances, sizeof(this->_instances));
    ft_memcpy(this->_board_count, other._board_count, sizeof(this->_board_count));
    ft_memcpy(this->_hand, other._hand, sizeof(this->_hand));
    ft_memcpy(this->_hand_count, other._hand_count, sizeof(this->_hand_count));
    ft_memcpy(this->_health, other._health, sizeof(this->_health));
    deck_index = 0U;
    while (deck_index < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (this->_decks[deck_index].move(other._decks[deck_index])
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        deck_index += 1U;
    }
    if (this->_resources.move(other._resources) != FT_ERR_SUCCESS
        || this->_allowances.move(other._allowances) != FT_ERR_SUCCESS
        || this->_choices.move(other._choices) != FT_ERR_SUCCESS
        || this->_zone_store.move(other._zone_store) != FT_ERR_SUCCESS
        || this->_usage_limits.move(other._usage_limits) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
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
    this->_command_records = other._command_records;
    this->_command_record_capacity = other._command_record_capacity;
    this->_command_record_count = other._command_record_count;
    ft_memcpy(this->_start_overrides, other._start_overrides,
        sizeof(this->_start_overrides));
    this->_start_override_count = other._start_override_count;
    this->_phase_count = other._phase_count;
    this->_zone_count = other._zone_count;
    this->_current_phase_id = other._current_phase_id;
    this->_event_count = other._event_count;
    this->_event_sequence = other._event_sequence;
    this->_random_state = other._random_state;
    this->_initialised_state = 2U;
    other._command_records = ft_nullptr;
    other._command_record_capacity = 0U;
    other._events = ft_nullptr;
    other._event_capacity = 0U;
    other._cards = ft_nullptr;
    other._card_type_ids = ft_nullptr;
    other._card_capacity = 0U;
    other._start_override_count = 0U;
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
                        restore_error = this->apply_snapshot(before_state);
                        if (restore_error != FT_ERR_SUCCESS)
                            return (restore_error);
                        return (usage_error);
                    }
                    usage_limit_bound = FT_TRUE;
                    usage_error = this->_usage_limits.can_consume(
                        usage_limit.limit_id, 1U, context.turn_number);
                    if (usage_error != FT_ERR_SUCCESS)
                    {
                        restore_error = this->apply_snapshot(before_state);
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
                    restore_error = this->apply_snapshot(before_state);
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
                        restore_error = this->apply_snapshot(before_state);
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
                        restore_error = this->apply_snapshot(before_state);
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
                        restore_error = this->apply_snapshot(before_state);
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
                        restore_error = this->apply_snapshot(before_state);
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

int32_t card_game_engine::get_player_mana(uint32_t player_id,
    uint32_t *mana) const noexcept
{
    if (this->_initialised_state != 2U || mana == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *mana = this->_mana[player_id];
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

int32_t card_game_engine::get_hand_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_hand_count[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::hand_inspect(uint32_t player_id, uint32_t index,
    card_game_deck_card *card) const noexcept
{
    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || index >= this->_hand_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    *card = this->_hand[player_id][index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::draw_to_hand(uint32_t player_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= this->_player_count)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_hand_count[player_id] >= this->_rules.max_hand_size)
        return (FT_ERR_FULL);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    this->_hand[player_id][this->_hand_count[player_id]].instance_id =
        entry.instance_id;
    this->_hand[player_id][this->_hand_count[player_id]].card_id = entry.card_id;
    *card = this->_hand[player_id][this->_hand_count[player_id]];
    this->_hand_count[player_id] += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::hand_remove_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) noexcept
{
    uint32_t index;
    uint32_t move_index;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_hand_count[player_id]
        && this->_hand[player_id][index].instance_id != instance_id)
        index += 1U;
    if (index >= this->_hand_count[player_id])
        return (FT_ERR_NOT_FOUND);
    if (card != ft_nullptr)
        *card = this->_hand[player_id][index];
    move_index = index + 1U;
    while (move_index < this->_hand_count[player_id])
    {
        this->_hand[player_id][move_index - 1U] =
            this->_hand[player_id][move_index];
        move_index += 1U;
    }
    this->_hand_count[player_id] -= 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::mulligan_hand(uint32_t player_id,
    const uint32_t *instance_ids, uint32_t count,
    uint64_t *random_state) noexcept
{
    card_game_deck_card original_hand[FT_CARD_GAME_MAX_CARDS];
    card_game_zone_entry original_deck[FT_CARD_GAME_MAX_CARDS];
    card_game_deck_card selected[FT_CARD_GAME_MAX_CARDS];
    card_game_deck_card replacement;
    uint32_t original_hand_count;
    uint32_t original_deck_count;
    uint32_t selected_count;
    uint32_t request_index;
    uint32_t hand_index;
    uint32_t deck_index;
    uint64_t original_random_state;
    uint64_t original_state_sequence;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || random_state == ft_nullptr || *random_state == 0U
        || count > this->_hand_count[player_id]
        || (count != 0U && instance_ids == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (count == 0U)
        return (FT_ERR_SUCCESS);
    if (this->_decks[player_id].size() + count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_FULL);
    original_hand_count = this->_hand_count[player_id];
    original_deck_count = this->_decks[player_id].size();
    original_random_state = *random_state;
    original_state_sequence = this->_state_sequence;
    ft_memcpy(original_hand, this->_hand[player_id],
        sizeof(original_hand));
    deck_index = 0U;
    while (deck_index < original_deck_count)
    {
        result = this->_decks[player_id].get_entry(deck_index,
            &original_deck[deck_index]);
        if (result != FT_ERR_SUCCESS)
            return (result);
        deck_index += 1U;
    }
    selected_count = 0U;
    request_index = 0U;
    while (request_index < count)
    {
        hand_index = 0U;
        while (hand_index < original_hand_count
            && original_hand[hand_index].instance_id
                != instance_ids[request_index])
            hand_index += 1U;
        if (hand_index >= original_hand_count
            || instance_ids[request_index] == 0U)
            return (FT_ERR_NOT_FOUND);
        deck_index = 0U;
        while (deck_index < selected_count
            && selected[deck_index].instance_id != instance_ids[request_index]
            )
            deck_index += 1U;
        if (deck_index < selected_count)
            return (FT_ERR_ALREADY_EXISTS);
        selected[selected_count] = original_hand[hand_index];
        selected_count += 1U;
        request_index += 1U;
    }
    request_index = 0U;
    while (request_index < selected_count)
    {
        result = this->hand_remove_instance(player_id,
            selected[request_index].instance_id, ft_nullptr);
        if (result != FT_ERR_SUCCESS)
            break ;
        request_index += 1U;
    }
    if (result == FT_ERR_SUCCESS)
    {
        request_index = 0U;
        while (request_index < selected_count)
        {
            card_game_zone_entry entry;

            entry.instance_id = selected[request_index].instance_id;
            entry.card_id = selected[request_index].card_id;
            result = this->_decks[player_id].push_bottom_entry(entry);
            if (result != FT_ERR_SUCCESS)
                break ;
            request_index += 1U;
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = this->shuffle_deck(player_id, random_state);
    if (result == FT_ERR_SUCCESS)
    {
        request_index = 0U;
        while (request_index < selected_count)
        {
            result = this->draw_to_hand(player_id, &replacement);
            if (result != FT_ERR_SUCCESS)
                break ;
            request_index += 1U;
        }
    }
    if (result == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    this->_hand_count[player_id] = original_hand_count;
    ft_memcpy(this->_hand[player_id], original_hand, sizeof(original_hand));
    (void)this->_decks[player_id].clear();
    deck_index = 0U;
    while (deck_index < original_deck_count)
    {
        (void)this->_decks[player_id].push_bottom_entry(original_deck[deck_index]);
        deck_index += 1U;
    }
    *random_state = original_random_state;
    this->_state_sequence = original_state_sequence;
    return (result);
}

int32_t card_game_engine::play_card_from_hand(uint32_t player_id,
    uint32_t instance_id, uint32_t target_instance, void *context) noexcept
{
    card_game_deck_card card;
    card_game_snapshot before_state;
    uint32_t hand_index;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = FT_ERR_NOT_FOUND;
    hand_index = 0U;
    while (hand_index < this->_hand_count[player_id])
    {
        if (this->_hand[player_id][hand_index].instance_id == instance_id)
        {
            card = this->_hand[player_id][hand_index];
            result = FT_ERR_SUCCESS;
            break ;
        }
        hand_index += 1U;
    }
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->play_card(player_id, card.card_id, target_instance, context);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->hand_remove_instance(player_id, instance_id, ft_nullptr);
    if (result != FT_ERR_SUCCESS)
    {
        int32_t restore_error = this->apply_snapshot(before_state);

        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (result);
    }
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
        uint32_t hand_index;

        hand_index = 0U;
        while (hand_index < this->_hand_count[player_id])
        {
            if (this->_hand[player_id][hand_index].instance_id == instance_id)
                return (FT_TRUE);
            hand_index += 1U;
        }
        if (this->zone_instance_exists(player_id, instance_id) != FT_FALSE)
            return (FT_TRUE);
        player_id += 1U;
    }
    return (FT_FALSE);
}

ft_bool card_game_engine::zone_instance_exists(uint32_t player_id,
    uint32_t instance_id) const noexcept
{
    uint32_t zone_index;

    if (player_id >= FT_CARD_GAME_MAX_PLAYERS || instance_id == 0U)
        return (FT_FALSE);
    zone_index = 0U;
    while (zone_index < this->_zone_count)
    {
        if (this->_zone_store.contains(player_id,
                this->_zones[zone_index].zone_id, instance_id) != FT_FALSE)
            return (FT_TRUE);
        zone_index += 1U;
    }
    return (FT_FALSE);
}

int32_t card_game_engine::zone_push_top(uint32_t player_id,
    uint32_t zone_id, uint32_t card_id, uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_top(player_id, zone_id, entry, type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_push_bottom(uint32_t player_id,
    uint32_t zone_id, uint32_t card_id, uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_bottom(player_id, zone_id, entry,
        type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_insert_at(uint32_t player_id,
    uint32_t zone_id, uint32_t index, uint32_t card_id,
    uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_at(player_id, zone_id, index, entry,
        type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_inspect(uint32_t player_id, uint32_t zone_id,
    uint32_t index, card_game_deck_card *card) const noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.inspect(player_id, zone_id, index, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_pop_top(uint32_t player_id, uint32_t zone_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.pop_top(player_id, zone_id, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_pop_bottom(uint32_t player_id,
    uint32_t zone_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.pop_bottom(player_id, zone_id, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_remove_instance(uint32_t player_id,
    uint32_t zone_id, uint32_t instance_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.remove(player_id, zone_id, instance_id,
        &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (card != ft_nullptr)
    {
        card->instance_id = entry.instance_id;
        card->card_id = entry.card_id;
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_move_instance(uint32_t player_id,
    uint32_t source_zone_id, uint32_t destination_zone_id,
    uint32_t instance_id) noexcept
{
    card_game_deck_card card;
    uint32_t source_index;
    uint32_t source_count;
    uint32_t type_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    source_count = this->_zone_store.size(player_id, source_zone_id);
    source_index = 0U;
    while (source_index < source_count)
    {
        result = this->zone_inspect(player_id, source_zone_id, source_index,
            &card);
        if (result != FT_ERR_SUCCESS)
            return (result);
        if (card.instance_id == instance_id)
            break ;
        source_index += 1U;
    }
    if (source_index >= source_count)
        return (FT_ERR_NOT_FOUND);
    if (this->find_card_type_id(card.card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    result = this->_zone_store.move_instance(player_id, source_zone_id,
        destination_zone_id, instance_id, type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_shuffle(uint32_t player_id, uint32_t zone_id,
    uint64_t *random_state) noexcept
{
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.shuffle(player_id, zone_id, random_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_zone_count(uint32_t player_id,
    uint32_t zone_id, uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_zone_store.size(player_id, zone_id);
    return (FT_ERR_SUCCESS);
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

int32_t card_game_engine::shuffle_deck(uint32_t player_id) noexcept
{
    uint64_t original_random_state;
    int32_t result;

    if (this->_initialised_state != 2U
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    original_random_state = this->_random_state;
    result = this->_decks[player_id].shuffle(&this->_random_state);
    if (result != FT_ERR_SUCCESS)
    {
        this->_random_state = original_random_state;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::set_random_seed(uint64_t seed) noexcept
{
    if (this->_initialised_state != 2U || seed == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_random_state = seed;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_random_state(uint64_t *state) const noexcept
{
    if (this->_initialised_state != 2U || state == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *state = this->_random_state;
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
    uint32_t max_board_spaces, uint32_t max_hand_size) noexcept
{
    uint32_t index;

    if (player.board_count > max_board_spaces
        || player.board_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (player.deck_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (player.hand_count > max_hand_size
        || player.hand_count > FT_CARD_GAME_MAX_CARDS)
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
    const card_game_ordered_zone &deck, const card_game_deck_card *hand,
    uint32_t hand_count, uint32_t health, uint32_t mana) noexcept
{
    uint32_t deck_index;

    ft_memcpy(destination->board, board,
        sizeof(destination->board));
    ft_memcpy(destination->instances, instances,
        sizeof(destination->instances));
    destination->board_count = board_count;
    destination->deck_count = deck.size();
    destination->hand_count = hand_count;
    deck_index = 0U;
    while (deck_index < hand_count)
    {
        destination->hand[deck_index] = hand[deck_index].card_id;
        destination->hand_instance_ids[deck_index] =
            hand[deck_index].instance_id;
        deck_index += 1U;
    }
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
    card_game_snapshot candidate;
    uint32_t player_id;
    int32_t result;

    if (this->_initialised_state != 2U || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_event_count != 0U)
    {
        candidate.events = static_cast<card_game_event *>(cma_malloc(
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_event)));
        if (candidate.events == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(candidate.events, this->_events,
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_event));
    }
    candidate.format_version = FT_CARD_GAME_STATE_FORMAT_VERSION;
    candidate.state_sequence = this->_state_sequence;
    candidate.player_count = this->_player_count;
    candidate.turn_number = this->_turn_number;
    candidate.active_player = this->_active_player;
    candidate.current_phase_id = this->_current_phase_id;
    candidate.event_count = this->_event_count;
    candidate.event_sequence = this->_event_sequence;
    candidate.random_state = this->_random_state;
    candidate.modifier_count = this->_modifier_count;
    candidate.event_capacity = this->_event_count;
    ft_memcpy(candidate.modifiers, this->_modifiers,
        sizeof(candidate.modifiers));
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (card_game_copy_player_snapshot(&candidate.players[player_id],
            this->_board[player_id], this->_instances[player_id],
            this->_board_count[player_id], this->_decks[player_id],
            this->_hand[player_id], this->_hand_count[player_id],
            this->_health[player_id], this->_mana[player_id])
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        player_id += 1U;
    }
    result = this->_zone_store.get_snapshot(&candidate.zones);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_resources.get_snapshot(&candidate.resources);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_allowances.get_snapshot(&candidate.allowances);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_choices.get_snapshot(&candidate.choices);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_usage_limits.get_snapshot(&candidate.usage_limits);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (snapshot->events != ft_nullptr)
        cma_free(snapshot->events);
    (void)card_game_zone_store::release_snapshot(&snapshot->zones);
    (void)card_game_resource_ledger::release_snapshot(&snapshot->resources);
    (void)card_game_allowance_ledger::release_snapshot(&snapshot->allowances);
    (void)card_game_choice_ledger::release_snapshot(&snapshot->choices);
    (void)card_game_usage_limit_ledger::release_snapshot(
        &snapshot->usage_limits);
    snapshot->format_version = candidate.format_version;
    snapshot->state_sequence = candidate.state_sequence;
    snapshot->player_count = candidate.player_count;
    snapshot->turn_number = candidate.turn_number;
    snapshot->active_player = candidate.active_player;
    snapshot->current_phase_id = candidate.current_phase_id;
    snapshot->event_count = candidate.event_count;
    snapshot->event_sequence = candidate.event_sequence;
    snapshot->random_state = candidate.random_state;
    snapshot->modifier_count = candidate.modifier_count;
    snapshot->event_capacity = candidate.event_capacity;
    snapshot->events = candidate.events;
    ft_memcpy(snapshot->modifiers, candidate.modifiers,
        sizeof(snapshot->modifiers));
    ft_memcpy(snapshot->players, candidate.players,
        sizeof(snapshot->players));
    snapshot->zones = candidate.zones;
    snapshot->resources = candidate.resources;
    snapshot->allowances = candidate.allowances;
    snapshot->choices = candidate.choices;
    snapshot->usage_limits = candidate.usage_limits;
    candidate.events = ft_nullptr;
    ft_bzero(&candidate.zones, sizeof(candidate.zones));
    ft_bzero(&candidate.resources, sizeof(candidate.resources));
    ft_bzero(&candidate.allowances, sizeof(candidate.allowances));
    ft_bzero(&candidate.choices, sizeof(candidate.choices));
    ft_bzero(&candidate.usage_limits, sizeof(candidate.usage_limits));
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
    card_game_snapshot snapshot;
    uint64_t calculated_hash;
    uint32_t player_id;
    uint32_t event_index;
    uint32_t board_index;
    uint32_t deck_index;
    uint32_t hand_index;
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
    card_game_hash_u64(&calculated_hash, snapshot.random_state);
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
            snapshot.players[player_id].hand_count);
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
        hand_index = 0U;
        while (hand_index < snapshot.players[player_id].hand_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot.players[player_id].hand[hand_index]);
            card_game_hash_u32(&calculated_hash,
                snapshot.players[player_id]
                    .hand_instance_ids[hand_index]);
            hand_index += 1U;
        }
        player_id += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.zones.definition_count);
    index = 0U;
    while (index < snapshot.zones.definition_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.zones.definitions[index].zone_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.zones.definitions[index].capacity);
        card_game_hash_u32(&calculated_hash,
            snapshot.zones.definitions[index].allowed_card_type_mask);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.zones.definitions[index]
                .owner_scoped));
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.zones.entry_count);
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        index = 0U;
        while (index < FT_CARD_GAME_MAX_ZONES)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot.zones.counts[player_id][index]);
            card_game_hash_u32(&calculated_hash,
                snapshot.zones.offsets[player_id][index]);
            index += 1U;
        }
        player_id += 1U;
    }
    index = 0U;
    while (index < snapshot.zones.entry_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.zones.entries[index].instance_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.zones.entries[index].card_id);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.resources.pool_count);
    card_game_hash_u32(&calculated_hash, snapshot.resources.unit_count);
    card_game_hash_u32(&calculated_hash, snapshot.resources.next_unit_id);
    index = 0U;
    while (index < snapshot.resources.pool_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].pool_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].resource_type_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].maximum_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].current_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].locked_amount);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.pools[index].temporary_amount);
        index += 1U;
    }
    index = 0U;
    while (index < snapshot.resources.unit_count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].unit_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].resource_type_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].amount);
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].tags);
        card_game_hash_u64(&calculated_hash,
            snapshot.resources.units[index].expiry_epoch);
        card_game_hash_u64(&calculated_hash,
            snapshot.resources.units[index].unlock_epoch);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.resources.units[index].temporary));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.resources.units[index].locked));
        card_game_hash_u32(&calculated_hash,
            snapshot.resources.units[index].locked_amount);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.allowances.count);
    card_game_hash_u32(&calculated_hash, snapshot.allowances.next_id);
    index = 0U;
    while (index < snapshot.allowances.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].allowance_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].owner_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].action_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].action_tags);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].remaining_uses);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].maximum_uses);
        card_game_hash_u64(&calculated_hash,
            snapshot.allowances.allowances[index].expiry_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].source_instance);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].source_effect_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].predicate_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.allowances.allowances[index].predicate_context_id);
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.choices.count);
    card_game_hash_u32(&calculated_hash, snapshot.choices.next_id);
    index = 0U;
    while (index < snapshot.choices.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.choices.choices[index].choice_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.choices.choices[index].player_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.choices.choices[index].kind));
        card_game_hash_u32(&calculated_hash,
            snapshot.choices.choices[index].option_count);
        card_game_hash_u64(&calculated_hash,
            snapshot.choices.choices[index].deadline_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot.choices.choices[index].default_option_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.choices.choices[index].selected_option_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.choices.choices[index].resolved));
        uint32_t option_index = 0U;
        while (option_index < snapshot.choices.choices[index].option_count)
        {
            card_game_hash_u32(&calculated_hash,
                snapshot.choices.choices[index].options[option_index].option_id);
            card_game_hash_u32(&calculated_hash,
                snapshot.choices.choices[index].options[option_index].value_a);
            card_game_hash_u32(&calculated_hash,
                snapshot.choices.choices[index].options[option_index].value_b);
            option_index += 1U;
        }
        index += 1U;
    }
    card_game_hash_u32(&calculated_hash, snapshot.usage_limits.count);
    card_game_hash_u32(&calculated_hash, snapshot.usage_limits.next_id);
    index = 0U;
    while (index < snapshot.usage_limits.count)
    {
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].limit_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].key_id);
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].subject_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.usage_limits.limits[index].scope));
        card_game_hash_u64(&calculated_hash,
            snapshot.usage_limits.limits[index].window_epoch);
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].maximum_uses);
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].used_uses);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(snapshot.usage_limits.limits[index]
                .attempt_policy));
        card_game_hash_u32(&calculated_hash,
            snapshot.usage_limits.limits[index].source_instance);
        index += 1U;
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
    uint32_t hand_index;
    uint32_t instance_id;
    uint32_t previous_index;
    uint32_t index;
    uint32_t next_deck_instance_id;
    uint32_t next_modifier_id;
    uint32_t previous_player_id;
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
    next_deck_instance_id = 1U;
    next_modifier_id = 1U;
    index = 0U;
    while (index < snapshot.modifier_count)
    {
        if (snapshot.modifiers[index].modifier_id
                >= next_modifier_id
            && snapshot.modifiers[index].modifier_id != UINT32_MAX)
            next_modifier_id = snapshot.modifiers[index].modifier_id + 1U;
        index += 1U;
    }
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
        delta->global_state_changed = FT_TRUE;
    player_id = 0U;
    while (player_id < current_snapshot.player_count)
    {
        if (ft_memcmp(&baseline.players[player_id],
            &current_snapshot.players[player_id],
            sizeof(card_game_player_snapshot)) != 0)
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

int32_t card_game_engine::apply_delta(const card_game_delta &delta) noexcept
{
    uint32_t player_id;
    uint32_t deck_index;
    uint32_t hand_index;
    uint32_t next_modifier_id;
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
    int32_t release_error;

    ft_bzero(&current_zones, sizeof(current_zones));
    ft_bzero(&current_resources, sizeof(current_resources));
    ft_bzero(&current_allowances, sizeof(current_allowances));
    ft_bzero(&current_choices, sizeof(current_choices));
    ft_bzero(&current_usage_limits, sizeof(current_usage_limits));

    if (this->_initialised_state != 2U
        || delta.format_version != FT_CARD_GAME_STATE_FORMAT_VERSION
        || delta.base_state_sequence != this->_state_sequence
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
    if (delta.global_state_changed == FT_FALSE
        && (delta.modifier_count != this->_modifier_count
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
        next_modifier_id = 1U;
        player_id = 0U;
        while (player_id < delta.modifier_count)
        {
            if (delta.modifiers[player_id].modifier_id
                >= next_modifier_id
                && delta.modifiers[player_id].modifier_id != UINT32_MAX)
                next_modifier_id = delta.modifiers[player_id].modifier_id
                    + 1U;
            player_id += 1U;
        }
        this->_turn_number = delta.turn_number;
        this->_active_player = delta.active_player;
        this->_current_phase_id = delta.current_phase_id;
        this->_event_count = delta.event_count;
        this->_event_sequence = delta.event_sequence;
        this->_random_state = delta.random_state;
        this->_modifier_count = delta.modifier_count;
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
    card_game_command_record *records;
    uint32_t magic;
    uint32_t version;
    uint32_t record_count;
    uint32_t required_size;
    uint32_t offset;
    uint32_t index;
    uint32_t type;
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
    if (record_count != 0U)
    {
        records = static_cast<card_game_command_record *>(cma_malloc(
            static_cast<ft_size_t>(record_count)
                * sizeof(card_game_command_record)));
        if (records == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    previous_sequence = 0U;
    index = 0U;
    while (index < record_count)
    {
        card_game_command_record *record;

        record = &records[index];
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->command.command_sequence);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
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
        if (read_error != FT_ERR_SUCCESS || type < 1U || type > 3U
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
        read_error = card_game_replay_read_u64(input, input_size, &offset,
            &record->rules_hash);
        if (read_error != FT_ERR_SUCCESS)
        {
            cma_free(records);
            return (read_error);
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

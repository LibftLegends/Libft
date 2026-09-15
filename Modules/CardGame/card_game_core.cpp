#include "card_game_internal.hpp"

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


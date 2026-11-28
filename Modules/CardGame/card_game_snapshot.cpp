#include "card_game_internal.hpp"
#include "../Buffer/buffer_endian.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstring>
#include <limits>
#include <new>

namespace
{
	static const uint32_t CARD_GAME_SNAPSHOT_SERIAL_VERSION = 2U;
	static const uint32_t CARD_GAME_SNAPSHOT_SERIAL_VERSION_LEGACY = 1U;
	static const std::size_t CARD_GAME_SNAPSHOT_HEADER_BYTES = 16U;
	static const std::size_t CARD_GAME_SNAPSHOT_CHECKSUM_BYTES = 8U;

    class card_game_snapshot_writer
    {
        private:
            std::vector<uint8_t> &_bytes;
            std::size_t _offset;

        public:
            explicit card_game_snapshot_writer(std::vector<uint8_t> &bytes) noexcept;
            ft_bool u8(uint8_t value) noexcept;
            ft_bool u32(uint32_t value) noexcept;
            ft_bool u64(uint64_t value) noexcept;
            std::size_t offset() const noexcept;
    };

    class card_game_snapshot_reader
    {
        private:
            const uint8_t *_bytes;
            std::size_t _size;
            std::size_t _offset;

        public:
            card_game_snapshot_reader(const uint8_t *bytes, std::size_t size) noexcept;
            ft_bool u8(uint8_t *value) noexcept;
            ft_bool u32(uint32_t *value) noexcept;
            ft_bool u64(uint64_t *value) noexcept;
            std::size_t remaining() const noexcept;
    };

    card_game_snapshot_writer::card_game_snapshot_writer(std::vector<uint8_t> &bytes) noexcept
        : _bytes(bytes), _offset(0U)
    {
        return ;
    }

    ft_bool card_game_snapshot_writer::u8(uint8_t value) noexcept
    {
        if (this->_offset == this->_bytes.size())
            return (FT_FALSE);
        this->_bytes[this->_offset++] = value;
        return (FT_TRUE);
    }

    ft_bool card_game_snapshot_writer::u32(uint32_t value) noexcept
    {
        if (this->_bytes.size() - this->_offset < sizeof(value))
            return (FT_FALSE);
        buffer_store_le<uint32_t>(this->_bytes.data() + this->_offset, value);
        this->_offset += sizeof(value);
        return (FT_TRUE);
    }

    ft_bool card_game_snapshot_writer::u64(uint64_t value) noexcept
    {
        if (this->_bytes.size() - this->_offset < sizeof(value))
            return (FT_FALSE);
        buffer_store_le<uint64_t>(this->_bytes.data() + this->_offset, value);
        this->_offset += sizeof(value);
        return (FT_TRUE);
    }

    std::size_t card_game_snapshot_writer::offset() const noexcept
    {
        return (this->_offset);
    }

    card_game_snapshot_reader::card_game_snapshot_reader(const uint8_t *bytes, std::size_t size) noexcept
        : _bytes(bytes), _size(size), _offset(0U)
    {
        return ;
    }

    ft_bool card_game_snapshot_reader::u8(uint8_t *value) noexcept
    {
        if (value == ft_nullptr || this->_offset == this->_size)
            return (FT_FALSE);
        *value = this->_bytes[this->_offset++];
        return (FT_TRUE);
    }

    ft_bool card_game_snapshot_reader::u32(uint32_t *value) noexcept
    {
        if (value == ft_nullptr || this->_size - this->_offset < sizeof(*value))
            return (FT_FALSE);
        *value = buffer_load_le<uint32_t>(this->_bytes + this->_offset);
        this->_offset += sizeof(*value);
        return (FT_TRUE);
    }

    ft_bool card_game_snapshot_reader::u64(uint64_t *value) noexcept
    {
        if (value == ft_nullptr || this->_size - this->_offset < sizeof(*value))
            return (FT_FALSE);
        *value = buffer_load_le<uint64_t>(this->_bytes + this->_offset);
        this->_offset += sizeof(*value);
        return (FT_TRUE);
    }

    std::size_t card_game_snapshot_reader::remaining() const noexcept
    {
        return (this->_size - this->_offset);
    }

	static uint64_t card_game_snapshot_checksum(const uint8_t *bytes,
		std::size_t size) noexcept
	{
		uint64_t hash = 14695981039346656037ULL;
		std::size_t index;

		for (index = 0U; index < size; ++index)
		{
			hash ^= bytes[index];
			hash *= 1099511628211ULL;
		}
		return (hash);
	}

	static bool write_instance(card_game_snapshot_writer &writer,
		const card_game_card_instance &value)
	{
		return (writer.u32(value.instance_id)
			&& writer.u32(value.definition_id) && writer.u32(value.owner_id)
			&& writer.u32(static_cast<uint32_t>(value.attack))
			&& writer.u32(static_cast<uint32_t>(value.health))
			&& writer.u32(static_cast<uint32_t>(value.damage_taken))
			&& writer.u32(static_cast<uint32_t>(value.on_board)));
	}

	static bool read_instance(card_game_snapshot_reader &reader,
		card_game_card_instance *value) noexcept
	{
		uint32_t attack;
		uint32_t health;
		uint32_t damage;
		uint32_t on_board;

		if (value == ft_nullptr || !reader.u32(&value->instance_id)
			|| !reader.u32(&value->definition_id)
			|| !reader.u32(&value->owner_id) || !reader.u32(&attack)
			|| !reader.u32(&health) || !reader.u32(&damage)
			|| !reader.u32(&on_board) || on_board > 1U)
			return (false);
		std::memcpy(&value->attack, &attack, sizeof(attack));
		std::memcpy(&value->health, &health, sizeof(health));
		std::memcpy(&value->damage_taken, &damage, sizeof(damage));
		value->on_board = static_cast<ft_bool>(on_board);
		return (true);
	}

	static bool write_modifier(card_game_snapshot_writer &writer,
		const card_game_card_modifier &value)
	{
		return (writer.u32(value.modifier_id)
			&& writer.u32(value.source_effect_id)
			&& writer.u32(value.target_player_id)
			&& writer.u32(value.target_instance_id)
			&& writer.u32(value.target_instance_index)
			&& writer.u32(static_cast<uint32_t>(value.attack_delta))
			&& writer.u32(static_cast<uint32_t>(value.health_delta))
			&& writer.u32(static_cast<uint32_t>(value.duration))
			&& writer.u32(value.created_turn)
			&& writer.u32(value.created_phase_id));
	}

	static bool read_modifier(card_game_snapshot_reader &reader,
		card_game_card_modifier *value) noexcept
	{
		uint32_t attack;
		uint32_t health;
		uint32_t duration;

		if (value == ft_nullptr || !reader.u32(&value->modifier_id)
			|| !reader.u32(&value->source_effect_id)
			|| !reader.u32(&value->target_player_id)
			|| !reader.u32(&value->target_instance_id)
			|| !reader.u32(&value->target_instance_index)
			|| !reader.u32(&attack) || !reader.u32(&health)
			|| !reader.u32(&duration)
			|| duration > static_cast<uint32_t>(
				CARD_GAME_MODIFIER_UNTIL_END_TURN)
			|| !reader.u32(&value->created_turn)
			|| !reader.u32(&value->created_phase_id))
			return (false);
		std::memcpy(&value->attack_delta, &attack, sizeof(attack));
		std::memcpy(&value->health_delta, &health, sizeof(health));
		value->duration = static_cast<card_game_modifier_duration>(duration);
		return (true);
	}

	static bool write_host_intent(card_game_snapshot_writer &writer,
		const card_game_host_intent_record &record)
	{
		uint32_t field;

		if (record.intent.field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS
			|| !writer.u64(record.sequence)
			|| !writer.u64(record.action_request_id)
			|| !writer.u32(record.intent.type_id)
			|| !writer.u32(record.intent.version)
			|| !writer.u32(record.intent.field_count))
			return (false);
		for (field = 0U; field < record.intent.field_count; ++field)
			if (!writer.u32(static_cast<uint32_t>(
					record.intent.fields[field].type))
				|| !writer.u64(record.intent.fields[field].value))
				return (false);
		return (true);
	}

	static bool read_host_intent(card_game_snapshot_reader &reader,
		card_game_host_intent_record *record, uint32_t snapshot_version) noexcept
	{
		uint32_t field;
		uint32_t type;

		if (record == ft_nullptr || !reader.u64(&record->sequence)
			|| (snapshot_version >= CARD_GAME_SNAPSHOT_SERIAL_VERSION
				&& !reader.u64(&record->action_request_id))
			|| !reader.u32(&record->intent.type_id)
			|| !reader.u32(&record->intent.version)
			|| !reader.u32(&record->intent.field_count)
			|| record->intent.field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
			return (false);
		if (snapshot_version == CARD_GAME_SNAPSHOT_SERIAL_VERSION_LEGACY)
			record->action_request_id = 0U;
		for (field = 0U; field < record->intent.field_count; ++field)
		{
			if (!reader.u32(&type)
				|| type < CARD_GAME_HOST_INTENT_U32
				|| type > CARD_GAME_HOST_INTENT_BOOL
				|| !reader.u64(&record->intent.fields[field].value))
				return (false);
			record->intent.fields[field].type =
				static_cast<card_game_host_intent_scalar_type>(type);
		}
		return (true);
	}

	static bool write_pool(card_game_snapshot_writer &writer,
		const card_game_resource_pool &value)
	{
		return (writer.u32(value.pool_id) && writer.u32(value.owner_id)
			&& writer.u32(value.resource_type_id)
			&& writer.u32(value.maximum_amount)
			&& writer.u32(value.current_amount)
			&& writer.u32(value.locked_amount)
			&& writer.u32(value.temporary_amount));
	}

	static bool read_pool(card_game_snapshot_reader &reader,
		card_game_resource_pool *value) noexcept
	{
		return (value != ft_nullptr && reader.u32(&value->pool_id)
			&& reader.u32(&value->owner_id)
			&& reader.u32(&value->resource_type_id)
			&& reader.u32(&value->maximum_amount)
			&& reader.u32(&value->current_amount)
			&& reader.u32(&value->locked_amount)
			&& reader.u32(&value->temporary_amount));
	}

	static bool write_unit(card_game_snapshot_writer &writer,
		const card_game_resource_unit &value)
	{
		return (writer.u32(value.unit_id) && writer.u32(value.owner_id)
			&& writer.u32(value.resource_type_id) && writer.u32(value.amount)
			&& writer.u32(value.tags) && writer.u64(value.expiry_epoch)
			&& writer.u64(value.unlock_epoch)
			&& writer.u32(static_cast<uint32_t>(value.temporary))
			&& writer.u32(static_cast<uint32_t>(value.locked))
			&& writer.u32(value.locked_amount));
	}

	static bool read_unit(card_game_snapshot_reader &reader,
		card_game_resource_unit *value) noexcept
	{
		uint32_t temporary;
		uint32_t locked;

		if (value == ft_nullptr || !reader.u32(&value->unit_id)
			|| !reader.u32(&value->owner_id)
			|| !reader.u32(&value->resource_type_id)
			|| !reader.u32(&value->amount) || !reader.u32(&value->tags)
			|| !reader.u64(&value->expiry_epoch)
			|| !reader.u64(&value->unlock_epoch)
			|| !reader.u32(&temporary) || temporary > 1U
			|| !reader.u32(&locked) || locked > 1U
			|| !reader.u32(&value->locked_amount))
			return (false);
		value->temporary = static_cast<ft_bool>(temporary);
		value->locked = static_cast<ft_bool>(locked);
		return (true);
	}

	static bool write_allowance(card_game_snapshot_writer &writer,
		const card_game_action_allowance &value)
	{
		return (writer.u32(value.allowance_id) && writer.u32(value.owner_id)
			&& writer.u32(value.action_id) && writer.u32(value.action_tags)
			&& writer.u32(value.remaining_uses)
			&& writer.u32(value.maximum_uses)
			&& writer.u64(value.expiry_epoch)
			&& writer.u32(value.source_instance)
			&& writer.u32(value.source_effect_id)
			&& writer.u32(value.predicate_id)
			&& writer.u32(value.predicate_context_id));
	}

	static bool read_allowance(card_game_snapshot_reader &reader,
		card_game_action_allowance *value) noexcept
	{
		return (value != ft_nullptr && reader.u32(&value->allowance_id)
			&& reader.u32(&value->owner_id) && reader.u32(&value->action_id)
			&& reader.u32(&value->action_tags)
			&& reader.u32(&value->remaining_uses)
			&& reader.u32(&value->maximum_uses)
			&& reader.u64(&value->expiry_epoch)
			&& reader.u32(&value->source_instance)
			&& reader.u32(&value->source_effect_id)
			&& reader.u32(&value->predicate_id)
			&& reader.u32(&value->predicate_context_id));
	}

	static bool write_choice(card_game_snapshot_writer &writer,
		const card_game_choice &value)
	{
		uint32_t option;

		if (value.option_count > FT_CARD_GAME_MAX_CHOICE_OPTIONS
			|| !writer.u32(value.choice_id) || !writer.u32(value.player_id)
			|| !writer.u32(static_cast<uint32_t>(value.kind))
			|| !writer.u32(value.option_count))
			return (false);
		for (option = 0U; option < value.option_count; ++option)
			if (!writer.u32(value.options[option].option_id)
				|| !writer.u32(value.options[option].value_a)
				|| !writer.u32(value.options[option].value_b))
				return (false);
		return (writer.u64(value.deadline_epoch)
			&& writer.u32(value.default_option_id)
			&& writer.u32(value.selected_option_id)
			&& writer.u32(static_cast<uint32_t>(value.resolved)));
	}

	static bool read_choice(card_game_snapshot_reader &reader,
		card_game_choice *value) noexcept
	{
		uint32_t kind;
		uint32_t option;
		uint32_t resolved;

		if (value == ft_nullptr || !reader.u32(&value->choice_id)
			|| !reader.u32(&value->player_id) || !reader.u32(&kind)
			|| kind < CARD_GAME_CHOICE_TARGET
			|| kind > CARD_GAME_CHOICE_MULLIGAN
			|| !reader.u32(&value->option_count)
			|| value->option_count > FT_CARD_GAME_MAX_CHOICE_OPTIONS)
			return (false);
		value->kind = static_cast<card_game_choice_kind>(kind);
		for (option = 0U; option < value->option_count; ++option)
			if (!reader.u32(&value->options[option].option_id)
				|| !reader.u32(&value->options[option].value_a)
				|| !reader.u32(&value->options[option].value_b))
				return (false);
		if (!reader.u64(&value->deadline_epoch)
			|| !reader.u32(&value->default_option_id)
			|| !reader.u32(&value->selected_option_id)
			|| !reader.u32(&resolved) || resolved > 1U)
			return (false);
		value->resolved = static_cast<ft_bool>(resolved);
		return (true);
	}

	static bool write_usage_limit(card_game_snapshot_writer &writer,
		const card_game_usage_limit &value)
	{
		return (writer.u32(value.limit_id) && writer.u32(value.key_id)
			&& writer.u32(value.subject_id)
			&& writer.u32(static_cast<uint32_t>(value.scope))
			&& writer.u64(value.window_epoch)
			&& writer.u32(value.maximum_uses)
			&& writer.u32(value.used_uses)
			&& writer.u32(static_cast<uint32_t>(value.attempt_policy))
			&& writer.u32(value.source_instance));
	}

	static bool read_usage_limit(card_game_snapshot_reader &reader,
		card_game_usage_limit *value) noexcept
	{
		uint32_t scope;
		uint32_t policy;

		if (value == ft_nullptr || !reader.u32(&value->limit_id)
			|| !reader.u32(&value->key_id)
			|| !reader.u32(&value->subject_id) || !reader.u32(&scope)
			|| scope > CARD_GAME_USAGE_CUSTOM
			|| !reader.u64(&value->window_epoch)
			|| !reader.u32(&value->maximum_uses)
			|| !reader.u32(&value->used_uses) || !reader.u32(&policy)
			|| policy > CARD_GAME_USAGE_ON_RESOLUTION
			|| !reader.u32(&value->source_instance))
			return (false);
		value->scope = static_cast<card_game_usage_scope>(scope);
		value->attempt_policy =
			static_cast<card_game_usage_attempt_policy>(policy);
		return (true);
	}

    template <typename Record>
    ft_bool allocate_array(Record *&memory, uint32_t count) noexcept
    {
        if (count == 0U)
            return (FT_TRUE);
        if (count > std::numeric_limits<ft_size_t>::max() / sizeof(Record))
            return (FT_FALSE);
        memory = static_cast<Record *>(cma_malloc(static_cast<ft_size_t>(count) * sizeof(Record)));
        return (memory != ft_nullptr);
    }
}

card_game_host_intent_snapshot::card_game_host_intent_snapshot() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _records(ft_nullptr),
      _count(0U), _capacity(0U)
{
    return ;
}

card_game_host_intent_snapshot::~card_game_host_intent_snapshot() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_host_intent_snapshot::initialize(
    const card_game_host_intent_record *records, uint32_t count) noexcept
{
    return (this->initialize_ranges(records, count, ft_nullptr, 0U));
}

int32_t card_game_host_intent_snapshot::initialize_ranges(
    const card_game_host_intent_record *first, uint32_t first_count,
    const card_game_host_intent_record *second, uint32_t second_count) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "card_game_host_intent_snapshot::initialize",
            "called while object is already initialised");
    if (first_count > FT_CARD_GAME_MAX_OPERATIONS
        || second_count > FT_CARD_GAME_MAX_OPERATIONS - first_count
        || (first_count != 0U && first == ft_nullptr)
        || (second_count != 0U && second == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t count = first_count + second_count;
    const int32_t result = this->initialize_storage(count);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (count != 0U)
    {
        ft_memcpy(this->_records, first,
            static_cast<ft_size_t>(first_count) * sizeof(*this->_records));
        ft_memcpy(this->_records + first_count, second,
            static_cast<ft_size_t>(second_count) * sizeof(*this->_records));
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_host_intent_snapshot::initialize_storage(uint32_t count) noexcept
{
    card_game_host_intent_record *owned_records = ft_nullptr;
    if (count != 0U)
    {
        owned_records = static_cast<card_game_host_intent_record *>(
            cma_malloc(static_cast<ft_size_t>(count)
                * sizeof(card_game_host_intent_record)));
        if (owned_records == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (FT_ERR_NO_MEMORY);
        }
    }
    this->_records = owned_records;
    this->_count = count;
    this->_capacity = count;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_host_intent_snapshot::initialize(
    const card_game_host_intent_snapshot &other) noexcept
{
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    return (this->initialize(other._records, other._count));
}

int32_t card_game_host_intent_snapshot::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    if (this->_records != ft_nullptr)
        cma_free(this->_records);
    this->_records = ft_nullptr;
    this->_count = 0U;
    this->_capacity = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_host_intent_snapshot::move(
    card_game_host_intent_snapshot &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "card_game_host_intent_snapshot::move",
            "source object is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    else if (this->_initialised_state != FT_CLASS_STATE_UNINITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_INVALID_STATE);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_records = ft_nullptr;
        this->_count = 0U;
        this->_capacity = 0U;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_records = other._records;
    this->_count = other._count;
    this->_capacity = other._capacity;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._records = ft_nullptr;
    other._count = 0U;
    other._capacity = 0U;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_host_intent_snapshot::get_count(uint32_t *count) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    *count = this->_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_host_intent_snapshot::get_record(uint32_t index,
    card_game_host_intent_record *record) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || record == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    if (index >= this->_count)
        return (FT_ERR_OUT_OF_RANGE);
    *record = this->_records[index];
    return (FT_ERR_SUCCESS);
}

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
    const uint32_t first_count = this->host_intent_first_range_count();
    const card_game_host_intent_record *first_records = this->_host_intent_records;
    if (first_count != 0U)
        first_records += this->_host_intent_record_head;
    result = candidate->host_intents.initialize_ranges(first_records, first_count,
        this->_host_intent_records, this->_host_intent_record_count - first_count);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
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
    candidate->next_host_intent_sequence =
        this->_next_host_intent_sequence;
    candidate->pending_discard_player_id =
        this->_pending_discard_player_id;
    candidate->pending_discard_count = this->_pending_discard_count;
    candidate->pending_discard_zone_id = this->_pending_discard_zone_id;
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
    (void)snapshot->host_intents.destroy();
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
    snapshot->next_host_intent_sequence =
        candidate->next_host_intent_sequence;
    snapshot->pending_discard_player_id =
        candidate->pending_discard_player_id;
    snapshot->pending_discard_count = candidate->pending_discard_count;
    snapshot->pending_discard_zone_id = candidate->pending_discard_zone_id;
    result = snapshot->host_intents.move(candidate->host_intents);
    if (result != FT_ERR_SUCCESS)
    {
        card_game_destroy_snapshot(candidate);
        return (result);
    }
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

static int32_t card_game_snapshot_wire_size(const card_game_snapshot &snapshot,
    uint32_t host_count, std::size_t *size) noexcept
{
    constexpr uint64_t U32 = sizeof(uint32_t);
    constexpr uint64_t U64 = sizeof(uint64_t);
    uint64_t bytes = CARD_GAME_SNAPSHOT_HEADER_BYTES + CARD_GAME_SNAPSHOT_CHECKSUM_BYTES
        + 12U * U32 + 4U * U64;  // Global state.
    bytes += snapshot.modifier_count * 10U * U32;
    bytes += snapshot.event_count * (U64 + 3U * U32);
    bytes += U32 + host_count * (2U * U64 + 3U * U32);
    uint32_t index = 0U;
    while (index < host_count)
    {
        card_game_host_intent_record record;
        if (snapshot.host_intents.get_record(index, &record) != FT_ERR_SUCCESS
            || record.intent.field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
            return (FT_ERR_INVALID_STATE);
        bytes += record.intent.field_count * (U32 + U64);
        ++index;
    }
    index = 0U;
    while (index < FT_CARD_GAME_MAX_PLAYERS)
    {
        const card_game_player_snapshot &player = snapshot.players[index];
        bytes += 5U * U32 + player.board_count * 8U * U32
            + (static_cast<uint64_t>(player.deck_count) + player.hand_count) * 2U * U32;
        ++index;
    }
    bytes += (3U + 2U * FT_CARD_GAME_MAX_PLAYERS * FT_CARD_GAME_MAX_ZONES) * U32
        + snapshot.zones.definition_count * 4U * U32
        + snapshot.zones.entry_count * 2U * U32;
    bytes += 3U * U32 + snapshot.resources.pool_count * 7U * U32
        + snapshot.resources.unit_count * (8U * U32 + 2U * U64);
    bytes += 2U * U32 + snapshot.allowances.count * (10U * U32 + U64);
    bytes += 2U * U32 + snapshot.choices.count * (7U * U32 + U64);
    index = 0U;
    while (index < snapshot.choices.count)
    {
        const uint32_t count = snapshot.choices.choices[index].option_count;
        if (count > FT_CARD_GAME_MAX_CHOICE_OPTIONS)
            return (FT_ERR_OUT_OF_RANGE);
        bytes += count * 3U * U32;
        ++index;
    }
    bytes += 3U * U32 + snapshot.usage_limits.count * (8U * U32 + U64);
    if (bytes > FT_CARD_GAME_MAX_SNAPSHOT_BYTES)
        return (FT_ERR_OUT_OF_RANGE);
    *size = static_cast<std::size_t>(bytes);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::serialize_snapshot(
	std::vector<uint8_t> &bytes_out) const noexcept
{
	card_game_snapshot snapshot;
	std::vector<uint8_t> staged;
	card_game_snapshot_writer writer(staged);
	uint64_t rules_hash;
	uint32_t host_count;
	uint32_t index;
	uint32_t player;
	uint32_t zone;
	card_game_host_intent_record intent;
	int32_t result;

	result = this->get_snapshot(&snapshot);
	if (result != FT_ERR_SUCCESS)
		return (result);
	if (snapshot.modifier_count > FT_CARD_GAME_MAX_MODIFIERS
		|| snapshot.event_count > FT_CARD_GAME_MAX_EVENTS
		|| snapshot.player_count > FT_CARD_GAME_MAX_PLAYERS
		|| snapshot.host_intents.get_count(&host_count) != FT_ERR_SUCCESS
		|| host_count > FT_CARD_GAME_MAX_OPERATIONS
		|| this->get_rules_hash(&rules_hash) != FT_ERR_SUCCESS)
		return (FT_ERR_INVALID_STATE);
    std::size_t wire_size;
    result = card_game_snapshot_wire_size(snapshot, host_count, &wire_size);
    if (result != FT_ERR_SUCCESS)
        return (result);
    try
    {
        staged.resize(wire_size);
		if (!writer.u8('G') || !writer.u8('C') || !writer.u8('G')
			|| !writer.u8('S')
			|| !writer.u32(CARD_GAME_SNAPSHOT_SERIAL_VERSION)
			|| !writer.u64(rules_hash)
			|| !writer.u32(snapshot.format_version)
			|| !writer.u64(snapshot.state_sequence)
			|| !writer.u32(snapshot.player_count)
			|| !writer.u32(snapshot.turn_number)
			|| !writer.u32(snapshot.active_player)
			|| !writer.u32(snapshot.current_phase_id)
			|| !writer.u32(snapshot.event_count)
			|| !writer.u64(snapshot.event_sequence)
			|| !writer.u64(snapshot.random_state)
			|| !writer.u32(snapshot.next_deck_instance_id)
			|| !writer.u32(snapshot.next_modifier_id)
			|| !writer.u32(snapshot.modifier_count)
			|| !writer.u64(snapshot.next_host_intent_sequence)
			|| !writer.u32(snapshot.pending_discard_player_id)
			|| !writer.u32(snapshot.pending_discard_count)
			|| !writer.u32(snapshot.pending_discard_zone_id))
			return (FT_ERR_OUT_OF_RANGE);
		for (index = 0U; index < snapshot.modifier_count; ++index)
			if (!write_modifier(writer, snapshot.modifiers[index]))
				return (FT_ERR_OUT_OF_RANGE);
		for (index = 0U; index < snapshot.event_count; ++index)
			if (!writer.u64(snapshot.events[index].sequence)
				|| !writer.u32(snapshot.events[index].event_type)
				|| !writer.u32(snapshot.events[index].source_instance)
				|| !writer.u32(snapshot.events[index].target_instance))
				return (FT_ERR_OUT_OF_RANGE);
		if (!writer.u32(host_count))
			return (FT_ERR_OUT_OF_RANGE);
		for (index = 0U; index < host_count; ++index)
		{
			if (snapshot.host_intents.get_record(index, &intent)
				!= FT_ERR_SUCCESS || !write_host_intent(writer, intent))
				return (FT_ERR_INVALID_STATE);
		}
		for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
		{
			const card_game_player_snapshot &value = snapshot.players[player];

			if (value.board_count > FT_CARD_GAME_MAX_CARDS
				|| value.deck_count > FT_CARD_GAME_MAX_CARDS
				|| value.hand_count > FT_CARD_GAME_MAX_CARDS
				|| !writer.u32(value.board_count)
				|| !writer.u32(value.deck_count)
				|| !writer.u32(value.hand_count)
				|| !writer.u32(value.health) || !writer.u32(value.mana))
				return (FT_ERR_INVALID_STATE);
			for (index = 0U; index < value.board_count; ++index)
				if (!writer.u32(value.board[index])
					|| !write_instance(writer, value.instances[index]))
					return (FT_ERR_OUT_OF_RANGE);
			for (index = 0U; index < value.deck_count; ++index)
				if (!writer.u32(value.deck[index])
					|| !writer.u32(value.deck_instance_ids[index]))
					return (FT_ERR_OUT_OF_RANGE);
			for (index = 0U; index < value.hand_count; ++index)
				if (!writer.u32(value.hand[index])
					|| !writer.u32(value.hand_instance_ids[index]))
					return (FT_ERR_OUT_OF_RANGE);
		}
		if (snapshot.zones.definition_count > FT_CARD_GAME_MAX_ZONES
			|| snapshot.zones.entry_count > snapshot.zones.entry_capacity
			|| (snapshot.zones.entry_count != 0U
				&& snapshot.zones.entries == ft_nullptr)
			|| !writer.u32(snapshot.zones.definition_count))
			return (FT_ERR_INVALID_STATE);
		for (index = 0U; index < snapshot.zones.definition_count; ++index)
		{
			const card_game_zone_store_definition &definition =
				snapshot.zones.definitions[index];

			if (!writer.u32(definition.zone_id)
				|| !writer.u32(definition.capacity)
				|| !writer.u32(definition.allowed_card_type_mask)
				|| !writer.u32(static_cast<uint32_t>(definition.owner_scoped)))
				return (FT_ERR_OUT_OF_RANGE);
		}
		for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
			for (zone = 0U; zone < FT_CARD_GAME_MAX_ZONES; ++zone)
				if (!writer.u32(snapshot.zones.counts[player][zone]))
					return (FT_ERR_OUT_OF_RANGE);
		for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
			for (zone = 0U; zone < FT_CARD_GAME_MAX_ZONES; ++zone)
				if (!writer.u32(snapshot.zones.offsets[player][zone]))
					return (FT_ERR_OUT_OF_RANGE);
		if (!writer.u32(snapshot.zones.entry_count)
			|| !writer.u32(snapshot.zones.entry_capacity))
			return (FT_ERR_OUT_OF_RANGE);
		for (index = 0U; index < snapshot.zones.entry_count; ++index)
			if (!writer.u32(snapshot.zones.entries[index].instance_id)
				|| !writer.u32(snapshot.zones.entries[index].card_id))
				return (FT_ERR_OUT_OF_RANGE);
		if (snapshot.resources.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
			|| snapshot.resources.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
			|| (snapshot.resources.pool_count != 0U
				&& snapshot.resources.pools == ft_nullptr)
			|| (snapshot.resources.unit_count != 0U
				&& snapshot.resources.units == ft_nullptr)
			|| !writer.u32(snapshot.resources.pool_count)
			|| !writer.u32(snapshot.resources.unit_count)
			|| !writer.u32(snapshot.resources.next_unit_id))
			return (FT_ERR_INVALID_STATE);
		for (index = 0U; index < snapshot.resources.pool_count; ++index)
			if (!write_pool(writer, snapshot.resources.pools[index]))
				return (FT_ERR_OUT_OF_RANGE);
		for (index = 0U; index < snapshot.resources.unit_count; ++index)
			if (!write_unit(writer, snapshot.resources.units[index]))
				return (FT_ERR_OUT_OF_RANGE);
		if (snapshot.allowances.count > FT_CARD_GAME_MAX_ALLOWANCES
			|| (snapshot.allowances.count != 0U
				&& snapshot.allowances.allowances == ft_nullptr)
			|| !writer.u32(snapshot.allowances.count)
			|| !writer.u32(snapshot.allowances.next_id))
			return (FT_ERR_INVALID_STATE);
		for (index = 0U; index < snapshot.allowances.count; ++index)
			if (!write_allowance(writer, snapshot.allowances.allowances[index]))
				return (FT_ERR_OUT_OF_RANGE);
		if (snapshot.choices.count > FT_CARD_GAME_MAX_CHOICES
			|| (snapshot.choices.count != 0U
				&& snapshot.choices.choices == ft_nullptr)
			|| !writer.u32(snapshot.choices.count)
			|| !writer.u32(snapshot.choices.next_id))
			return (FT_ERR_INVALID_STATE);
		for (index = 0U; index < snapshot.choices.count; ++index)
			if (!write_choice(writer, snapshot.choices.choices[index]))
				return (FT_ERR_OUT_OF_RANGE);
		if (snapshot.usage_limits.count > FT_CARD_GAME_MAX_USAGE_LIMITS
			|| snapshot.usage_limits.capacity < snapshot.usage_limits.count
			|| (snapshot.usage_limits.count != 0U
				&& snapshot.usage_limits.limits == ft_nullptr)
			|| !writer.u32(snapshot.usage_limits.count)
			|| !writer.u32(snapshot.usage_limits.next_id)
			|| !writer.u32(snapshot.usage_limits.capacity))
			return (FT_ERR_INVALID_STATE);
		for (index = 0U; index < snapshot.usage_limits.count; ++index)
			if (!write_usage_limit(writer, snapshot.usage_limits.limits[index]))
				return (FT_ERR_OUT_OF_RANGE);
	}
	catch (const std::bad_alloc &)
	{
		return (FT_ERR_NO_MEMORY);
	}
    const std::size_t payload_size = wire_size - CARD_GAME_SNAPSHOT_CHECKSUM_BYTES;
    if (writer.offset() != payload_size)
        return (FT_ERR_INVALID_STATE);
    const uint64_t checksum = card_game_snapshot_checksum(staged.data(), payload_size);
    if (!writer.u64(checksum))
        return (FT_ERR_INVALID_STATE);
    bytes_out.swap(staged);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::restore_snapshot(const uint8_t *bytes,
    std::size_t byte_count) noexcept
{
    card_game_snapshot snapshot;
    uint32_t version;
    uint32_t host_count;
    uint32_t player;
    uint32_t zone;
    uint32_t index;
    uint32_t flag;
    uint8_t signature[4];
    uint64_t rules_hash;
    uint32_t host_index;
    int32_t result;

    if (bytes == ft_nullptr || byte_count < CARD_GAME_SNAPSHOT_HEADER_BYTES
        + CARD_GAME_SNAPSHOT_CHECKSUM_BYTES
        || byte_count > FT_CARD_GAME_MAX_SNAPSHOT_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    const std::size_t payload_size = byte_count - CARD_GAME_SNAPSHOT_CHECKSUM_BYTES;
    if (buffer_load_le<uint64_t>(bytes + payload_size)
        != card_game_snapshot_checksum(bytes, payload_size))
        return (FT_ERR_INVALID_ARGUMENT);
    card_game_snapshot_reader reader(bytes, payload_size);
    for (index = 0U; index < 4U; ++index)
        if (!reader.u8(&signature[index]))
            return (FT_ERR_INVALID_ARGUMENT);
    if (signature[0] != 'G' || signature[1] != 'C' || signature[2] != 'G'
        || signature[3] != 'S' || !reader.u32(&version)
        || (version != CARD_GAME_SNAPSHOT_SERIAL_VERSION
            && version != CARD_GAME_SNAPSHOT_SERIAL_VERSION_LEGACY)
        || !reader.u64(&rules_hash))
        return (FT_ERR_INVALID_ARGUMENT);
    uint64_t current_rules_hash;
    if (this->get_rules_hash(&current_rules_hash) != FT_ERR_SUCCESS
        || current_rules_hash != rules_hash)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!reader.u32(&snapshot.format_version)
        || !reader.u64(&snapshot.state_sequence)
        || !reader.u32(&snapshot.player_count)
        || snapshot.player_count > FT_CARD_GAME_MAX_PLAYERS
        || !reader.u32(&snapshot.turn_number)
        || !reader.u32(&snapshot.active_player)
        || !reader.u32(&snapshot.current_phase_id)
        || !reader.u32(&snapshot.event_count)
        || snapshot.event_count > FT_CARD_GAME_MAX_EVENTS
        || !reader.u64(&snapshot.event_sequence)
        || !reader.u64(&snapshot.random_state)
        || !reader.u32(&snapshot.next_deck_instance_id)
        || !reader.u32(&snapshot.next_modifier_id)
        || !reader.u32(&snapshot.modifier_count)
        || snapshot.modifier_count > FT_CARD_GAME_MAX_MODIFIERS
        || !reader.u64(&snapshot.next_host_intent_sequence)
        || !reader.u32(&snapshot.pending_discard_player_id)
        || !reader.u32(&snapshot.pending_discard_count)
        || !reader.u32(&snapshot.pending_discard_zone_id))
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.modifier_count * 40ULL + snapshot.event_count * 20ULL > reader.remaining())
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot.event_capacity = snapshot.event_count;
    if (!allocate_array(snapshot.events, snapshot.event_count))
        return (FT_ERR_NO_MEMORY);
    for (index = 0U; index < snapshot.modifier_count; ++index)
        if (!read_modifier(reader, &snapshot.modifiers[index]))
            return (FT_ERR_INVALID_ARGUMENT);
    for (index = 0U; index < snapshot.event_count; ++index)
        if (!reader.u64(&snapshot.events[index].sequence)
            || !reader.u32(&snapshot.events[index].event_type)
            || !reader.u32(&snapshot.events[index].source_instance)
            || !reader.u32(&snapshot.events[index].target_instance))
            return (FT_ERR_INVALID_ARGUMENT);
    uint32_t host_minimum_bytes = 28U;
    if (version == CARD_GAME_SNAPSHOT_SERIAL_VERSION_LEGACY)
        host_minimum_bytes = 20U;
    if (!reader.u32(&host_count) || host_count > FT_CARD_GAME_MAX_OPERATIONS
        || host_count > reader.remaining() / host_minimum_bytes)
        return (FT_ERR_INVALID_ARGUMENT);
    result = snapshot.host_intents.initialize_storage(host_count);
    if (result != FT_ERR_SUCCESS)
        return (result);
    host_index = 0U;
    while (host_index < host_count)
    {
        card_game_host_intent_record *record = ::new (
            snapshot.host_intents._records + host_index) card_game_host_intent_record{};
        if (!read_host_intent(reader, record, version))
            return (FT_ERR_INVALID_ARGUMENT);
        ++host_index;
    }
    for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
    {
        card_game_player_snapshot &value = snapshot.players[player];

        if (!reader.u32(&value.board_count)
            || value.board_count > FT_CARD_GAME_MAX_CARDS
            || !reader.u32(&value.deck_count)
            || value.deck_count > FT_CARD_GAME_MAX_CARDS
            || !reader.u32(&value.hand_count)
            || value.hand_count > FT_CARD_GAME_MAX_CARDS
            || !reader.u32(&value.health) || !reader.u32(&value.mana))
            return (FT_ERR_INVALID_ARGUMENT);
        for (index = 0U; index < value.board_count; ++index)
            if (!reader.u32(&value.board[index])
                || !read_instance(reader, &value.instances[index]))
                return (FT_ERR_INVALID_ARGUMENT);
        for (index = 0U; index < value.deck_count; ++index)
            if (!reader.u32(&value.deck[index])
                || !reader.u32(&value.deck_instance_ids[index]))
                return (FT_ERR_INVALID_ARGUMENT);
        for (index = 0U; index < value.hand_count; ++index)
            if (!reader.u32(&value.hand[index])
                || !reader.u32(&value.hand_instance_ids[index]))
                return (FT_ERR_INVALID_ARGUMENT);
    }
    if (!reader.u32(&snapshot.zones.definition_count)
        || snapshot.zones.definition_count > FT_CARD_GAME_MAX_ZONES)
        return (FT_ERR_INVALID_ARGUMENT);
    for (index = 0U; index < snapshot.zones.definition_count; ++index)
    {
        card_game_zone_store_definition &definition =
            snapshot.zones.definitions[index];

        if (!reader.u32(&definition.zone_id)
            || !reader.u32(&definition.capacity)
            || !reader.u32(&definition.allowed_card_type_mask)
            || !reader.u32(&flag) || flag > 1U)
            return (FT_ERR_INVALID_ARGUMENT);
        definition.owner_scoped = static_cast<ft_bool>(flag);
    }
    for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
        for (zone = 0U; zone < FT_CARD_GAME_MAX_ZONES; ++zone)
            if (!reader.u32(&snapshot.zones.counts[player][zone]))
                return (FT_ERR_INVALID_ARGUMENT);
    for (player = 0U; player < FT_CARD_GAME_MAX_PLAYERS; ++player)
        for (zone = 0U; zone < FT_CARD_GAME_MAX_ZONES; ++zone)
            if (!reader.u32(&snapshot.zones.offsets[player][zone]))
                return (FT_ERR_INVALID_ARGUMENT);
    if (!reader.u32(&snapshot.zones.entry_count)
        || !reader.u32(&snapshot.zones.entry_capacity)
        || snapshot.zones.entry_count > snapshot.zones.entry_capacity
        || snapshot.zones.entry_capacity
            > FT_CARD_GAME_MAX_PLAYERS * FT_CARD_GAME_MAX_ZONES
                * FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot.zones.entry_count > reader.remaining() / 8U)
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot.zones.entry_capacity = snapshot.zones.entry_count;
    if (!allocate_array(snapshot.zones.entries, snapshot.zones.entry_count))
        return (FT_ERR_NO_MEMORY);
    for (index = 0U; index < snapshot.zones.entry_count; ++index)
        if (!reader.u32(&snapshot.zones.entries[index].instance_id)
            || !reader.u32(&snapshot.zones.entries[index].card_id))
            return (FT_ERR_INVALID_ARGUMENT);
    if (!reader.u32(&snapshot.resources.pool_count)
        || snapshot.resources.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || !reader.u32(&snapshot.resources.unit_count)
        || snapshot.resources.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || !reader.u32(&snapshot.resources.next_unit_id)
        || snapshot.resources.pool_count * 28ULL + snapshot.resources.unit_count * 48ULL
            > reader.remaining())
        return (FT_ERR_INVALID_ARGUMENT);
    if (!allocate_array(snapshot.resources.pools, snapshot.resources.pool_count)
        || !allocate_array(snapshot.resources.units, snapshot.resources.unit_count))
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < snapshot.resources.pool_count)
    {
        if (!read_pool(reader, &snapshot.resources.pools[index]))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    for (index = 0U; index < snapshot.resources.unit_count; ++index)
        if (!read_unit(reader, &snapshot.resources.units[index]))
            return (FT_ERR_INVALID_ARGUMENT);
    if (!reader.u32(&snapshot.allowances.count)
        || snapshot.allowances.count > FT_CARD_GAME_MAX_ALLOWANCES
        || !reader.u32(&snapshot.allowances.next_id)
        || snapshot.allowances.count > reader.remaining() / 48U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!allocate_array(snapshot.allowances.allowances, snapshot.allowances.count))
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < snapshot.allowances.count)
    {
        if (!read_allowance(reader, &snapshot.allowances.allowances[index]))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    if (!reader.u32(&snapshot.choices.count)
        || snapshot.choices.count > FT_CARD_GAME_MAX_CHOICES
        || !reader.u32(&snapshot.choices.next_id)
        || snapshot.choices.count > reader.remaining() / 36U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!allocate_array(snapshot.choices.choices, snapshot.choices.count))
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < snapshot.choices.count)
    {
        if (!read_choice(reader, &snapshot.choices.choices[index]))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    if (!reader.u32(&snapshot.usage_limits.count)
        || snapshot.usage_limits.count > FT_CARD_GAME_MAX_USAGE_LIMITS
        || !reader.u32(&snapshot.usage_limits.next_id)
        || !reader.u32(&snapshot.usage_limits.capacity)
        || snapshot.usage_limits.capacity < snapshot.usage_limits.count
        || snapshot.usage_limits.capacity > FT_CARD_GAME_MAX_USAGE_LIMITS
        || snapshot.usage_limits.count > reader.remaining() / 40U)
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot.usage_limits.capacity = snapshot.usage_limits.count;
    if (!allocate_array(snapshot.usage_limits.limits, snapshot.usage_limits.count))
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < snapshot.usage_limits.count)
    {
        if (!read_usage_limit(reader, &snapshot.usage_limits.limits[index]))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    if (reader.remaining() != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->apply_snapshot(snapshot));
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
    card_game_hash_u32(&calculated_hash,
        static_cast<uint32_t>(this->_draw_policy_configured));
    if (this->_draw_policy_configured == FT_TRUE)
    {
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.discard_zone_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.burned_zone_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_draw_policy.reshuffle_discard));
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.empty_draw_event_type_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.empty_draw_event_version);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.overflow_event_type_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.overflow_event_version);
    }
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
    card_game_hash_u32(&calculated_hash, this->_host_intent_type_count);
    index = 0U;
    while (index < this->_host_intent_type_count)
    {
        uint32_t field_index;

        card_game_hash_u32(&calculated_hash,
            this->_host_intent_types[index].type_id);
        card_game_hash_u32(&calculated_hash,
            this->_host_intent_types[index].version);
        card_game_hash_u32(&calculated_hash,
            this->_host_intent_types[index].field_count);
        field_index = 0U;
        while (field_index < this->_host_intent_types[index].field_count)
        {
            card_game_hash_u32(&calculated_hash,
                static_cast<uint32_t>(this->_host_intent_types[index]
                    .field_types[field_index]));
            field_index += 1U;
        }
        index += 1U;
    }
    *hash = calculated_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_script_environment_hash(
    uint64_t *hash) const noexcept
{
    card_game_host_intent_definition sorted_types[
        FT_CARD_GAME_MAX_HOST_INTENT_TYPES];
    uint64_t calculated_hash;
    uint32_t index;
    uint32_t sort_index;

    if (this->_initialised_state != 2U || hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_host_intent_type_count)
    {
        sorted_types[index] = this->_host_intent_types[index];
        index += 1U;
    }
    sort_index = 1U;
    while (sort_index < this->_host_intent_type_count)
    {
        card_game_host_intent_definition current = sorted_types[sort_index];
        uint32_t insertion_index = sort_index;

        while (insertion_index > 0U
            && sorted_types[insertion_index - 1U].type_id > current.type_id)
        {
            sorted_types[insertion_index] = sorted_types[insertion_index - 1U];
            insertion_index -= 1U;
        }
        sorted_types[insertion_index] = current;
        sort_index += 1U;
    }
    calculated_hash = 14695981039346656037ULL;
    card_game_hash_u32(&calculated_hash, 1U);
    card_game_hash_u32(&calculated_hash,
        static_cast<uint32_t>(this->_draw_policy_configured));
    if (this->_draw_policy_configured == FT_TRUE)
    {
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.discard_zone_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.burned_zone_id);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_draw_policy.reshuffle_discard));
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.empty_draw_event_type_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.empty_draw_event_version);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.overflow_event_type_id);
        card_game_hash_u32(&calculated_hash,
            this->_draw_policy.overflow_event_version);
    }
    card_game_hash_u32(&calculated_hash, this->_host_intent_type_count);
    index = 0U;
    while (index < this->_host_intent_type_count)
    {
        card_game_hash_u32(&calculated_hash, sorted_types[index].type_id);
        card_game_hash_u32(&calculated_hash, sorted_types[index].version);
        card_game_hash_u32(&calculated_hash,
            sorted_types[index].field_count);
        uint32_t field_index = 0U;
        while (field_index < sorted_types[index].field_count)
        {
            card_game_hash_u32(&calculated_hash,
                static_cast<uint32_t>(sorted_types[index].field_types[
                    field_index]));
            field_index += 1U;
        }
        index += 1U;
    }
    *hash = calculated_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_zone_store::append_state_hash(uint64_t &hash) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    card_game_hash_u32(&hash, this->_definition_count);
    uint32_t zone_index = 0U;
    while (zone_index < this->_definition_count)
    {
        const card_game_zone_store_definition &definition = this->_definitions[zone_index];
        card_game_hash_u32(&hash, definition.zone_id);
        card_game_hash_u32(&hash, definition.capacity);
        card_game_hash_u32(&hash, definition.allowed_card_type_mask);
        card_game_hash_u32(&hash, static_cast<uint32_t>(definition.owner_scoped));
        ++zone_index;
    }
    uint32_t entry_count = 0U;
    uint32_t player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < this->_definition_count)
        {
            entry_count += this->_zones[player_id][zone_index].size();
            ++zone_index;
        }
        ++player_id;
    }
    card_game_hash_u32(&hash, entry_count);
    uint32_t offset = 0U;
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < FT_CARD_GAME_MAX_ZONES)
        {
            uint32_t count = 0U;
            if (zone_index < this->_definition_count)
                count = this->_zones[player_id][zone_index].size();
            card_game_hash_u32(&hash, count);
            card_game_hash_u32(&hash, offset);
            offset += count;
            ++zone_index;
        }
        ++player_id;
    }
    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_index = 0U;
        while (zone_index < this->_definition_count)
        {
            const card_game_ordered_zone &zone = this->_zones[player_id][zone_index];
            const uint32_t count = zone.size();
            uint32_t index = 0U;
            while (index < count)
            {
                const card_game_zone_entry &entry = zone._cards[zone.storage_index(index)];
                card_game_hash_u32(&hash, entry.instance_id);
                card_game_hash_u32(&hash, entry.card_id);
                ++index;
            }
            ++zone_index;
        }
        ++player_id;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::append_state_hash(uint64_t &hash) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    uint32_t index;
    card_game_hash_u32(&hash, this->_pool_count);
    card_game_hash_u32(&hash, this->_unit_count);
    card_game_hash_u32(&hash, this->_next_unit_id);
    index = 0U;
    while (index < this->_pool_count)
    {
        card_game_hash_u32(&hash,
            this->_pools[index].pool_id);
        card_game_hash_u32(&hash,
            this->_pools[index].owner_id);
        card_game_hash_u32(&hash,
            this->_pools[index].resource_type_id);
        card_game_hash_u32(&hash,
            this->_pools[index].maximum_amount);
        card_game_hash_u32(&hash,
            this->_pools[index].current_amount);
        card_game_hash_u32(&hash,
            this->_pools[index].locked_amount);
        card_game_hash_u32(&hash,
            this->_pools[index].temporary_amount);
        index += 1U;
    }
    index = 0U;
    while (index < this->_unit_count)
    {
        card_game_hash_u32(&hash,
            this->_units[index].unit_id);
        card_game_hash_u32(&hash,
            this->_units[index].owner_id);
        card_game_hash_u32(&hash,
            this->_units[index].resource_type_id);
        card_game_hash_u32(&hash,
            this->_units[index].amount);
        card_game_hash_u32(&hash,
            this->_units[index].tags);
        card_game_hash_u64(&hash,
            this->_units[index].expiry_epoch);
        card_game_hash_u64(&hash,
            this->_units[index].unlock_epoch);
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_units[index].temporary));
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_units[index].locked));
        card_game_hash_u32(&hash,
            this->_units[index].locked_amount);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::append_state_hash(uint64_t &hash) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    uint32_t index;
    card_game_hash_u32(&hash, this->_count);
    card_game_hash_u32(&hash, this->_next_id);
    index = 0U;
    while (index < this->_count)
    {
        card_game_hash_u32(&hash,
            this->_allowances[index].allowance_id);
        card_game_hash_u32(&hash,
            this->_allowances[index].owner_id);
        card_game_hash_u32(&hash,
            this->_allowances[index].action_id);
        card_game_hash_u32(&hash,
            this->_allowances[index].action_tags);
        card_game_hash_u32(&hash,
            this->_allowances[index].remaining_uses);
        card_game_hash_u32(&hash,
            this->_allowances[index].maximum_uses);
        card_game_hash_u64(&hash,
            this->_allowances[index].expiry_epoch);
        card_game_hash_u32(&hash,
            this->_allowances[index].source_instance);
        card_game_hash_u32(&hash,
            this->_allowances[index].source_effect_id);
        card_game_hash_u32(&hash,
            this->_allowances[index].predicate_id);
        card_game_hash_u32(&hash,
            this->_allowances[index].predicate_context_id);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_choice_ledger::append_state_hash(uint64_t &hash) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    uint32_t index;
    card_game_hash_u32(&hash, this->_count);
    card_game_hash_u32(&hash, this->_next_id);
    index = 0U;
    while (index < this->_count)
    {
        card_game_hash_u32(&hash,
            this->_choices[index].choice_id);
        card_game_hash_u32(&hash,
            this->_choices[index].player_id);
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_choices[index].kind));
        card_game_hash_u32(&hash,
            this->_choices[index].option_count);
        card_game_hash_u64(&hash,
            this->_choices[index].deadline_epoch);
        card_game_hash_u32(&hash,
            this->_choices[index].default_option_id);
        card_game_hash_u32(&hash,
            this->_choices[index].selected_option_id);
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_choices[index].resolved));
        uint32_t option_index = 0U;
        while (option_index < this->_choices[index].option_count)
        {
            card_game_hash_u32(&hash,
                this->_choices[index].options[option_index].option_id);
            card_game_hash_u32(&hash,
                this->_choices[index].options[option_index].value_a);
            card_game_hash_u32(&hash,
                this->_choices[index].options[option_index].value_b);
            option_index += 1U;
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_usage_limit_ledger::append_state_hash(uint64_t &hash) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    uint32_t index;
    card_game_hash_u32(&hash, this->_count);
    card_game_hash_u32(&hash, this->_next_id);
    index = 0U;
    while (index < this->_count)
    {
        card_game_hash_u32(&hash,
            this->_limits[index].limit_id);
        card_game_hash_u32(&hash,
            this->_limits[index].key_id);
        card_game_hash_u32(&hash,
            this->_limits[index].subject_id);
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_limits[index].scope));
        card_game_hash_u64(&hash,
            this->_limits[index].window_epoch);
        card_game_hash_u32(&hash,
            this->_limits[index].maximum_uses);
        card_game_hash_u32(&hash,
            this->_limits[index].used_uses);
        card_game_hash_u32(&hash,
            static_cast<uint32_t>(this->_limits[index]
                .attempt_policy));
        card_game_hash_u32(&hash,
            this->_limits[index].source_instance);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_state_hash(uint64_t *hash) const noexcept
{
    uint64_t calculated_hash;
    uint32_t player_id;
    uint32_t event_index;
    uint32_t board_index;
    uint32_t deck_index;
    uint32_t hand_index;
    uint32_t index;
    uint32_t host_intent_index;
    uint32_t field_index;

    if (this->_initialised_state != 2U || hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    calculated_hash = 1469598103934665603ULL;
    card_game_hash_u32(&calculated_hash, FT_CARD_GAME_STATE_FORMAT_VERSION);
    card_game_hash_u64(&calculated_hash, this->_state_sequence);
    card_game_hash_u32(&calculated_hash, this->_player_count);
    card_game_hash_u32(&calculated_hash, this->_turn_number);
    card_game_hash_u32(&calculated_hash, this->_active_player);
    card_game_hash_u32(&calculated_hash, this->_current_phase_id);
    card_game_hash_u32(&calculated_hash, this->_event_count);
    card_game_hash_u64(&calculated_hash, this->_event_sequence);
    card_game_hash_u64(&calculated_hash, this->_random_state);
    card_game_hash_u64(&calculated_hash,
        this->_next_host_intent_sequence);
    card_game_hash_u32(&calculated_hash,
        this->_pending_discard_player_id);
    card_game_hash_u32(&calculated_hash, this->_pending_discard_count);
    card_game_hash_u32(&calculated_hash, this->_pending_discard_zone_id);
    card_game_hash_u32(&calculated_hash, this->_host_intent_record_count);
    host_intent_index = 0U;
    while (host_intent_index < this->_host_intent_record_count)
    {
        const card_game_host_intent_record &host_intent_record =
            this->_host_intent_records[this->host_intent_record_index(host_intent_index)];
        card_game_hash_u64(&calculated_hash, host_intent_record.sequence);
        card_game_hash_u64(&calculated_hash,
            host_intent_record.action_request_id);
        card_game_hash_u32(&calculated_hash,
            host_intent_record.intent.type_id);
        card_game_hash_u32(&calculated_hash,
            host_intent_record.intent.version);
        card_game_hash_u32(&calculated_hash,
            host_intent_record.intent.field_count);
        field_index = 0U;
        while (field_index < host_intent_record.intent.field_count
            && field_index < FT_CARD_GAME_MAX_HOST_INTENT_FIELDS)
        {
            card_game_hash_u32(&calculated_hash,
                static_cast<uint32_t>(host_intent_record.intent.fields[
                    field_index].type));
            card_game_hash_u64(&calculated_hash,
                host_intent_record.intent.fields[field_index].value);
            field_index += 1U;
        }
        host_intent_index += 1U;
    }
    card_game_hash_u32(&calculated_hash, this->_next_deck_instance_id);
    card_game_hash_u32(&calculated_hash, this->_next_modifier_id);
    card_game_hash_u32(&calculated_hash, this->_modifier_count);
    index = 0U;
    while (index < this->_modifier_count)
    {
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].modifier_id);
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].source_effect_id);
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].target_player_id);
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].target_instance_id);
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].target_instance_index);
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_modifiers[index].attack_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_modifiers[index].health_delta));
        card_game_hash_u32(&calculated_hash,
            static_cast<uint32_t>(this->_modifiers[index].duration));
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].created_turn);
        card_game_hash_u32(&calculated_hash,
            this->_modifiers[index].created_phase_id);
        index += 1U;
    }
    event_index = 0U;
    while (event_index < this->_event_count)
    {
        card_game_hash_u64(&calculated_hash,
            this->_events[event_index].sequence);
        card_game_hash_u32(&calculated_hash,
            this->_events[event_index].event_type);
        card_game_hash_u32(&calculated_hash,
            this->_events[event_index].source_instance);
        card_game_hash_u32(&calculated_hash,
            this->_events[event_index].target_instance);
        event_index += 1U;
    }
    player_id = 0U;
    while (player_id < this->_player_count)
    {
        const uint32_t deck_count = this->_decks[player_id].size();
        card_game_hash_u32(&calculated_hash,
            this->_board_count[player_id]);
        card_game_hash_u32(&calculated_hash, deck_count);
        card_game_hash_u32(&calculated_hash,
            this->_hand_count[player_id]);
        card_game_hash_u32(&calculated_hash,
            this->_health[player_id]);
        card_game_hash_u32(&calculated_hash,
            this->_mana[player_id]);
        board_index = 0U;
        while (board_index < this->_board_count[player_id])
        {
            card_game_hash_u32(&calculated_hash,
                this->_board[player_id][board_index]);
            card_game_hash_instance(&calculated_hash,
                this->_instances[player_id][board_index]);
            board_index += 1U;
        }
        deck_index = 0U;
        while (deck_index < deck_count)
        {
            card_game_zone_entry entry;
            if (this->_decks[player_id].get_entry(deck_index, &entry) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            card_game_hash_u32(&calculated_hash, entry.card_id);
            card_game_hash_u32(&calculated_hash, entry.instance_id);
            ++deck_index;
        }
        hand_index = 0U;
        while (hand_index < this->_hand_count[player_id])
        {
            card_game_hash_u32(&calculated_hash,
                this->_hand[player_id][hand_index].card_id);
            card_game_hash_u32(&calculated_hash,
                this->_hand[player_id][hand_index].instance_id);
            hand_index += 1U;
        }
        player_id += 1U;
    }
    if (this->_zone_store.append_state_hash(calculated_hash) != FT_ERR_SUCCESS
        || this->_resources.append_state_hash(calculated_hash) != FT_ERR_SUCCESS
        || this->_allowances.append_state_hash(calculated_hash) != FT_ERR_SUCCESS
        || this->_choices.append_state_hash(calculated_hash) != FT_ERR_SUCCESS
        || this->_usage_limits.append_state_hash(calculated_hash) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    *hash = calculated_hash;
    return (FT_ERR_SUCCESS);
}

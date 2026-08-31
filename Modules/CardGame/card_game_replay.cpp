#include "card_game_replay.hpp"
#include "../CMA/CMA.hpp"
#include "../File/file_utils.hpp"

static const uint32_t CARD_GAME_REPLAY_MAGIC = 0x43524732U;
static const uint32_t CARD_GAME_REPLAY_VERSION = 1U;
static const uint32_t CARD_GAME_REPLAY_HEADER_BYTES = 84U;
static const uint32_t CARD_GAME_REPLAY_EVENT_BYTES = 120U;

static void replay_write_u32(uint8_t *output, uint32_t *offset,
    uint32_t value) noexcept
{
    output[*offset] = static_cast<uint8_t>(value & 255U);
    output[*offset + 1U] = static_cast<uint8_t>((value >> 8U) & 255U);
    output[*offset + 2U] = static_cast<uint8_t>((value >> 16U) & 255U);
    output[*offset + 3U] = static_cast<uint8_t>((value >> 24U) & 255U);
    *offset += 4U;
}

static void replay_write_u64(uint8_t *output, uint32_t *offset,
    uint64_t value) noexcept
{
    replay_write_u32(output, offset, static_cast<uint32_t>(value));
    replay_write_u32(output, offset, static_cast<uint32_t>(value >> 32U));
}

static int32_t replay_read_u32(const uint8_t *input, uint32_t input_size,
    uint32_t *offset, uint32_t *value) noexcept
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

static int32_t replay_read_u64(const uint8_t *input, uint32_t input_size,
    uint32_t *offset, uint64_t *value) noexcept
{
    uint32_t low;
    uint32_t high;
    int32_t result;

    result = replay_read_u32(input, input_size, offset, &low);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size, offset, &high);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *value = static_cast<uint64_t>(low)
        | (static_cast<uint64_t>(high) << 32U);
    return (FT_ERR_SUCCESS);
}

static uint32_t replay_checksum(const uint8_t *input,
    uint32_t input_size) noexcept
{
    uint32_t hash;
    uint32_t index;

    hash = 2166136261U;
    index = 0U;
    while (index < input_size)
    {
        hash ^= input[index];
        hash *= 16777619U;
        index += 1U;
    }
    return (hash);
}

card_game_replay::card_game_replay() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _header(),
      _event_count(0U), _event_capacity(0U), _events(ft_nullptr),
      _has_result(FT_FALSE), _result()
{
    return ;
}

card_game_replay::~card_game_replay() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_replay::initialize(
    const card_game_replay_header &header) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (header.visibility_mode != CARD_GAME_REPLAY_FULL_INFORMATION
        && header.visibility_mode != CARD_GAME_REPLAY_PLAYER_VIEW)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_header = header;
    this->_event_count = 0U;
    this->_event_capacity = FT_CARD_GAME_REPLAY_INITIAL_CAPACITY;
    this->_has_result = FT_FALSE;
    ft_bzero(&this->_result, sizeof(this->_result));
    this->_events = static_cast<card_game_replay_event *>(cma_malloc(
        static_cast<ft_size_t>(this->_event_capacity)
            * sizeof(card_game_replay_event)));
    if (this->_events == ft_nullptr)
    {
        this->_event_capacity = 0U;
        return (FT_ERR_NO_MEMORY);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::destroy() noexcept
{
    if (this->_events != ft_nullptr)
        cma_free(this->_events);
    this->_events = ft_nullptr;
    this->_event_count = 0U;
    this->_event_capacity = 0U;
    this->_has_result = FT_FALSE;
    ft_bzero(&this->_result, sizeof(this->_result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::validate_event(
    const card_game_replay_event &event) const noexcept
{
    if (event.sequence == 0U
        || event.command_type < CARD_GAME_INTENT_PLAY_CARD
        || event.command_type > CARD_GAME_INTENT_ADVANCE_PHASE
        || event.private_size > FT_CARD_GAME_REPLAY_PRIVATE_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_event_count != 0U
        && event.sequence <= this->_events[this->_event_count - 1U].sequence)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_header.visibility_mode == CARD_GAME_REPLAY_PLAYER_VIEW
        && event.private_size != 0U
        && event.private_owner_id != this->_header.viewer_player_id)
        return (FT_ERR_PERMISSION_DENIED);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::append(const card_game_replay_event &event) noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->validate_event(event);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_event_count >= FT_CARD_GAME_MAX_REPLAY_EVENTS)
        return (FT_ERR_FULL);
    if (this->_event_count == this->_event_capacity)
    {
        result = this->grow_events();
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    this->_events[this->_event_count] = event;
    if (event.private_size < FT_CARD_GAME_REPLAY_PRIVATE_BYTES)
        ft_bzero(this->_events[this->_event_count].private_data
            + event.private_size,
            FT_CARD_GAME_REPLAY_PRIVATE_BYTES - event.private_size);
    this->_event_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::append_command_record(
    const card_game_command_record &record) noexcept
{
    card_game_replay_event event;

    if (record.rules_hash != this->_header.rules_hash)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bzero(&event, sizeof(event));
    event.sequence = record.command.command_sequence;
    event.expected_state_sequence = record.command.expected_state_sequence;
    event.player_id = record.command.player_id;
    event.command_type = record.command.type;
    event.card_id = record.command.card_id;
    event.target_instance = record.command.target_instance;
    event.state_hash_before = record.state_hash_before;
    event.state_hash_after = record.state_hash_after;
    return (this->append(event));
}

int32_t card_game_replay::set_result(
    const card_game_replay_result &result) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (result.outcome == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_result = result;
    this->_has_result = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::get_result(
    card_game_replay_result *result) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_has_result == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    *result = this->_result;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::grow_events() noexcept
{
    card_game_replay_event *events;
    uint32_t capacity;

    if (this->_event_capacity >= FT_CARD_GAME_MAX_REPLAY_EVENTS)
        return (FT_ERR_FULL);
    capacity = this->_event_capacity * 2U;
    if (capacity < this->_event_capacity
        || capacity > FT_CARD_GAME_MAX_REPLAY_EVENTS)
        capacity = FT_CARD_GAME_MAX_REPLAY_EVENTS;
    events = static_cast<card_game_replay_event *>(cma_malloc(
        static_cast<ft_size_t>(capacity) * sizeof(card_game_replay_event)));
    if (events == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_event_count != 0U)
        ft_memcpy(events, this->_events,
            static_cast<ft_size_t>(this->_event_count)
                * sizeof(card_game_replay_event));
    cma_free(this->_events);
    this->_events = events;
    this->_event_capacity = capacity;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::get_header(
    card_game_replay_header *header) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || header == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *header = this->_header;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::get_event(uint32_t index,
    card_game_replay_event *event) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || event == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (index >= this->_event_count)
        return (FT_ERR_OUT_OF_RANGE);
    *event = this->_events[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::project_player_view(uint32_t viewer_player_id,
    card_game_replay &output) const noexcept
{
    card_game_replay candidate;
    card_game_replay_header header;
    card_game_replay_event event;
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || &output == this || viewer_player_id == 0U
        || this->_header.visibility_mode != CARD_GAME_REPLAY_FULL_INFORMATION)
        return (FT_ERR_INVALID_ARGUMENT);
    header = this->_header;
    header.visibility_mode = CARD_GAME_REPLAY_PLAYER_VIEW;
    header.viewer_player_id = viewer_player_id;
    header.source_replay_hash = this->_header.source_replay_hash;
    result = candidate.initialize(header);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_has_result != FT_FALSE)
    {
        result = candidate.set_result(this->_result);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    index = 0U;
    while (index < this->_event_count)
    {
        event = this->_events[index];
        if (event.private_size != 0U
            && event.private_owner_id != viewer_player_id)
        {
            event.private_owner_id = 0U;
            event.private_size = 0U;
            ft_bzero(event.private_data, sizeof(event.private_data));
        }
        result = candidate.append(event);
        if (result != FT_ERR_SUCCESS)
            return (result);
        index += 1U;
    }
    result = output.destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    output._header = candidate._header;
    output._event_count = candidate._event_count;
    output._event_capacity = candidate._event_capacity;
    output._events = candidate._events;
    output._has_result = candidate._has_result;
    output._result = candidate._result;
    candidate._events = ft_nullptr;
    candidate._event_capacity = 0U;
    output._initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::serialize(uint8_t *output,
    uint32_t output_capacity, uint32_t *output_size) const noexcept
{
    uint32_t offset;
    uint32_t index;
    uint32_t checksum;
    const card_game_replay_event *event;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || output == ft_nullptr || output_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_event_count > FT_CARD_GAME_MAX_REPLAY_EVENTS
        || output_capacity < CARD_GAME_REPLAY_HEADER_BYTES
            + this->_event_count * CARD_GAME_REPLAY_EVENT_BYTES + 4U)
        return (FT_ERR_FULL);
    offset = 0U;
    replay_write_u32(output, &offset, CARD_GAME_REPLAY_MAGIC);
    replay_write_u32(output, &offset, CARD_GAME_REPLAY_VERSION);
    replay_write_u32(output, &offset, this->_header.profile_id);
    replay_write_u32(output, &offset, this->_header.format_id);
    replay_write_u32(output, &offset, this->_header.corpus_version);
    replay_write_u64(output, &offset, this->_header.rules_hash);
    replay_write_u64(output, &offset, this->_header.corpus_hash);
    replay_write_u64(output, &offset, this->_header.source_replay_hash);
    replay_write_u32(output, &offset,
        static_cast<uint32_t>(this->_header.visibility_mode));
    replay_write_u32(output, &offset, this->_header.viewer_player_id);
    replay_write_u32(output, &offset, this->_event_count);
    replay_write_u32(output, &offset, static_cast<uint32_t>(this->_has_result));
    replay_write_u32(output, &offset, this->_result.outcome);
    replay_write_u32(output, &offset, this->_result.winner_player_id);
    replay_write_u64(output, &offset, this->_result.final_state_hash);
    replay_write_u64(output, &offset, this->_result.duration_epoch);
    index = 0U;
    while (index < this->_event_count)
    {
        event = &this->_events[index];
        replay_write_u64(output, &offset, event->sequence);
        replay_write_u64(output, &offset, event->expected_state_sequence);
        replay_write_u32(output, &offset, event->player_id);
        replay_write_u32(output, &offset,
            static_cast<uint32_t>(event->command_type));
        replay_write_u32(output, &offset, event->card_id);
        replay_write_u32(output, &offset, event->target_instance);
        replay_write_u64(output, &offset, event->state_hash_before);
        replay_write_u64(output, &offset, event->state_hash_after);
        replay_write_u32(output, &offset, event->private_owner_id);
        replay_write_u32(output, &offset, event->private_size);
        ft_memcpy(output + offset, event->private_data,
            FT_CARD_GAME_REPLAY_PRIVATE_BYTES);
        offset += FT_CARD_GAME_REPLAY_PRIVATE_BYTES;
        index += 1U;
    }
    checksum = replay_checksum(output, offset);
    replay_write_u32(output, &offset, checksum);
    *output_size = offset;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::serialized_size(uint32_t *size) const noexcept
{
    uint32_t required_size;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_event_count > FT_CARD_GAME_MAX_REPLAY_EVENTS)
        return (FT_ERR_FULL);
    required_size = CARD_GAME_REPLAY_HEADER_BYTES + 4U;
    if (this->_event_count > (UINT32_MAX - required_size)
        / CARD_GAME_REPLAY_EVENT_BYTES)
        return (FT_ERR_FULL);
    required_size += this->_event_count * CARD_GAME_REPLAY_EVENT_BYTES;
    *size = required_size;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_replay::save_file(const char *path) const noexcept
{
    uint8_t *serialized_data;
    uint32_t serialized_size_value;
    int32_t result;

    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->serialized_size(&serialized_size_value);
    if (result != FT_ERR_SUCCESS)
        return (result);
    serialized_data = static_cast<uint8_t *>(cma_malloc(
        static_cast<ft_size_t>(serialized_size_value)));
    if (serialized_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    result = this->serialize(serialized_data, serialized_size_value,
        &serialized_size_value);
    if (result == FT_ERR_SUCCESS)
        result = file_write_all_atomic(path,
            reinterpret_cast<const char *>(serialized_data),
            serialized_size_value);
    cma_free(serialized_data);
    return (result);
}

int32_t card_game_replay::load_file(const char *path) noexcept
{
    ft_string file_data;
    int32_t result;

    if (path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = file_data.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = file_read_all(path, file_data);
    if (result != FT_ERR_SUCCESS)
    {
        (void)file_data.destroy();
        return (result);
    }
    if (file_data.size() > UINT32_MAX)
    {
        (void)file_data.destroy();
        return (FT_ERR_FULL);
    }
    result = this->deserialize(
        reinterpret_cast<const uint8_t *>(file_data.data()),
        static_cast<uint32_t>(file_data.size()));
    if (file_data.destroy() != FT_ERR_SUCCESS && result == FT_ERR_SUCCESS)
        result = FT_ERR_INTERNAL;
    return (result);
}

int32_t card_game_replay::deserialize(const uint8_t *input,
    uint32_t input_size) noexcept
{
    card_game_replay candidate;
    card_game_replay_header header;
    card_game_replay_event event;
    uint32_t offset;
    uint32_t value;
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    uint32_t index;
    uint32_t event_count;
    uint32_t has_result;
    int32_t result;
    card_game_replay_result replay_result;

    ft_bzero(&replay_result, sizeof(replay_result));

    if (input == ft_nullptr || input_size < CARD_GAME_REPLAY_HEADER_BYTES + 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    actual_checksum = static_cast<uint32_t>(input[input_size - 4U])
        | (static_cast<uint32_t>(input[input_size - 3U]) << 8U)
        | (static_cast<uint32_t>(input[input_size - 2U]) << 16U)
        | (static_cast<uint32_t>(input[input_size - 1U]) << 24U);
    expected_checksum = replay_checksum(input, input_size - 4U);
    if (actual_checksum != expected_checksum)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 0U;
    result = replay_read_u32(input, input_size - 4U, &offset, &value);
    if (result != FT_ERR_SUCCESS || value != CARD_GAME_REPLAY_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    result = replay_read_u32(input, input_size - 4U, &offset, &value);
    if (result != FT_ERR_SUCCESS || value != CARD_GAME_REPLAY_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    result = replay_read_u32(input, input_size - 4U, &offset, &header.profile_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size - 4U, &offset, &header.format_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size - 4U, &offset,
        &header.corpus_version);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u64(input, input_size - 4U, &offset, &header.rules_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u64(input, input_size - 4U, &offset, &header.corpus_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u64(input, input_size - 4U, &offset,
        &header.source_replay_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size - 4U, &offset, &value);
    if (result != FT_ERR_SUCCESS || (value != CARD_GAME_REPLAY_FULL_INFORMATION
        && value != CARD_GAME_REPLAY_PLAYER_VIEW))
        return (FT_ERR_INVALID_ARGUMENT);
    header.visibility_mode = static_cast<card_game_replay_visibility_mode>(value);
    result = replay_read_u32(input, input_size - 4U, &offset,
        &header.viewer_player_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size - 4U, &offset, &event_count);
    if (result != FT_ERR_SUCCESS || event_count > FT_CARD_GAME_MAX_REPLAY_EVENTS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = replay_read_u32(input, input_size - 4U, &offset, &has_result);
    if (result != FT_ERR_SUCCESS || (has_result != FT_FALSE
        && has_result != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    result = replay_read_u32(input, input_size - 4U, &offset,
        &replay_result.outcome);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u32(input, input_size - 4U, &offset,
        &replay_result.winner_player_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u64(input, input_size - 4U, &offset,
        &replay_result.final_state_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = replay_read_u64(input, input_size - 4U, &offset,
        &replay_result.duration_epoch);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (has_result != FT_FALSE && replay_result.outcome == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (input_size != CARD_GAME_REPLAY_HEADER_BYTES
        + event_count * CARD_GAME_REPLAY_EVENT_BYTES + 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = candidate.initialize(header);
    if (result != FT_ERR_SUCCESS)
        return (result);
    candidate._has_result = static_cast<ft_bool>(has_result);
    candidate._result = replay_result;
    index = 0U;
    while (index < event_count)
    {
        ft_bzero(&event, sizeof(event));
        result = replay_read_u64(input, input_size - 4U, &offset,
            &event.sequence);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u64(input, input_size - 4U, &offset,
            &event.expected_state_sequence);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u32(input, input_size - 4U, &offset,
            &event.player_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u32(input, input_size - 4U, &offset, &value);
        if (result != FT_ERR_SUCCESS)
            return (result);
        event.command_type = static_cast<card_game_command_type>(value);
        result = replay_read_u32(input, input_size - 4U, &offset,
            &event.card_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u32(input, input_size - 4U, &offset,
            &event.target_instance);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u64(input, input_size - 4U, &offset,
            &event.state_hash_before);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u64(input, input_size - 4U, &offset,
            &event.state_hash_after);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u32(input, input_size - 4U, &offset,
            &event.private_owner_id);
        if (result != FT_ERR_SUCCESS)
            return (result);
        result = replay_read_u32(input, input_size - 4U, &offset,
            &event.private_size);
        if (result != FT_ERR_SUCCESS || input_size - 4U - offset
            < FT_CARD_GAME_REPLAY_PRIVATE_BYTES)
            return (FT_ERR_INVALID_ARGUMENT);
        ft_memcpy(event.private_data, input + offset,
            FT_CARD_GAME_REPLAY_PRIVATE_BYTES);
        offset += FT_CARD_GAME_REPLAY_PRIVATE_BYTES;
        result = candidate.append(event);
        if (result != FT_ERR_SUCCESS)
            return (result);
        index += 1U;
    }
    if (offset != input_size - 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_header = candidate._header;
    this->_event_count = candidate._event_count;
    this->_event_capacity = candidate._event_capacity;
    this->_events = candidate._events;
    this->_has_result = candidate._has_result;
    this->_result = candidate._result;
    candidate._events = ft_nullptr;
    candidate._event_capacity = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_replay::event_count() const noexcept
{
    return (this->_event_count);
}

int32_t card_game_replay::replay_into(card_game_engine &engine) const noexcept
{
    card_game_command_record *records;
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_header.visibility_mode != CARD_GAME_REPLAY_FULL_INFORMATION)
        return (FT_ERR_PERMISSION_DENIED);
    if (this->_event_count == 0U)
        return (FT_ERR_SUCCESS);
    records = static_cast<card_game_command_record *>(cma_malloc(
        static_cast<ft_size_t>(this->_event_count)
            * sizeof(card_game_command_record)));
    if (records == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    index = 0U;
    while (index < this->_event_count)
    {
        card_game_replay_event event;

        event = this->_events[index];
        ft_bzero(&records[index], sizeof(records[index]));
        records[index].command.command_sequence = event.sequence;
        records[index].command.expected_state_sequence =
            event.expected_state_sequence;
        records[index].command.player_id = event.player_id;
        records[index].command.type = event.command_type;
        records[index].command.card_id = event.card_id;
        records[index].command.target_instance = event.target_instance;
        records[index].rules_hash = this->_header.rules_hash;
        records[index].state_hash_before = event.state_hash_before;
        records[index].state_hash_after = event.state_hash_after;
        index += 1U;
    }
    result = engine.replay_command_records(records, this->_event_count,
        ft_nullptr);
    cma_free(records);
    return (result);
}

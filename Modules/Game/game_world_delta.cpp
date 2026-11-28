#include "game_world_delta.hpp"
#include "game_voxel_chunk.hpp"
#include "../Buffer/buffer_endian.hpp"
#include "../Errno/errno_internal.hpp"

static uint32_t game_world_delta_snapshot_checksum(const uint8_t *data,
    ft_size_t size) noexcept
{
    ft_size_t index;
    uint32_t checksum;

    checksum = 2166136261U;
    index = 0U;
    while (index < size)
    {
        checksum ^= static_cast<uint32_t>(data[index]);
        checksum *= 16777619U;
        index += 1U;
    }
    return (checksum);
}

static constexpr uint8_t GAME_BLOCK_REQUEST_FIELDS[] = {2, 8, 8, 8, 4, 4, 8, 4, 4, 1, 2, 1};
static constexpr uint8_t GAME_BLOCK_DELTA_FIELDS[] = {2, 8, 8, 8, 4, 4, 8, 8, 4, 1, 8, 1, 2, 1};

template <ft_size_t Count>
static constexpr ft_size_t game_world_delta_wire_size(const uint8_t (&widths)[Count]) noexcept
{
    ft_size_t size = 0U;
    ft_size_t index = 0U;
    while (index < Count)
        size += widths[index++];
    return (size);
}

template <typename Value>
static void game_world_delta_write_field(uint8_t *&cursor, Value value) noexcept
{
    buffer_store_le(cursor, value);
    cursor += sizeof(Value);
    return ;
}

template <typename Value>
static void game_world_delta_read_field(const uint8_t *&cursor, Value &value) noexcept
{
    value = buffer_load_le<Value>(cursor);
    cursor += sizeof(Value);
    return ;
}

template <typename Message>
static void game_world_delta_write_header(uint8_t *&cursor, const Message &message) noexcept
{
    game_world_delta_write_field(cursor, message.protocol_version);
    game_world_delta_write_field(cursor, message.session_id);
    game_world_delta_write_field(cursor, message.request_id);
    game_world_delta_write_field(cursor, message.world_id);
    game_world_delta_write_field(cursor, static_cast<uint32_t>(message.chunk_x));
    game_world_delta_write_field(cursor, static_cast<uint32_t>(message.chunk_z));
    return ;
}

template <typename Message>
static void game_world_delta_read_header(const uint8_t *&cursor, Message &message) noexcept
{
    uint32_t chunk_x;
    uint32_t chunk_z;
    game_world_delta_read_field(cursor, message.protocol_version);
    game_world_delta_read_field(cursor, message.session_id);
    game_world_delta_read_field(cursor, message.request_id);
    game_world_delta_read_field(cursor, message.world_id);
    game_world_delta_read_field(cursor, chunk_x);
    game_world_delta_read_field(cursor, chunk_z);
    message.chunk_x = static_cast<int32_t>(chunk_x);
    message.chunk_z = static_cast<int32_t>(chunk_z);
    return ;
}

template <typename Message>
static void game_world_delta_write_coordinates(uint8_t *&cursor, const Message &message) noexcept
{
    game_world_delta_write_field(cursor, message.local_x);
    game_world_delta_write_field(cursor, message.local_y);
    game_world_delta_write_field(cursor, message.local_z);
    return ;
}

template <typename Message>
static void game_world_delta_read_coordinates(const uint8_t *&cursor, Message &message) noexcept
{
    game_world_delta_read_field(cursor, message.local_x);
    game_world_delta_read_field(cursor, message.local_y);
    game_world_delta_read_field(cursor, message.local_z);
    return ;
}

static int32_t game_world_delta_append_fields(ft_byte_buffer &buffer, const uint8_t *wire,
    ft_size_t size, const uint8_t *widths, ft_size_t count) noexcept
{
    const int32_t status = buffer.append(wire, size);
    if (status != FT_ERR_FULL && status != FT_ERR_NO_MEMORY && status != FT_ERR_OUT_OF_RANGE)
        return (status);
    // Retain the prefix ending at the last complete field when space runs out.
    ft_size_t index = 0U;
    while (index < count)
    {
        const int32_t field_status = buffer.append(wire, widths[index]);
        if (field_status != FT_ERR_SUCCESS)
            return (field_status);
        wire += widths[index];
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_block_change_request_serialize(
    const game_block_change_request &request, ft_byte_buffer &buffer) noexcept
{
    uint8_t wire[game_world_delta_wire_size(GAME_BLOCK_REQUEST_FIELDS)];
    uint8_t *cursor = wire;
    game_world_delta_write_header(cursor, request);
    game_world_delta_write_field(cursor, request.expected_revision);
    game_world_delta_write_field(cursor, request.expected_block_id);
    game_world_delta_write_field(cursor, request.requested_block_id);
    game_world_delta_write_coordinates(cursor, request);
    return (game_world_delta_append_fields(buffer, wire, sizeof(wire),
        GAME_BLOCK_REQUEST_FIELDS, sizeof(GAME_BLOCK_REQUEST_FIELDS)));
}

int32_t game_block_change_request_deserialize(
    game_block_change_request &request, ft_byte_buffer &buffer) noexcept
{
    uint8_t wire[game_world_delta_wire_size(GAME_BLOCK_REQUEST_FIELDS)];
    const ft_size_t initial_read_position = buffer.read_position();
    const int32_t status = buffer.read(wire, sizeof(wire));
    if (status != FT_ERR_SUCCESS)
        return (status);
    game_block_change_request temporary_request;
    const uint8_t *cursor = wire;
    game_world_delta_read_header(cursor, temporary_request);
    game_world_delta_read_field(cursor, temporary_request.expected_revision);
    game_world_delta_read_field(cursor, temporary_request.expected_block_id);
    game_world_delta_read_field(cursor, temporary_request.requested_block_id);
    game_world_delta_read_coordinates(cursor, temporary_request);
    if (temporary_request.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || temporary_request.session_id == 0U
        || temporary_request.request_id == 0U
        || temporary_request.world_id == 0U
        || temporary_request.local_x >= GAME_VOXEL_CHUNK_WIDTH
        || temporary_request.local_y >= GAME_VOXEL_CHUNK_HEIGHT
        || temporary_request.local_z >= GAME_VOXEL_CHUNK_DEPTH)
    {
        const int32_t restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    request = temporary_request;
    return (FT_ERR_SUCCESS);
}

int32_t game_block_delta_serialize(const game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t wire[game_world_delta_wire_size(GAME_BLOCK_DELTA_FIELDS)];
    uint8_t *cursor = wire;
    game_world_delta_write_header(cursor, delta);
    game_world_delta_write_field(cursor, delta.previous_revision);
    game_world_delta_write_field(cursor, delta.revision);
    game_world_delta_write_field(cursor, delta.current_block_id);
    game_world_delta_write_field(cursor, delta.player_modified);
    game_world_delta_write_field(cursor, delta.server_tick);
    game_world_delta_write_coordinates(cursor, delta);
    return (game_world_delta_append_fields(buffer, wire, sizeof(wire),
        GAME_BLOCK_DELTA_FIELDS, sizeof(GAME_BLOCK_DELTA_FIELDS)));
}

int32_t game_block_delta_deserialize(game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept
{
    uint8_t wire[game_world_delta_wire_size(GAME_BLOCK_DELTA_FIELDS)];
    const ft_size_t initial_read_position = buffer.read_position();
    const int32_t status = buffer.read(wire, sizeof(wire));
    if (status != FT_ERR_SUCCESS)
        return (status);
    game_block_delta temporary_delta;
    const uint8_t *cursor = wire;
    game_world_delta_read_header(cursor, temporary_delta);
    game_world_delta_read_field(cursor, temporary_delta.previous_revision);
    game_world_delta_read_field(cursor, temporary_delta.revision);
    game_world_delta_read_field(cursor, temporary_delta.current_block_id);
    game_world_delta_read_field(cursor, temporary_delta.player_modified);
    game_world_delta_read_field(cursor, temporary_delta.server_tick);
    game_world_delta_read_coordinates(cursor, temporary_delta);
    if (temporary_delta.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || temporary_delta.session_id == 0U
        || temporary_delta.request_id == 0U
        || temporary_delta.world_id == 0U
        || temporary_delta.local_x >= GAME_VOXEL_CHUNK_WIDTH
        || temporary_delta.local_y >= GAME_VOXEL_CHUNK_HEIGHT
        || temporary_delta.local_z >= GAME_VOXEL_CHUNK_DEPTH
        || temporary_delta.player_modified > 1U
        || temporary_delta.revision == 0U
        || temporary_delta.previous_revision + 1U != temporary_delta.revision)
    {
        const int32_t restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    delta = temporary_delta;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_snapshot_serialize(const game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept
{
    errno_abort_if_uninitialised(chunk._initialised_state, "game_voxel_chunk::serialize");
    if (chunk._access_lock == ft_nullptr)
        return (chunk.set_error(FT_ERR_INVALID_STATE));
    ft_byte_buffer temporary_buffer;
    int32_t error_code = temporary_buffer.initialize(buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const ft_size_t frame_start = temporary_buffer.size();
    error_code = temporary_buffer.append_u32_le(0U);
    if (error_code == FT_ERR_SUCCESS)
    {
        const ft_size_t payload_start = temporary_buffer.size();
        error_code = chunk.serialize_locked(temporary_buffer);
        if (error_code == FT_ERR_SUCCESS)
        {
            const ft_size_t payload_size = temporary_buffer.size() - payload_start;
            if (payload_size > UINT32_MAX)
                error_code = FT_ERR_OUT_OF_RANGE;
            else
                error_code = temporary_buffer.overwrite_u32_le(frame_start,
                    static_cast<uint32_t>(payload_size));
            const uint8_t *payload_data;
            if (error_code == FT_ERR_SUCCESS)
                error_code = temporary_buffer.view(payload_start, payload_size, &payload_data);
            if (error_code == FT_ERR_SUCCESS)
                error_code = temporary_buffer.append_u32_le(
                    game_world_delta_snapshot_checksum(payload_data, payload_size));
        }
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.move(temporary_buffer);
    const int32_t temporary_destroy_error = temporary_buffer.destroy();
    if (error_code == FT_ERR_SUCCESS
        && temporary_destroy_error != FT_ERR_SUCCESS)
        error_code = temporary_destroy_error;
    return (error_code);
}

int32_t game_world_delta_snapshot_deserialize(game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept
{
    const uint8_t *payload_data;
    uint32_t payload_size;
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    ft_size_t initial_read_position;
    int32_t error_code;
    int32_t restore_error;

    initial_read_position = buffer.read_position();
    error_code = buffer.read_u32_le(&payload_size);
    if (error_code != FT_ERR_SUCCESS
        || static_cast<ft_size_t>(payload_size) > buffer.remaining()
        || buffer.remaining() - payload_size < 4U)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_IO);
    }
    error_code = buffer.view(buffer.read_position(), payload_size,
        &payload_data);
    if (error_code != FT_ERR_SUCCESS)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (error_code);
    }
    actual_checksum = game_world_delta_snapshot_checksum(payload_data,
        payload_size);
    error_code = buffer.skip(payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&expected_checksum);
    if (error_code != FT_ERR_SUCCESS)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (error_code);
    }
    if (expected_checksum != actual_checksum)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    error_code = chunk.deserialize_view(payload_data, payload_size);
    if (error_code != FT_ERR_SUCCESS)
    {
        restore_error = buffer.set_read_position(initial_read_position);
        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
    }
    return (error_code);
}

game_world_delta_history::game_world_delta_history() noexcept
    : _entries(), _capacity(0U), _oldest_index(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_world_delta_history::~game_world_delta_history() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_world_delta_history::initialize(uint32_t capacity) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (capacity == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_entries.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    this->_entries.reserve(capacity);
    if (this->_entries.get_error() != FT_ERR_SUCCESS)
    {
        (void)this->_entries.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    this->_capacity = capacity;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_history::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_entries.destroy();
    this->_capacity = 0U;
    this->_oldest_index = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (destroy_error);
}

uint32_t game_world_delta_history::entry_index(uint32_t offset) const noexcept
{
    const uint32_t tail_size = this->_capacity - this->_oldest_index;
    if (offset < tail_size)
        return (this->_oldest_index + offset);
    return (offset - tail_size);
}

int32_t game_world_delta_history::append(const game_block_delta &delta) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (delta.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || delta.revision == 0U
        || delta.revision != delta.previous_revision + 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t count = static_cast<uint32_t>(this->_entries.size());
    if (count != 0U
        && delta.previous_revision != this->_entries[this->entry_index(count - 1U)].revision)
        return (FT_ERR_INVALID_STATE);
    if (count == this->_capacity)
    {
        this->_entries[this->_oldest_index] = delta;
        this->_oldest_index += 1U;
        if (this->_oldest_index == this->_capacity)
            this->_oldest_index = 0U;
        return (FT_ERR_SUCCESS);
    }
    if (this->_entries.push_back(delta) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_history::get_since(uint64_t revision,
    ft_vector<game_block_delta> &deltas) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    const uint32_t count = static_cast<uint32_t>(this->_entries.size());
    if (count == 0U)
    {
        deltas.clear();
        return (deltas.get_error());
    }
    const uint64_t oldest_revision = this->_entries[this->_oldest_index].revision;
    if (revision < oldest_revision - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    // Appends keep the retained revisions consecutive, even across the wrap.
    const uint64_t latest_revision = oldest_revision + (count - 1U);
    if (revision >= latest_revision)
    {
        deltas.clear();
        return (deltas.get_error());
    }
    uint32_t offset = 0U;
    if (revision >= oldest_revision)
        offset = static_cast<uint32_t>(revision - oldest_revision + 1U);
    const uint32_t recovered_count = count - offset;
    const uint32_t index = this->entry_index(offset);
    if (recovered_count == 1U)
    {
        deltas.clear();
        const int32_t result = deltas.get_error();
        if (result != FT_ERR_SUCCESS)
            return (result);
        return (deltas.push_back(this->_entries[index]));
    }
    uint32_t first_span = this->_capacity - index;
    if (first_span > recovered_count)
        first_span = recovered_count;
    const game_block_delta *entries = this->_entries.begin();
    deltas.reserve(recovered_count);
    int32_t result = deltas.get_error();
    if (result != FT_ERR_SUCCESS)
        return (result);
    deltas.clear();
    result = deltas.get_error();
    if (result == FT_ERR_SUCCESS)
        result = deltas.append(entries + index, first_span);
    if (result == FT_ERR_SUCCESS && first_span != recovered_count)
        result = deltas.append(entries, recovered_count - first_span);
    return (result);
}

uint64_t game_world_delta_history::get_oldest_revision() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_entries.empty() != FT_FALSE)
        return (0U);
    return (this->_entries[this->_oldest_index].revision);
}

uint64_t game_world_delta_history::get_latest_revision() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    const uint32_t count = static_cast<uint32_t>(this->_entries.size());
    if (count == 0U)
        return (0U);
    return (this->_entries[this->entry_index(count - 1U)].revision);
}

uint32_t game_world_delta_history::size() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    return (static_cast<uint32_t>(this->_entries.size()));
}

game_world_delta_interest_set::game_world_delta_interest_set() noexcept
    : _entries(), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_world_delta_interest_set::~game_world_delta_interest_set() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_world_delta_interest_set::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (this->_entries.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_entries.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (destroy_error);
}

static ft_size_t game_world_delta_interest_group_start(const game_world_delta_interest *entries,
    ft_size_t count, uint64_t world_id, int32_t chunk_x, int32_t chunk_z) noexcept
{
    ft_size_t first = 0U;
    while (first < count)
    {
        const ft_size_t middle = first + (count - first) / 2U;
        const game_world_delta_interest &entry = entries[middle];
        if (entry.world_id < world_id || (entry.world_id == world_id
                && (entry.chunk_x < chunk_x || (entry.chunk_x == chunk_x && entry.chunk_z < chunk_z))))
            first = middle + 1U;
        else
            count = middle;
    }
    return (first);
}

static ft_bool game_world_delta_interest_matches(const game_world_delta_interest &entry,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z) noexcept
{
    return (entry.world_id == world_id && entry.chunk_x == chunk_x && entry.chunk_z == chunk_z);
}

int32_t game_world_delta_interest_set::find(uint64_t client_id,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    ft_size_t *index_out) const noexcept
{
    if (index_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t count = this->_entries.size();
    const game_world_delta_interest *entries = this->_entries.begin();
    ft_size_t index = game_world_delta_interest_group_start(entries, count, world_id, chunk_x, chunk_z);
    while (index < count && game_world_delta_interest_matches(entries[index], world_id, chunk_x, chunk_z))
    {
        if (entries[index].client_id == client_id)
        {
            *index_out = index;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    *index_out = index;
    return (FT_ERR_NOT_FOUND);
}

int32_t game_world_delta_interest_set::subscribe(uint64_t client_id,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    uint64_t snapshot_revision) noexcept
{
    game_world_delta_interest entry;
    ft_size_t index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (client_id == 0U || world_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->find(client_id, world_id, chunk_x, chunk_z, &index);
    if (error_code == FT_ERR_SUCCESS)
    {
        game_world_delta_interest &existing = this->_entries[index];
        existing.snapshot_revision = snapshot_revision;
        existing.acknowledged_revision = 0U;
        existing.snapshot_pending = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (error_code != FT_ERR_NOT_FOUND)
        return (error_code);
    entry.client_id = client_id;
    entry.world_id = world_id;
    entry.chunk_x = chunk_x;
    entry.chunk_z = chunk_z;
    entry.snapshot_revision = snapshot_revision;
    entry.acknowledged_revision = 0U;
    entry.snapshot_pending = FT_TRUE;
    if (index == this->_entries.size())
        return (this->_entries.push_back(entry));
    (void)this->_entries.insert(this->_entries.begin() + index, entry);
    return (this->_entries.get_error());
}

int32_t game_world_delta_interest_set::unsubscribe(uint64_t client_id,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    error_code = this->find(client_id, world_id, chunk_x, chunk_z, &index);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_entries.erase(this->_entries.begin() + index);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::acknowledge_snapshot(
    uint64_t client_id, uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    uint64_t snapshot_revision) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    error_code = this->find(client_id, world_id, chunk_x, chunk_z, &index);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    game_world_delta_interest &entry = this->_entries[index];
    if (entry.snapshot_pending == FT_FALSE || snapshot_revision != entry.snapshot_revision)
        return (FT_ERR_INVALID_STATE);
    entry.acknowledged_revision = snapshot_revision;
    entry.snapshot_pending = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::acknowledge_revision(
    uint64_t client_id, uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    uint64_t revision) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    error_code = this->find(client_id, world_id, chunk_x, chunk_z, &index);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    game_world_delta_interest &entry = this->_entries[index];
    if (entry.snapshot_pending != FT_FALSE || revision < entry.acknowledged_revision)
        return (FT_ERR_INVALID_STATE);
    entry.acknowledged_revision = revision;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::collect_live_clients(
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    ft_vector<uint64_t> &client_ids) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    client_ids.clear();
    const int32_t clear_error = client_ids.get_error();
    if (clear_error != FT_ERR_SUCCESS)
        return (clear_error);
    const ft_size_t count = this->_entries.size();
    const game_world_delta_interest *entries = this->_entries.begin();
    ft_size_t index = game_world_delta_interest_group_start(entries, count, world_id, chunk_x, chunk_z);
    constexpr ft_size_t BATCH_SIZE = 64U;
    uint64_t batch[BATCH_SIZE];
    ft_size_t batch_count = 0U;
    while (index < count && game_world_delta_interest_matches(entries[index], world_id, chunk_x, chunk_z))
    {
        if (entries[index].snapshot_pending == FT_FALSE)
        {
            batch[batch_count++] = entries[index].client_id;
            if (batch_count == BATCH_SIZE)
            {
                const int32_t result = client_ids.append(batch, batch_count);
                if (result != FT_ERR_SUCCESS)
                    return (result);
                batch_count = 0U;
            }
        }
        index += 1U;
    }
    if (batch_count == 0U)
        return (FT_ERR_SUCCESS);
    if (batch_count == 1U)
        return (client_ids.push_back(batch[0]));
    return (client_ids.append(batch, batch_count));
}

ft_bool game_world_delta_interest_set::is_snapshot_pending(
    uint64_t client_id, uint64_t world_id, int32_t chunk_x,
    int32_t chunk_z) const noexcept
{
    ft_size_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (this->find(client_id, world_id, chunk_x, chunk_z, &index)
        != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (this->_entries[index].snapshot_pending);
}

uint32_t game_world_delta_interest_set::size() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    return (static_cast<uint32_t>(this->_entries.size()));
}

game_world_delta_channel::game_world_delta_channel() noexcept
    : _chunk(ft_nullptr), _history(), _interests(), _world_id(0U),
      _chunk_x(0), _chunk_z(0), _server_tick(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_world_delta_channel::~game_world_delta_channel() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_world_delta_channel::initialize(game_voxel_chunk &chunk,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    uint32_t history_capacity) noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (world_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_history.initialize(history_capacity);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_interests.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_history.destroy();
        return (error_code);
    }
    this->_chunk = &chunk;
    this->_world_id = world_id;
    this->_chunk_x = chunk_x;
    this->_chunk_z = chunk_z;
    this->_server_tick = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_channel::destroy() noexcept
{
    int32_t interest_destroy_error;
    int32_t history_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    interest_destroy_error = this->_interests.destroy();
    history_destroy_error = this->_history.destroy();
    this->_chunk = ft_nullptr;
    this->_world_id = 0U;
    this->_chunk_x = 0;
    this->_chunk_z = 0;
    this->_server_tick = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (interest_destroy_error != FT_ERR_SUCCESS)
        return (interest_destroy_error);
    return (history_destroy_error);
}

int32_t game_world_delta_channel::apply_request(
    const game_block_change_request &request, game_block_delta &delta) noexcept
{
    uint64_t revision_before;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (request.world_id != this->_world_id
        || request.chunk_x != this->_chunk_x
        || request.chunk_z != this->_chunk_z)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_chunk->apply_authoritative_block_change_with_revision(request,
        &delta, &revision_before);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (delta.revision == revision_before)
        return (FT_ERR_SUCCESS);
    if (delta.revision != revision_before + 1U)
        return (FT_ERR_INVALID_STATE);
    if (this->_server_tick != UINT64_MAX)
        this->_server_tick += 1U;
    delta.server_tick = this->_server_tick;
    error_code = this->_history.append(delta);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_channel::subscribe(uint64_t client_id,
    uint64_t snapshot_revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (snapshot_revision > this->_chunk->get_revision())
        return (FT_ERR_INVALID_STATE);
    return (this->_interests.subscribe(client_id, this->_world_id,
        this->_chunk_x, this->_chunk_z, snapshot_revision));
}

int32_t game_world_delta_channel::unsubscribe(uint64_t client_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_interests.unsubscribe(client_id, this->_world_id,
        this->_chunk_x, this->_chunk_z));
}

int32_t game_world_delta_channel::acknowledge_snapshot(uint64_t client_id,
    uint64_t snapshot_revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (snapshot_revision > this->_chunk->get_revision())
        return (FT_ERR_INVALID_STATE);
    return (this->_interests.acknowledge_snapshot(client_id, this->_world_id,
        this->_chunk_x, this->_chunk_z, snapshot_revision));
}

int32_t game_world_delta_channel::acknowledge_revision(uint64_t client_id,
    uint64_t revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (revision > this->_chunk->get_revision())
        return (FT_ERR_INVALID_STATE);
    return (this->_interests.acknowledge_revision(client_id, this->_world_id,
        this->_chunk_x, this->_chunk_z, revision));
}

int32_t game_world_delta_channel::collect_live_clients(
    ft_vector<uint64_t> &client_ids) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (this->_interests.collect_live_clients(this->_world_id,
        this->_chunk_x, this->_chunk_z, client_ids));
}

int32_t game_world_delta_channel::recover_from(uint64_t revision,
    ft_vector<game_block_delta> &deltas) const noexcept
{
    uint64_t current_revision;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    current_revision = this->_chunk->get_revision();
    if (revision > current_revision)
        return (FT_ERR_INVALID_STATE);
    if (revision < current_revision && this->_history.size() == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->_history.get_since(revision, deltas));
}

int32_t game_world_delta_channel::serialize_snapshot(
    ft_byte_buffer &buffer) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    return (game_world_delta_snapshot_serialize(*this->_chunk, buffer));
}

uint64_t game_world_delta_channel::get_revision() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_chunk == ft_nullptr)
        return (0U);
    return (this->_chunk->get_revision());
}

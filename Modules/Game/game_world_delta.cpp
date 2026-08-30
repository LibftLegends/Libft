#include "game_world_delta.hpp"
#include "game_voxel_chunk.hpp"

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

static int32_t game_world_delta_read_request_fields(
    game_block_change_request &request, ft_byte_buffer &buffer) noexcept
{
    uint32_t chunk_x;
    uint32_t chunk_z;
    int32_t error_code;

    error_code = buffer.read_u16_le(&request.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&request.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&request.request_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&request.world_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&chunk_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&chunk_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&request.expected_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&request.expected_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&request.requested_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&request.local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u16_le(&request.local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&request.local_z);
    if (error_code == FT_ERR_SUCCESS)
    {
        request.chunk_x = static_cast<int32_t>(chunk_x);
        request.chunk_z = static_cast<int32_t>(chunk_z);
    }
    return (error_code);
}

int32_t game_block_change_request_serialize(
    const game_block_change_request &request, ft_byte_buffer &buffer) noexcept
{
    int32_t error_code;

    error_code = buffer.append_u16_le(request.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(request.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(request.request_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(request.world_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(static_cast<uint32_t>(request.chunk_x));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(static_cast<uint32_t>(request.chunk_z));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(request.expected_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(request.expected_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(request.requested_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u8(request.local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u16_le(request.local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u8(request.local_z);
    return (error_code);
}

int32_t game_block_change_request_deserialize(
    game_block_change_request &request, ft_byte_buffer &buffer) noexcept
{
    game_block_change_request temporary_request;
    int32_t error_code;

    error_code = game_world_delta_read_request_fields(temporary_request,
        buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (temporary_request.protocol_version
            != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || temporary_request.local_x >= 16U
        || temporary_request.local_y >= 256U
        || temporary_request.local_z >= 16U)
        return (FT_ERR_INVALID_ARGUMENT);
    request = temporary_request;
    return (FT_ERR_SUCCESS);
}

int32_t game_block_delta_serialize(const game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept
{
    int32_t error_code;

    error_code = buffer.append_u16_le(delta.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(delta.session_id);
    if (error_code == FT_ERR_SUCCESS)
    error_code = buffer.append_u64_le(delta.request_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(delta.world_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(static_cast<uint32_t>(delta.chunk_x));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(static_cast<uint32_t>(delta.chunk_z));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(delta.previous_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(delta.revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u32_le(delta.current_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u8(delta.player_modified);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u64_le(delta.server_tick);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u8(delta.local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u16_le(delta.local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append_u8(delta.local_z);
    return (error_code);
}

int32_t game_block_delta_deserialize(game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept
{
    game_block_delta temporary_delta;
    uint32_t chunk_x;
    uint32_t chunk_z;
    int32_t error_code;

    error_code = buffer.read_u16_le(&temporary_delta.protocol_version);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.session_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.request_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.world_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&chunk_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&chunk_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.previous_revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&temporary_delta.current_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&temporary_delta.player_modified);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&temporary_delta.server_tick);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&temporary_delta.local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u16_le(&temporary_delta.local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&temporary_delta.local_z);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (temporary_delta.protocol_version
            != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || temporary_delta.local_x >= 16U
        || temporary_delta.local_y >= 256U
        || temporary_delta.local_z >= 16U
        || temporary_delta.player_modified > 1U
        || temporary_delta.revision == 0U
        || temporary_delta.previous_revision + 1U
            != temporary_delta.revision)
        return (FT_ERR_INVALID_ARGUMENT);
    temporary_delta.chunk_x = static_cast<int32_t>(chunk_x);
    temporary_delta.chunk_z = static_cast<int32_t>(chunk_z);
    delta = temporary_delta;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_snapshot_serialize(const game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept
{
    ft_byte_buffer payload;
    ft_byte_buffer temporary_buffer;
    const uint8_t *payload_data;
    uint32_t payload_size;
    int32_t error_code;

    error_code = payload.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk.serialize(payload);
    if (error_code == FT_ERR_SUCCESS
        && payload.size() > UINT32_MAX)
        error_code = FT_ERR_OUT_OF_RANGE;
    if (error_code == FT_ERR_SUCCESS)
        error_code = payload.view(0U, payload.size(), &payload_data);
    if (error_code == FT_ERR_SUCCESS)
    {
        payload_size = static_cast<uint32_t>(payload.size());
        error_code = temporary_buffer.initialize(buffer);
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = temporary_buffer.append_u32_le(payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = temporary_buffer.append(payload_data, payload.size());
    if (error_code == FT_ERR_SUCCESS)
        error_code = temporary_buffer.append_u32_le(
            game_world_delta_snapshot_checksum(payload_data, payload.size()));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.move(temporary_buffer);
    (void)payload.destroy();
    (void)temporary_buffer.destroy();
    return (error_code);
}

int32_t game_world_delta_snapshot_deserialize(game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept
{
    ft_byte_buffer payload;
    const uint8_t *payload_data;
    uint32_t payload_size;
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    int32_t error_code;

    error_code = buffer.read_u32_le(&payload_size);
    if (error_code != FT_ERR_SUCCESS
        || static_cast<ft_size_t>(payload_size) > buffer.remaining()
        || buffer.remaining() - payload_size < 4U)
        return (FT_ERR_IO);
    error_code = buffer.view(buffer.read_position(), payload_size,
        &payload_data);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    actual_checksum = game_world_delta_snapshot_checksum(payload_data,
        payload_size);
    error_code = buffer.skip(payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&expected_checksum);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (expected_checksum != actual_checksum)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = payload.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = payload.append(payload_data, payload_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = chunk.deserialize(payload);
    (void)payload.destroy();
    return (error_code);
}

game_world_delta_history::game_world_delta_history() noexcept
    : _entries(), _capacity(0U),
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
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->_entries.destroy();
    this->_capacity = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_history::append(const game_block_delta &delta) noexcept
{
    uint64_t latest_revision;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (delta.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || delta.revision == 0U
        || delta.revision != delta.previous_revision + 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_entries.empty() == FT_FALSE)
    {
        latest_revision = this->_entries[this->_entries.size() - 1U].revision;
        if (delta.previous_revision != latest_revision)
            return (FT_ERR_INVALID_STATE);
    }
    if (this->_entries.size() >= this->_capacity)
        this->_entries.erase(this->_entries.begin());
    if (this->_entries.push_back(delta) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_history::get_since(uint64_t revision,
    ft_vector<game_block_delta> &deltas) const noexcept
{
    ft_size_t index;
    uint64_t oldest_revision;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    deltas.clear();
    if (this->_entries.empty() != FT_FALSE)
        return (FT_ERR_SUCCESS);
    oldest_revision = this->_entries[0U].revision;
    if (revision != UINT64_MAX && revision + 1U < oldest_revision)
        return (FT_ERR_OUT_OF_RANGE);
    index = 0U;
    while (index < this->_entries.size())
    {
        if (this->_entries[index].revision > revision)
        {
            error_code = deltas.push_back(this->_entries[index]);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

uint64_t game_world_delta_history::get_oldest_revision() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_entries.empty() != FT_FALSE)
        return (0U);
    return (this->_entries[0U].revision);
}

uint64_t game_world_delta_history::get_latest_revision() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_entries.empty() != FT_FALSE)
        return (0U);
    return (this->_entries[this->_entries.size() - 1U].revision);
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
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->_entries.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::find(uint64_t client_id,
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    ft_size_t *index_out) const noexcept
{
    ft_size_t index;

    if (index_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_entries.size())
    {
        if (this->_entries[index].client_id == client_id
            && this->_entries[index].world_id == world_id
            && this->_entries[index].chunk_x == chunk_x
            && this->_entries[index].chunk_z == chunk_z)
        {
            *index_out = index;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
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
        this->_entries[index].snapshot_revision = snapshot_revision;
        this->_entries[index].acknowledged_revision = 0U;
        this->_entries[index].snapshot_pending = FT_TRUE;
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
    return (this->_entries.push_back(entry));
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
    if (this->_entries[index].snapshot_pending == FT_FALSE
        || snapshot_revision != this->_entries[index].snapshot_revision)
        return (FT_ERR_INVALID_STATE);
    this->_entries[index].acknowledged_revision = snapshot_revision;
    this->_entries[index].snapshot_pending = FT_FALSE;
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
    if (this->_entries[index].snapshot_pending != FT_FALSE
        || revision < this->_entries[index].acknowledged_revision)
        return (FT_ERR_INVALID_STATE);
    this->_entries[index].acknowledged_revision = revision;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_interest_set::collect_live_clients(
    uint64_t world_id, int32_t chunk_x, int32_t chunk_z,
    ft_vector<uint64_t> &client_ids) const noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    client_ids.clear();
    index = 0U;
    while (index < this->_entries.size())
    {
        if (this->_entries[index].world_id == world_id
            && this->_entries[index].chunk_x == chunk_x
            && this->_entries[index].chunk_z == chunk_z
            && this->_entries[index].snapshot_pending == FT_FALSE)
        {
            error_code = client_ids.push_back(this->_entries[index].client_id);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
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
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->_interests.destroy();
    (void)this->_history.destroy();
    this->_chunk = ft_nullptr;
    this->_world_id = 0U;
    this->_chunk_x = 0;
    this->_chunk_z = 0;
    this->_server_tick = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t game_world_delta_channel::apply_request(
    const game_block_change_request &request, game_block_delta &delta) noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (request.world_id != this->_world_id
        || request.chunk_x != this->_chunk_x
        || request.chunk_z != this->_chunk_z)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = this->_chunk->apply_authoritative_block_change(request,
        &delta);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
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
    return (this->_interests.acknowledge_snapshot(client_id, this->_world_id,
        this->_chunk_x, this->_chunk_z, snapshot_revision));
}

int32_t game_world_delta_channel::acknowledge_revision(uint64_t client_id,
    uint64_t revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
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

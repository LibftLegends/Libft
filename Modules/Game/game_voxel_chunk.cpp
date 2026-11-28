#include "game_voxel_chunk.hpp"
#include "game_world_delta.hpp"
#include "../Basic/basic.hpp"
#include "../Buffer/buffer_endian.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno_internal.hpp"
#include "../File/file_utils.hpp"
#include "../Printf/printf.hpp"
#include <algorithm>
#include <cstdio>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <functional>
#define GAME_VOXEL_CHUNK_MAGIC 0x474D4348U
#define GAME_VOXEL_CHUNK_VERSION 5U
#define GAME_VOXEL_CHUNK_LEGACY_VERSION 4U
#define GAME_VOXEL_CHUNK_LEGACY_MAX_DIRTY_EDITS 1048576U
#define GAME_VOXEL_OVERRIDE_RECORD_BYTES 10U
#define GAME_VOXEL_OVERRIDE_BATCH_COUNT 64U
static constexpr uint32_t GAME_VOXEL_INDEX_THRESHOLD = 256U;
static constexpr ft_size_t GAME_VOXEL_PALETTE_BYTES = 256U * sizeof(uint32_t);
static constexpr ft_size_t GAME_VOXEL_CHUNK_PREFIX_BYTES = 8U;
static constexpr ft_size_t GAME_VOXEL_CHUNK_METADATA_BYTES = 30U;

struct game_voxel_chunk_reader
{
    const uint8_t *data;
    ft_size_t size;
    ft_size_t position;

    int32_t read(void *destination, ft_size_t length) noexcept;
    ft_size_t read_position() const noexcept;
    int32_t set_read_position(ft_size_t offset) noexcept;
    ft_size_t remaining() const noexcept;
};

int32_t game_voxel_chunk_reader::read(void *destination, ft_size_t length) noexcept
{
    if (length > this->remaining())
        return (FT_ERR_OUT_OF_RANGE);
    ft_memcpy(destination, this->data + this->position, length);
    this->position += length;
    return (FT_ERR_SUCCESS);
}

ft_size_t game_voxel_chunk_reader::read_position() const noexcept
{
    return (this->position);
}

int32_t game_voxel_chunk_reader::set_read_position(ft_size_t offset) noexcept
{
    if (offset > this->size)
        return (FT_ERR_OUT_OF_RANGE);
    this->position = offset;
    return (FT_ERR_SUCCESS);
}

ft_size_t game_voxel_chunk_reader::remaining() const noexcept
{
    return (this->size - this->position);
}

template <typename Reader, typename Value>
static int32_t game_voxel_read_le(Reader &buffer, Value *value) noexcept
{
    uint8_t bytes[sizeof(Value)];
    const int32_t status = buffer.read(bytes, sizeof(bytes));
    if (status == FT_ERR_SUCCESS)
        *value = buffer_load_le<Value>(bytes);
    return (status);
}

static int32_t game_voxel_serialize_overrides(ft_byte_buffer &buffer,
    const game_voxel_block_override *overrides, uint32_t count) noexcept
{
    uint8_t bytes[GAME_VOXEL_OVERRIDE_BATCH_COUNT * GAME_VOXEL_OVERRIDE_RECORD_BYTES];
    uint32_t batch_count;
    uint32_t index;
    int32_t error_code;

    while (count > 0U)
    {
        batch_count = count;
        if (batch_count > GAME_VOXEL_OVERRIDE_BATCH_COUNT)
            batch_count = GAME_VOXEL_OVERRIDE_BATCH_COUNT;
        index = 0U;
        while (index < batch_count)
        {
            const game_voxel_block_override &entry = overrides[index];
            uint8_t *record = bytes + index * GAME_VOXEL_OVERRIDE_RECORD_BYTES;
            buffer_store_le(record, entry.local_index);
            buffer_store_le(record + 2U, entry.generated_block_id);
            buffer_store_le(record + 6U, entry.current_block_id);
            ++index;
        }
        error_code = buffer.append(bytes,
            batch_count * GAME_VOXEL_OVERRIDE_RECORD_BYTES);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        overrides += batch_count;
        count -= batch_count;
    }
    return (FT_ERR_SUCCESS);
}

template <typename Reader>
static int32_t game_voxel_deserialize_overrides(Reader &buffer,
    game_voxel_block_override *overrides, uint32_t count) noexcept
{
    uint8_t seen_indices[(UINT16_MAX + 1U) / 8U] = {};
    uint8_t bytes[GAME_VOXEL_OVERRIDE_BATCH_COUNT * GAME_VOXEL_OVERRIDE_RECORD_BYTES];
    while (count != 0U)
    {
        uint32_t batch = count;
        if (batch > GAME_VOXEL_OVERRIDE_BATCH_COUNT)
            batch = GAME_VOXEL_OVERRIDE_BATCH_COUNT;
        const int32_t status = buffer.read(bytes, batch * GAME_VOXEL_OVERRIDE_RECORD_BYTES);
        if (status != FT_ERR_SUCCESS)
            return (status);
        uint32_t index = 0U;
        while (index < batch)
        {
            const uint8_t *record = bytes + index * GAME_VOXEL_OVERRIDE_RECORD_BYTES;
            const uint16_t local_index = buffer_load_le<uint16_t>(record);
            const uint32_t generated = buffer_load_le<uint32_t>(record + 2U);
            const uint32_t current = buffer_load_le<uint32_t>(record + 6U);
            uint8_t &seen = seen_indices[local_index / 8U];
            const uint8_t mask = static_cast<uint8_t>(1U << (local_index % 8U));
            if ((seen & mask) != 0U || generated == current)
                return (FT_ERR_INVALID_ARGUMENT);
            seen |= mask;
            overrides[index] = {local_index, generated, current};
            ++index;
        }
        overrides += batch;
        count -= batch;
    }
    return (FT_ERR_SUCCESS);
}

thread_local int32_t game_voxel_chunk_section::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_voxel_chunk::_last_error = FT_ERR_SUCCESS;

int32_t game_voxel_chunk_section::set_error(int32_t error_code) noexcept
{
    game_voxel_chunk_section::_last_error = error_code;
    return (error_code);
}

game_voxel_chunk_section::game_voxel_chunk_section() noexcept
    : _uniform_block_id(GAME_VOXEL_AIR_BLOCK), _palette(ft_nullptr),
    _indices(ft_nullptr), _palette_size(0),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED), _uniform_candidate(0U),
    _uniform_candidate_count(0U)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_voxel_chunk_section::~game_voxel_chunk_section() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_voxel_chunk_section::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk_section::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    this->_palette = ft_nullptr;
    this->_indices = ft_nullptr;
    this->_palette_size = 0;
    this->_uniform_candidate = 0U;
    this->_uniform_candidate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk_section::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_palette != ft_nullptr)
        cma_free(this->_palette);
    this->_uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    this->_palette = ft_nullptr;
    this->_indices = ft_nullptr;
    this->_palette_size = 0;
    this->_uniform_candidate = 0U;
    this->_uniform_candidate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk_section::move(
    game_voxel_chunk_section &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_voxel_chunk_section::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    this->_uniform_block_id = other._uniform_block_id;
    this->_palette = other._palette;
    this->_indices = other._indices;
    this->_palette_size = other._palette_size;
    this->_initialised_state = other._initialised_state;
    this->_uniform_candidate = other._uniform_candidate;
    this->_uniform_candidate_count = other._uniform_candidate_count;
    other._uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    other._palette = ft_nullptr;
    other._indices = ft_nullptr;
    other._palette_size = 0;
    other._uniform_candidate = 0U;
    other._uniform_candidate_count = 0U;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk_section::allocate_payload() noexcept
{
    uint8_t *storage = static_cast<uint8_t *>(cma_malloc(
        GAME_VOXEL_PALETTE_BYTES + GAME_VOXEL_SECTION_BLOCKS));
    if (storage == ft_nullptr)
        return (this->set_error(FT_ERR_NO_MEMORY));
    this->_palette = reinterpret_cast<uint32_t *>(storage);
    this->_indices = storage + GAME_VOXEL_PALETTE_BYTES;
    return (FT_ERR_SUCCESS);
}

int32_t game_voxel_chunk_section::find_or_add_palette_index(
    uint32_t block_id, uint8_t *palette_index) noexcept
{
    uint16_t index;

    index = 0;
    while (index < this->_palette_size)
    {
        if (this->_palette[index] == block_id)
        {
            *palette_index = static_cast<uint8_t>(index);
            return (FT_ERR_SUCCESS);
        }
        index += 1;
    }
    if (this->_palette_size == 256)
        return (FT_ERR_FULL);
    this->_palette[this->_palette_size] = block_id;
    *palette_index = static_cast<uint8_t>(this->_palette_size);
    this->_palette_size += 1;
    return (FT_ERR_SUCCESS);
}

int32_t game_voxel_chunk_section::copy_payload(
    const game_voxel_chunk_section &other) noexcept
{
    int32_t error_code;

    this->_uniform_block_id = other._uniform_block_id;
    if (other._indices == ft_nullptr)
        return (FT_ERR_SUCCESS);
    error_code = this->allocate_payload();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_palette_size = other._palette_size;
    this->_uniform_candidate = other._uniform_candidate;
    this->_uniform_candidate_count = other._uniform_candidate_count;
    ft_memcpy(this->_palette, other._palette,
        sizeof(uint32_t) * this->_palette_size);
    ft_memcpy(this->_indices, other._indices, GAME_VOXEL_SECTION_BLOCKS);
    return (FT_ERR_SUCCESS);
}

void game_voxel_chunk_section::collapse_to_candidate() noexcept
{
    this->_uniform_block_id = this->_palette[this->_uniform_candidate];
    cma_free(this->_palette);
    this->_palette = ft_nullptr;
    this->_indices = ft_nullptr;
    this->_palette_size = 0;
    this->_uniform_candidate = 0U;
    this->_uniform_candidate_count = 0U;
    return ;
}

uint32_t game_voxel_chunk_section::get_block(
    uint16_t local_index) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk_section::get_block");
    if (local_index >= GAME_VOXEL_SECTION_BLOCKS)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk_section::get_block",
            "local index is out of range");
        return (GAME_VOXEL_AIR_BLOCK);
    }
    if (this->_indices == ft_nullptr || this->_palette == ft_nullptr)
        return (this->_uniform_block_id);
    return (this->_palette[this->_indices[local_index]]);
}

void game_voxel_chunk_section::copy_rows(uint32_t *output, uint32_t first_index,
    uint32_t width, uint32_t block_stride) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk_section::copy_rows");
    if (this->_indices == ft_nullptr || this->_palette == ft_nullptr)
    {
        std::fill_n(output, width * GAME_VOXEL_SECTION_EDGE, this->_uniform_block_id);
        return ;
    }
    uint32_t row = 0U;
    while (row < GAME_VOXEL_SECTION_EDGE)
    {
        uint32_t column = 0U;
        while (column < width)
        {
            output[column] = this->_palette[this->_indices[first_index + column * block_stride]];
            ++column;
        }
        output += width;
        first_index += GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH;
        ++row;
    }
    return ;
}

int32_t game_voxel_chunk_section::set_block(uint16_t local_index,
    uint32_t block_id) noexcept
{
    uint8_t previous_index = 0U;
    uint8_t palette_index = 1U;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk_section::set_block");
    if (local_index >= GAME_VOXEL_SECTION_BLOCKS)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (this->_indices == ft_nullptr)
    {
        if (block_id == this->_uniform_block_id)
            return (this->set_error(FT_ERR_SUCCESS));
        error_code = this->allocate_payload();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        this->_palette[0] = this->_uniform_block_id;
        this->_palette[1] = block_id;
        this->_palette_size = 2U;
        ft_bzero(this->_indices, GAME_VOXEL_SECTION_BLOCKS);
        this->_uniform_candidate = 0U;
        this->_uniform_candidate_count = GAME_VOXEL_SECTION_BLOCKS;
    }
    else
    {
        previous_index = this->_indices[local_index];
        if (this->_palette[previous_index] == block_id)
            return (this->set_error(FT_ERR_SUCCESS));
        error_code = this->find_or_add_palette_index(block_id, &palette_index);
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    this->_indices[local_index] = palette_index;
    if (palette_index == this->_uniform_candidate)
        ++this->_uniform_candidate_count;
    else if (previous_index == this->_uniform_candidate)
    {
        --this->_uniform_candidate_count;
        if (this->_uniform_candidate_count == 0U)
        {
            // Recount only when the tracked palette entry disappears.
            const uint32_t next_block = (local_index + 1U) % GAME_VOXEL_SECTION_BLOCKS;
            this->_uniform_candidate = this->_indices[next_block];
            this->_uniform_candidate_count = static_cast<uint16_t>(std::count(
                this->_indices, this->_indices + GAME_VOXEL_SECTION_BLOCKS,
                this->_uniform_candidate));
        }
    }
    if (this->_uniform_candidate_count == GAME_VOXEL_SECTION_BLOCKS)
        this->collapse_to_candidate();
    return (this->set_error(FT_ERR_SUCCESS));
}

ft_bool game_voxel_chunk_section::is_uniform() const noexcept
{
    return (this->_indices == ft_nullptr);
}

ft_bool game_voxel_chunk_section::is_materialized() const noexcept
{
    return (this->_indices != ft_nullptr);
}

uint32_t game_voxel_chunk_section::get_uniform_block() const noexcept
{
    return (this->_uniform_block_id);
}

uint16_t game_voxel_chunk_section::get_palette_size() const noexcept
{
    if (this->_indices == ft_nullptr)
        return (1);
    return (this->_palette_size);
}

uint32_t game_voxel_chunk_section::get_palette_block(
    uint8_t palette_index) const noexcept
{
    if (this->_indices == ft_nullptr)
        return (this->_uniform_block_id);
    if (palette_index >= this->_palette_size)
        return (GAME_VOXEL_AIR_BLOCK);
    return (this->_palette[palette_index]);
}

int32_t game_voxel_chunk_section::serialize(
    ft_byte_buffer &buffer) const noexcept
{
    if (this->_indices == ft_nullptr)
    {
        uint8_t bytes[1U + sizeof(uint32_t)];
        bytes[0] = 0U;
        buffer_store_le(bytes + 1U, this->_uniform_block_id);
        return (buffer.append(bytes, sizeof(bytes)));
    }
    uint8_t bytes[3U + GAME_VOXEL_PALETTE_BYTES];
    bytes[0] = 1U;
    buffer_store_le(bytes + 1U, this->_palette_size);
    uint16_t index = 0U;
    while (index < this->_palette_size)
    {
        buffer_store_le(bytes + 3U + index * sizeof(uint32_t), this->_palette[index]);
        ++index;
    }
    const int32_t error_code = buffer.append(bytes, 3U + index * sizeof(uint32_t));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (buffer.append(this->_indices, GAME_VOXEL_SECTION_BLOCKS));
}

int32_t game_voxel_chunk_section::deserialize(
    ft_byte_buffer &buffer) noexcept
{
    return (this->deserialize_from(buffer));
}

template <typename Reader>
int32_t game_voxel_chunk_section::deserialize_from(Reader &buffer) noexcept
{
    uint8_t section_type;
    uint16_t index;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read(&section_type, sizeof(section_type));
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(error_code));
    }
    if (section_type == 0)
    {
        error_code = game_voxel_read_le(buffer, &this->_uniform_block_id);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
        return (this->set_error(FT_ERR_SUCCESS));
    }
    if (section_type != 1)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    this->_uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    error_code = this->allocate_payload();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = game_voxel_read_le(buffer, &this->_palette_size);
    if (error_code != FT_ERR_SUCCESS || this->_palette_size == 0
        || this->_palette_size > 256)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    uint8_t palette_bytes[GAME_VOXEL_PALETTE_BYTES];
    error_code = buffer.read(palette_bytes, this->_palette_size * sizeof(uint32_t));
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(error_code));
    }
    index = 0U;
    while (index < this->_palette_size)
    {
        this->_palette[index] = buffer_load_le<uint32_t>(palette_bytes + index * sizeof(uint32_t));
        ++index;
    }
    error_code = buffer.read(this->_indices, GAME_VOXEL_SECTION_BLOCKS);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(error_code));
    }
    this->_uniform_candidate = this->_indices[0];
    this->_uniform_candidate_count = 0U;
    index = 0;
    while (index < GAME_VOXEL_SECTION_BLOCKS)
    {
        if (this->_indices[index] >= this->_palette_size)
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_IO));
        }
        if (this->_indices[index] == this->_uniform_candidate)
            ++this->_uniform_candidate_count;
        index += 1;
    }
    if (this->_uniform_candidate_count == GAME_VOXEL_SECTION_BLOCKS)
        this->collapse_to_candidate();
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk_section::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_voxel_chunk_section::get_error");
    return (game_voxel_chunk_section::_last_error);
}

const char *game_voxel_chunk_section::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_voxel_chunk_section::get_error_str");
    return (ft_strerror(game_voxel_chunk_section::_last_error));
}

int32_t game_voxel_chunk::set_error(int32_t error_code) noexcept
{
    game_voxel_chunk::_last_error = error_code;
    return (error_code);
}

game_voxel_chunk::game_voxel_chunk() noexcept
    : _sections(), _dirty(FT_FALSE), _generation_protected(FT_FALSE),
    _generation_metadata(), _biome_id(0U), _player_overrides(ft_nullptr),
    _player_override_count(0U), _player_override_capacity(0U), _revision(0U),
    _last_request_valid(FT_FALSE), _last_request(), _last_delta(),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED), _access_lock(ft_nullptr)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_voxel_chunk::grow_player_overrides(
    uint32_t minimum_capacity) noexcept
{
    uint32_t new_capacity;
    game_voxel_block_override *new_buffer;

    const uint32_t first = this->player_override_first();
    const uint32_t capacity = this->_player_override_capacity;
    const ft_bool can_compact = minimum_capacity <= capacity && first != 0U;
    constexpr uint32_t maximum = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH
        * GAME_VOXEL_CHUNK_HEIGHT;
    if (minimum_capacity <= capacity - first)
        return (FT_ERR_SUCCESS);
    if (minimum_capacity > maximum)
        return (FT_ERR_FULL);
    if (minimum_capacity <= capacity && (first >= capacity / 4U || capacity == maximum))
    {
        this->compact_player_overrides(first, FT_FALSE);
        return (FT_ERR_SUCCESS);
    }
    if (minimum_capacity <= capacity)
        minimum_capacity = capacity + 1U;
    if (this->_player_override_capacity == 0U)
        new_capacity = 16U;
    else
        new_capacity = this->_player_override_capacity;
    if (new_capacity < minimum_capacity)
    {
        while (new_capacity < minimum_capacity)
        {
            if (new_capacity > (GAME_VOXEL_CHUNK_WIDTH
                    * GAME_VOXEL_CHUNK_DEPTH * GAME_VOXEL_CHUNK_HEIGHT) / 2U)
            {
                new_capacity = GAME_VOXEL_CHUNK_WIDTH
                    * GAME_VOXEL_CHUNK_DEPTH * GAME_VOXEL_CHUNK_HEIGHT;
                break ;
            }
            new_capacity *= 2U;
        }
    }
    if (new_capacity < minimum_capacity)
        new_capacity = minimum_capacity;
    // Store the index after the records so both share one allocation and lifetime.
    ft_size_t bytes = sizeof(game_voxel_block_override) * new_capacity;
    if (new_capacity >= GAME_VOXEL_INDEX_THRESHOLD)
        bytes += (new_capacity * 2U + 1U) * sizeof(uint32_t);
    new_buffer = static_cast<game_voxel_block_override *>(cma_realloc(this->_player_overrides, bytes));
    if (new_buffer == ft_nullptr)
    {
        if (can_compact == FT_TRUE)
        {
            this->compact_player_overrides(first, FT_FALSE);
            return (FT_ERR_SUCCESS);
        }
        return (FT_ERR_NO_MEMORY);
    }
    this->_player_overrides = new_buffer;
    this->_player_override_capacity = new_capacity;
    this->compact_player_overrides(first, FT_TRUE);
    return (FT_ERR_SUCCESS);
}

static uint32_t game_voxel_override_bucket(uint16_t coordinate, uint32_t mask) noexcept
{
    const uint32_t value = static_cast<uint32_t>(coordinate) * 0x9E3779B1U;
    return ((value ^ (value >> 16U)) & mask);
}

uint32_t *game_voxel_chunk::player_override_index_table() const noexcept
{
    if (this->_player_override_capacity < GAME_VOXEL_INDEX_THRESHOLD)
        return (ft_nullptr);
    return (reinterpret_cast<uint32_t *>(this->_player_overrides + this->_player_override_capacity)
        + 1U);
}

uint32_t game_voxel_chunk::player_override_first() const noexcept
{
    const uint32_t *table = this->player_override_index_table();
    if (table == ft_nullptr)
        return (0U);
    return (*(table - 1));
}

void game_voxel_chunk::compact_player_overrides(uint32_t first, ft_bool rebuild_index) noexcept
{
    uint32_t *table = this->player_override_index_table();
    if (table == ft_nullptr)
        return ;
    if (first != 0U && this->_player_override_count != 0U)
        (void)ft_memmove(this->_player_overrides, this->_player_overrides + first,
            this->_player_override_count * sizeof(game_voxel_block_override));
    *(table - 1) = 0U;
    if (rebuild_index || this->_player_override_count < this->_player_override_capacity / 8U)
        this->rebuild_player_override_index();
    else
    {
        // The hashes and probe chains are unchanged when only the record positions move.
        const uint32_t slots = this->_player_override_capacity * 2U;
        uint32_t bucket = 0U;
        while (bucket < slots)
        {
            const uint32_t entry = table[bucket];
            table[bucket] = entry - first * static_cast<uint32_t>(entry != 0U);
            ++bucket;
        }
    }
    return ;
}

uint32_t game_voxel_chunk::player_override_bucket(const uint32_t *table,
    uint16_t coordinate, uint32_t mask) const noexcept
{
    uint32_t bucket = game_voxel_override_bucket(coordinate, mask);
    while (table[bucket] != 0U
        && this->_player_overrides[table[bucket] - 1U].local_index != coordinate)
        bucket = (bucket + 1U) & mask;
    return (bucket);
}

void game_voxel_chunk::rebuild_player_override_index() noexcept
{
    uint32_t *table = this->player_override_index_table();
    if (table == ft_nullptr)
        return ;
    const uint32_t slots = this->_player_override_capacity * 2U;
    ft_bzero(table, slots * sizeof(*table));
    uint32_t index = *(table - 1);
    const uint32_t end = index + this->_player_override_count;
    while (index < end)
    {
        const uint32_t bucket = this->player_override_bucket(table,
            this->_player_overrides[index].local_index, slots - 1U);
        table[bucket] = index + 1U;
        ++index;
    }
    return ;
}

void game_voxel_chunk::index_player_override(uint32_t index) noexcept
{
    uint32_t *table = this->player_override_index_table();
    if (table != ft_nullptr)
    {
        const uint32_t mask = this->_player_override_capacity * 2U - 1U;
        const uint32_t bucket = this->player_override_bucket(table,
            this->_player_overrides[index].local_index, mask);
        table[bucket] = index + 1U;
    }
    return ;
}

int32_t game_voxel_chunk::find_player_override(uint16_t local_index,
    uint32_t *override_index) const noexcept
{
    if (override_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t *table = this->player_override_index_table();
    if (table != ft_nullptr)
    {
        const uint32_t mask = this->_player_override_capacity * 2U - 1U;
        const uint32_t bucket = this->player_override_bucket(table, local_index, mask);
        if (table[bucket] == 0U)
            return (FT_ERR_NOT_FOUND);
        *override_index = table[bucket] - 1U;
        return (FT_ERR_SUCCESS);
    }
    uint32_t index = 0U;
    while (index < this->_player_override_count)
    {
        if (this->_player_overrides[index].local_index == local_index)
        {
            *override_index = index;
            return (FT_ERR_SUCCESS);
        }
        ++index;
    }
    return (FT_ERR_NOT_FOUND);
}

void game_voxel_chunk::remove_player_override_from_index(uint32_t *table,
    uint32_t index) noexcept
{
    const uint32_t mask = this->_player_override_capacity * 2U - 1U;
    uint32_t hole = this->player_override_bucket(table,
        this->_player_overrides[index].local_index, mask);
    uint32_t next = (hole + 1U) & mask;
    // Close the probe-chain gap for entries that were displaced past the hole.
    while (table[next] != 0U)
    {
        const uint32_t home = game_voxel_override_bucket(
            this->_player_overrides[table[next] - 1U].local_index, mask);
        if (((hole - home) & mask) < ((next - home) & mask))
        {
            table[hole] = table[next];
            hole = next;
        }
        next = (next + 1U) & mask;
    }
    table[hole] = 0U;

    return ;
}

void game_voxel_chunk::shift_player_override_index(uint32_t *table,
    uint32_t first, uint32_t end, ft_bool forward) noexcept
{
    const uint32_t slots = this->_player_override_capacity * 2U;
    const uint32_t mask = slots - 1U;
    if (end - first >= this->_player_override_capacity / 8U)
    {
        uint32_t adjustment = UINT32_MAX;
        if (forward == FT_TRUE)
            adjustment = 1U;
        uint32_t bucket = 0U;
        while (bucket < slots)
        {
            const uint32_t value = table[bucket];
            table[bucket] = value + adjustment * static_cast<uint32_t>(
                value > first && value <= end);
            ++bucket;
        }
        return ;
    }
    // Walk away from the removed record so adjusted positions never shadow
    // an entry we have yet to update.
    while (first < end)
    {
        uint32_t index;
        if (forward == FT_TRUE)
            index = --end;
        else
            index = first++;
        uint32_t bucket = game_voxel_override_bucket(this->_player_overrides[index].local_index, mask);
        while (table[bucket] != index + 1U)
            bucket = (bucket + 1U) & mask;
        table[bucket] = index + static_cast<uint32_t>(forward == FT_TRUE) * 2U;
    }
    return ;
}

void game_voxel_chunk::remove_player_override(uint32_t index) noexcept
{
    const uint32_t first = this->player_override_first();
    const uint32_t end = first + this->_player_override_count;
    if (index < first || index >= end)
        return ;
    uint32_t *table = this->player_override_index_table();
    if (table != ft_nullptr)
        this->remove_player_override_from_index(table, index);
    const uint32_t before = index - first;
    const uint32_t after = end - index - 1U;
    if (table != ft_nullptr && before < after)
    {
        this->shift_player_override_index(table, first, index, FT_TRUE);
        (void)ft_memmove(this->_player_overrides + first + 1U, this->_player_overrides + first,
            before * sizeof(game_voxel_block_override));
        *(table - 1) = first + 1U;
    }
    else if (after != 0U)
    {
        if (table != ft_nullptr)
            this->shift_player_override_index(table, index + 1U, end, FT_FALSE);
        (void)ft_memmove(this->_player_overrides + index, this->_player_overrides + index + 1U,
            after * sizeof(game_voxel_block_override));
    }
    --this->_player_override_count;
    if (table != ft_nullptr && this->_player_override_count == 0U)
        *(table - 1) = 0U;
    return ;
}

game_voxel_chunk::~game_voxel_chunk() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_access_lock != ft_nullptr)
    {
        (void)pt_rwlock_strategy_destroy(this->_access_lock);
        this->_access_lock->~t_pt_rwlock();
        cma_free(this->_access_lock);
        this->_access_lock = ft_nullptr;
    }
    return ;
}

uint16_t game_voxel_chunk::local_index(int32_t local_x, int32_t local_y,
    int32_t local_z) noexcept
{
    return (static_cast<uint16_t>(local_x + (local_z << 4)
        + ((local_y & 15) << 8)));
}

int32_t game_voxel_chunk::initialize() noexcept
{
    uint8_t section_index;
    int32_t error_code;
    t_pt_rwlock *new_lock;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    new_lock = static_cast<t_pt_rwlock *>(cma_malloc(sizeof(t_pt_rwlock)));
    if (new_lock == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    new (new_lock) t_pt_rwlock();
    error_code = pt_rwlock_strategy_init(new_lock,
        PT_RWLOCK_STRATEGY_WRITER_PRIORITY);
    if (error_code != FT_ERR_SUCCESS)
    {
        new_lock->~t_pt_rwlock();
        cma_free(new_lock);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_access_lock = new_lock;
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].initialize();
        if (error_code != FT_ERR_SUCCESS)
        {
            while (section_index > 0)
            {
                section_index -= 1;
                (void)this->_sections[section_index].destroy();
            }
            (void)pt_rwlock_strategy_destroy(this->_access_lock);
            this->_access_lock->~t_pt_rwlock();
            cma_free(this->_access_lock);
            this->_access_lock = ft_nullptr;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->set_error(error_code));
        }
        section_index += 1;
    }
    this->_dirty = FT_FALSE;
    this->_generation_protected = FT_FALSE;
    this->clear_generation_metadata_locked();
    this->_biome_id = 0U;
    this->_player_overrides = ft_nullptr;
    this->_player_override_count = 0U;
    this->_player_override_capacity = 0U;
    this->_revision = 0U;
    this->_last_request_valid = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::initialize(const game_voxel_chunk &other) noexcept
{
    int32_t error_code;
    uint8_t section_index;

    errno_abort_if_uninitialised(other._initialised_state,
        "game_voxel_chunk::initialize(copy)");
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = other.get_error();
    error_code = this->destroy();
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(other._access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->initialize();
    section_index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].copy_payload(
            other._sections[section_index]);
        section_index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->grow_player_overrides(other._player_override_count);
    if (error_code == FT_ERR_SUCCESS)
    {
        this->_dirty = other._dirty;
        this->_generation_protected = other._generation_protected;
        this->_generation_metadata = other._generation_metadata;
        this->_biome_id = other._biome_id;
        this->_player_override_count = other._player_override_count;
        if (this->_player_override_count != 0U)
            ft_memcpy(this->_player_overrides,
                other._player_overrides + other.player_override_first(),
                sizeof(game_voxel_block_override) * this->_player_override_count);
        this->rebuild_player_override_index();
        this->_revision = other._revision;
        this->_last_request_valid = other._last_request_valid;
        this->_last_request = other._last_request;
        this->_last_delta = other._last_delta;
    }
    (void)pt_rwlock_strategy_rdunlock(other._access_lock);
    if (error_code != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(error_code));
}

int32_t game_voxel_chunk::destroy_locked() noexcept
{
    uint8_t section_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        (void)this->_sections[section_index].destroy();
        section_index += 1;
    }
    this->_dirty = FT_FALSE;
    this->_generation_protected = FT_FALSE;
    this->clear_generation_metadata_locked();
    this->_biome_id = 0U;
    if (this->_player_overrides != ft_nullptr)
        cma_free(this->_player_overrides);
    this->_player_overrides = ft_nullptr;
    this->_player_override_count = 0U;
    this->_player_override_capacity = 0U;
    this->_revision = 0U;
    this->_last_request_valid = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->destroy_locked();
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    if (error_code == FT_ERR_SUCCESS)
    {
        (void)pt_rwlock_strategy_destroy(this->_access_lock);
        this->_access_lock->~t_pt_rwlock();
        cma_free(this->_access_lock);
        this->_access_lock = ft_nullptr;
    }
    return (this->set_error(error_code));
}

int32_t game_voxel_chunk::move_payload_locked(game_voxel_chunk &other) noexcept
{
    uint8_t section_index;
    int32_t error_code;

    if (other._initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].move(
            other._sections[section_index]);
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
        section_index += 1;
    }
    this->_dirty = other._dirty;
    this->_generation_protected = other._generation_protected;
    this->_generation_metadata = other._generation_metadata;
    this->_biome_id = other._biome_id;
    if (this->_player_overrides != ft_nullptr)
        cma_free(this->_player_overrides);
    this->_player_overrides = other._player_overrides;
    this->_player_override_count = other._player_override_count;
    this->_player_override_capacity = other._player_override_capacity;
    this->_revision = other._revision;
    this->_last_request_valid = other._last_request_valid;
    this->_last_request = other._last_request;
    this->_last_delta = other._last_delta;
    other.clear_generation_metadata_locked();
    other._generation_protected = FT_FALSE;
    other._dirty = FT_FALSE;
    other._biome_id = 0U;
    other._player_overrides = ft_nullptr;
    other._player_override_count = 0U;
    other._player_override_capacity = 0U;
    other._revision = 0U;
    other._last_request_valid = FT_FALSE;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::move(game_voxel_chunk &other) noexcept
{
    int32_t error_code;
    ft_bool destination_was_initialised;
    t_pt_rwlock *first_lock;
    t_pt_rwlock *second_lock;

    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_voxel_chunk::move", "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            error_code = this->destroy();
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        this->_dirty = FT_FALSE;
        this->_generation_protected = FT_FALSE;
        this->clear_generation_metadata_locked();
        this->_biome_id = 0U;
        this->_player_override_count = 0U;
        this->_player_override_capacity = 0U;
        this->_player_overrides = ft_nullptr;
        this->_revision = 0U;
        this->_last_request_valid = FT_FALSE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    destination_was_initialised = FT_FALSE;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        destination_was_initialised = FT_TRUE;
    else
    {
        error_code = this->initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (this->_access_lock == ft_nullptr || other._access_lock == ft_nullptr)
    {
        if (destination_was_initialised == FT_FALSE)
            (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (std::less<t_pt_rwlock *>()(this->_access_lock,
            other._access_lock) == true)
    {
        first_lock = this->_access_lock;
        second_lock = other._access_lock;
    }
    else
    {
        first_lock = other._access_lock;
        second_lock = this->_access_lock;
    }
    error_code = pt_rwlock_strategy_wrlock(first_lock);
    if (error_code != FT_ERR_SUCCESS)
    {
        if (destination_was_initialised == FT_FALSE)
            (void)this->destroy();
        return (this->set_error(error_code));
    }
    error_code = pt_rwlock_strategy_wrlock(second_lock);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)pt_rwlock_strategy_wrunlock(first_lock);
        if (destination_was_initialised == FT_FALSE)
            (void)this->destroy();
        return (this->set_error(error_code));
    }
    if (other._access_lock->waiting_readers != 0
        || other._access_lock->waiting_writers != 0)
    {
        (void)pt_rwlock_strategy_wrunlock(second_lock);
        (void)pt_rwlock_strategy_wrunlock(first_lock);
        if (destination_was_initialised == FT_FALSE)
            (void)this->destroy();
        return (this->set_error(FT_ERR_THREAD_BUSY));
    }
    error_code = this->move_payload_locked(other);
    (void)pt_rwlock_strategy_wrunlock(second_lock);
    (void)pt_rwlock_strategy_wrunlock(first_lock);
    return (this->set_error(error_code));
}

int32_t game_voxel_chunk::read_block_locked(int32_t local_x, int32_t local_y,
    int32_t local_z, uint32_t *block_id) const noexcept
{
    uint8_t section_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::read_block");
    if (block_id == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (game_voxel_chunk::set_error(FT_ERR_OUT_OF_RANGE));
    section_index = static_cast<uint8_t>(local_y >> 4);
    *block_id = this->_sections[section_index].get_block(
        game_voxel_chunk::local_index(local_x, local_y, local_z));
    return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::write_block(int32_t local_x, int32_t local_y,
    int32_t local_z, uint32_t block_id) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_block");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->apply_block_edit_locked(local_x, local_y, local_z, block_id);
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

static constexpr uint32_t GAME_VOXEL_NO_OVERRIDE = UINT32_MAX;

game_voxel_chunk::block_edit_state game_voxel_chunk::inspect_block_edit(
    uint16_t chunk_index, uint32_t current_block_id) const noexcept
{
    uint32_t override_index;
    if (this->_player_override_count != 0U
        && this->find_player_override(chunk_index, &override_index) == FT_ERR_SUCCESS)
        return (block_edit_state{current_block_id,
            this->_player_overrides[override_index].generated_block_id, override_index});
    return (block_edit_state{current_block_id, current_block_id, GAME_VOXEL_NO_OVERRIDE});
}

int32_t game_voxel_chunk::apply_block_edit_locked(int32_t local_x, int32_t local_y,
    int32_t local_z, uint32_t block_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_block_edit");
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    const uint16_t chunk_index = static_cast<uint16_t>(local_x + (local_z << 4) + (local_y << 8));
    const uint32_t current_block_id = this->_sections[chunk_index / GAME_VOXEL_SECTION_BLOCKS]
        .get_block(chunk_index % GAME_VOXEL_SECTION_BLOCKS);
    const block_edit_state state = this->inspect_block_edit(chunk_index, current_block_id);
    return (this->apply_block_edit_at(chunk_index, block_id, state));
}

int32_t game_voxel_chunk::apply_block_edit_at(uint16_t chunk_index, uint32_t block_id,
    const block_edit_state &state) noexcept
{
    if (state.current_block_id == block_id)
    {
        this->_generation_protected = FT_TRUE;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    if (this->_revision == UINT64_MAX)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    const ft_bool has_override = state.override_index != GAME_VOXEL_NO_OVERRIDE;
    const ft_bool remains_modified = block_id != state.generated_block_id;
    if (has_override == FT_FALSE && remains_modified == FT_TRUE)
    {
        const int32_t error_code = this->grow_player_overrides(
            this->_player_override_count + 1U);
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    const int32_t error_code = this->_sections[chunk_index / GAME_VOXEL_SECTION_BLOCKS]
        .set_block(chunk_index % GAME_VOXEL_SECTION_BLOCKS, block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (remains_modified == FT_FALSE)
    {
        if (has_override == FT_TRUE)
            this->remove_player_override(state.override_index);
    }
    else if (has_override == FT_FALSE)
    {
        const uint32_t index = this->player_override_first() + this->_player_override_count;
        this->_player_overrides[index] = {chunk_index, state.generated_block_id, block_id};
        this->index_player_override(index);
        this->_player_override_count += 1U;
    }
    else
    {
        this->_player_overrides[state.override_index].current_block_id = block_id;
    }
    this->_dirty = FT_TRUE;
    this->_generation_protected = FT_TRUE;
    this->_revision += 1U;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::apply_block_edit(int32_t local_x, int32_t local_y,
    int32_t local_z, const game_block_edit_op &edit) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_block_edit");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->apply_block_edit_locked(local_x, local_y, local_z, edit.block_type);
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::apply_authoritative_block_change_locked(
    const game_block_change_request &request,
    game_block_delta *delta_out) noexcept
{
    uint32_t current_block_id;
    uint64_t previous_revision;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_change");
    if (delta_out == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (request.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (request.session_id == 0U || request.request_id == 0U
        || request.world_id == 0U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (this->_last_request_valid != FT_FALSE
        && request.session_id == this->_last_request.session_id
        && request.request_id == this->_last_request.request_id)
    {
        if (request.world_id != this->_last_request.world_id
            || request.chunk_x != this->_last_request.chunk_x
            || request.chunk_z != this->_last_request.chunk_z
            || request.expected_revision
                != this->_last_request.expected_revision
            || request.expected_block_id
                != this->_last_request.expected_block_id
            || request.requested_block_id
                != this->_last_request.requested_block_id
            || request.local_x != this->_last_request.local_x
            || request.local_y != this->_last_request.local_y
            || request.local_z != this->_last_request.local_z)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        *delta_out = this->_last_delta;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    error_code = this->read_block_locked(request.local_x, request.local_y,
        request.local_z, &current_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (request.expected_revision != this->_revision)
        return (this->set_error(FT_ERR_INVALID_STATE));
    if (request.expected_block_id != current_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (request.requested_block_id == current_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    previous_revision = this->_revision;
    const uint16_t chunk_index = static_cast<uint16_t>(request.local_x
        + (request.local_z << 4) + (request.local_y << 8));
    const block_edit_state state = this->inspect_block_edit(chunk_index, current_block_id);
    error_code = this->apply_block_edit_at(chunk_index, request.requested_block_id, state);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    delta_out->protocol_version = request.protocol_version;
    delta_out->session_id = request.session_id;
    delta_out->request_id = request.request_id;
    delta_out->world_id = request.world_id;
    delta_out->chunk_x = request.chunk_x;
    delta_out->chunk_z = request.chunk_z;
    delta_out->previous_revision = previous_revision;
    delta_out->revision = this->_revision;
    delta_out->current_block_id = request.requested_block_id;
    delta_out->player_modified = request.requested_block_id != state.generated_block_id;
    delta_out->server_tick = 0U;
    delta_out->local_x = request.local_x;
    delta_out->local_y = request.local_y;
    delta_out->local_z = request.local_z;
    this->_last_request = request;
    this->_last_delta = *delta_out;
    this->_last_request_valid = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::apply_authoritative_block_change(
    const game_block_change_request &request,
    game_block_delta *delta_out) noexcept
{
    return (this->apply_authoritative_block_change_with_revision(request, delta_out, ft_nullptr));
}

int32_t game_voxel_chunk::apply_authoritative_block_change_with_revision(
    const game_block_change_request &request, game_block_delta *delta_out,
    uint64_t *revision_before) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_change");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (revision_before != ft_nullptr)
        *revision_before = this->_revision;
    error_code = this->apply_authoritative_block_change_locked(request,
        delta_out);
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::apply_authoritative_block_delta_locked(
    const game_block_delta &delta) noexcept
{
    uint32_t current_block_id;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_delta");
    if (delta.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || delta.session_id == 0U
        || delta.request_id == 0U
        || delta.world_id == 0U
        || delta.revision == 0U
        || delta.revision != delta.previous_revision + 1U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (delta.player_modified > 1U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (delta.previous_revision != this->_revision)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = this->read_block_locked(delta.local_x, delta.local_y,
        delta.local_z, &current_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    const uint16_t chunk_index = static_cast<uint16_t>(delta.local_x
        + (delta.local_z << 4) + (delta.local_y << 8));
    const block_edit_state state = this->inspect_block_edit(chunk_index, current_block_id);
    if (delta.player_modified != (delta.current_block_id != state.generated_block_id))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (current_block_id == delta.current_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    error_code = this->apply_block_edit_at(chunk_index, delta.current_block_id, state);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_revision != delta.revision)
        return (this->set_error(FT_ERR_INVALID_STATE));
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::apply_authoritative_block_delta(
    const game_block_delta &delta) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_delta");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->apply_authoritative_block_delta_locked(delta);
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::write_generated_block_locked(int32_t local_x,
    int32_t local_y, int32_t local_z, uint32_t block_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_generated_block");
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    const uint16_t chunk_index = static_cast<uint16_t>(local_x + (local_z << 4) + (local_y << 8));
    return (this->set_error(this->write_generated_block_at(chunk_index, block_id)));
}

int32_t game_voxel_chunk::write_generated_block_at(uint16_t chunk_index, uint32_t block_id) noexcept
{
    const uint32_t section_index = chunk_index / GAME_VOXEL_SECTION_BLOCKS;
    const uint16_t section_block = chunk_index % GAME_VOXEL_SECTION_BLOCKS;
    uint32_t override_index = 0U;
    int32_t error_code = FT_ERR_NOT_FOUND;
    if (this->_player_override_count != 0U)
        error_code = this->find_player_override(chunk_index, &override_index);
    if (error_code == FT_ERR_SUCCESS)
    {
        this->_player_overrides[override_index].generated_block_id = block_id;
        if (this->_player_overrides[override_index].current_block_id != block_id)
        {
            this->_dirty = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }
    }
    else if (error_code != FT_ERR_NOT_FOUND)
        return (error_code);
    const int32_t write_error = this->_sections[section_index].set_block(section_block, block_id);
    if (write_error != FT_ERR_SUCCESS)
        return (write_error);
    if (error_code == FT_ERR_SUCCESS)
        this->remove_player_override(override_index);
    this->_dirty = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t game_voxel_chunk::write_generated_block(int32_t local_x,
    int32_t local_y, int32_t local_z, uint32_t block_id) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_generated_block");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->write_generated_block_locked(local_x, local_y,
        local_z, block_id);
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::write_generated_blocks_if(
    const game_voxel_generated_block *blocks, uint32_t count,
    ft_bool (*accepts)(uint32_t block_id) noexcept) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_generated_blocks_if");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    if (count == 0U)
        return (this->set_error(FT_ERR_SUCCESS));
    if (blocks == ft_nullptr || accepts == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_POINTER));
    int32_t error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    uint32_t index = 0U;
    while (index < count)
    {
        const game_voxel_generated_block &block = blocks[index];
        uint32_t current_block_id;
        error_code = this->read_block_locked(block.local_x, block.local_y,
            block.local_z, &current_block_id);
        if (error_code != FT_ERR_SUCCESS || accepts(current_block_id) == FT_FALSE)
        {
            (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
            return (error_code);
        }
        index += 1U;
    }
    index = 0U;
    while (index < count && error_code == FT_ERR_SUCCESS)
    {
        const game_voxel_generated_block &block = blocks[index];
        error_code = this->write_generated_block_locked(block.local_x,
            block.local_y, block.local_z, block.block_id);
        index += 1U;
    }
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::write_generated_column(int32_t local_x, int32_t local_z,
    const game_voxel_generated_column_block *blocks, uint32_t count) noexcept
{
    return (this->write_generated_column(local_x, local_z, blocks, count, ft_nullptr, ft_nullptr));
}

int32_t game_voxel_chunk::write_generated_column(int32_t local_x, int32_t local_z,
    const game_voxel_generated_column_block *blocks, uint32_t count,
    const game_voxel_generated_column_check *check, ft_bool *applied) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_generated_column");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH
        || local_z < 0 || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (applied != ft_nullptr)
        *applied = FT_FALSE;
    if (count == 0U && check == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    if (count != 0U && blocks == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_POINTER));
    if (check != ft_nullptr && (check->accepts == ft_nullptr || check->first_y < 0
        || check->last_y >= GAME_VOXEL_CHUNK_HEIGHT || check->last_y < check->first_y))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    uint32_t index = 0U;
    while (index < count)
    {
        if (blocks[index].local_y < 0 || blocks[index].local_y >= GAME_VOXEL_CHUNK_HEIGHT)
            return (this->set_error(FT_ERR_OUT_OF_RANGE));
        index += 1U;
    }
    int32_t error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (check != ft_nullptr)
    {
        int32_t y = check->first_y;
        while (y <= check->last_y)
        {
            uint32_t block_id;
            error_code = this->read_block_locked(local_x, y, local_z, &block_id);
            if (error_code != FT_ERR_SUCCESS
                || check->accepts(y, block_id, check->user_data) == FT_FALSE)
            {
                (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
                return (this->set_error(error_code));
            }
            y += 1;
        }
    }
    const uint16_t column_index = static_cast<uint16_t>(local_x + (local_z << 4));
    index = 0U;
    while (index < count && error_code == FT_ERR_SUCCESS)
    {
        const game_voxel_generated_column_block &block = blocks[index];
        if (check != ft_nullptr)
            error_code = this->write_generated_block_locked(local_x, block.local_y, local_z, block.block_id);
        else
        {
            const uint16_t chunk_index = static_cast<uint16_t>(column_index + (block.local_y << 8));
            error_code = this->write_generated_block_at(chunk_index, block.block_id);
        }
        index += 1U;
    }
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    if (applied != ft_nullptr && error_code == FT_ERR_SUCCESS)
        *applied = FT_TRUE;
    return (this->set_error(error_code));
}

int32_t game_voxel_chunk::clear_generated_blocks_locked() noexcept
{
    if (this->_player_override_count == 0U)
    {
        uint8_t section = 0U;
        while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
        {
            (void)this->_sections[section].destroy();
            const int32_t error_code = this->_sections[section].initialize();
            if (error_code != FT_ERR_SUCCESS)
                return (this->set_error(error_code));
            section += 1U;
        }
    }
    else
    {
        uint32_t sections_to_clear = 0U;
        const uint32_t first = this->player_override_first();
        const uint32_t end = first + this->_player_override_count;
        uint32_t override_index = first;
        while (override_index < end)
        {
            sections_to_clear |= 1U << (this->_player_overrides[override_index].local_index >> 12U);
            override_index += 1U;
        }
        uint32_t section = 0U;
        while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
        {
            if (this->_sections[section].is_uniform() == FT_FALSE
                || this->_sections[section].get_uniform_block() != GAME_VOXEL_AIR_BLOCK)
                sections_to_clear |= 1U << section;
            section += 1U;
        }
        constexpr int32_t COLUMN_COUNT = GAME_VOXEL_CHUNK_WIDTH * GAME_VOXEL_CHUNK_DEPTH;
        int32_t column = 0;
        while (column < COLUMN_COUNT)
        {
            const int32_t x = column % GAME_VOXEL_CHUNK_WIDTH;
            const int32_t z = column / GAME_VOXEL_CHUNK_WIDTH;
            section = 0U;
            while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
            {
                // Uniform air without overrides needs no per-block writes.
                if ((sections_to_clear & (1U << section)) == 0U)
                    this->_dirty = FT_TRUE;
                else
                {
                    int32_t y = static_cast<int32_t>(section << 4U);
                    const int32_t end_y = y + 16;
                    while (y < end_y)
                    {
                        const uint16_t chunk_index = static_cast<uint16_t>(x + (z << 4) + (y << 8));
                        const int32_t error_code = this->write_generated_block_at(chunk_index,
                            GAME_VOXEL_AIR_BLOCK);
                        if (error_code != FT_ERR_SUCCESS)
                            return (this->set_error(error_code));
                        y += 1;
                    }
                }
                section += 1U;
            }
            column += 1;
        }
    }
    this->_dirty = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::clear_generated_blocks() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::clear_generated_blocks");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    const int32_t lock_error = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    const int32_t error_code = this->clear_generated_blocks_locked();
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (error_code);
}

ft_bool game_voxel_chunk::is_dirty() const noexcept
{
    ft_bool value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return (FT_FALSE);
    if (pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    value = this->_dirty;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

void game_voxel_chunk::clear_dirty_locked() noexcept
{
    this->_dirty = FT_FALSE;
    return ;
}

void game_voxel_chunk::clear_dirty() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return ;
    if (pt_rwlock_strategy_wrlock(this->_access_lock) != FT_ERR_SUCCESS)
        return ;
    this->clear_dirty_locked();
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return ;
}

void game_voxel_chunk::clear_persistence_dirty() noexcept
{
    this->clear_dirty();
    return ;
}

void game_voxel_chunk::clear_generation_metadata_locked() noexcept
{
    this->_generation_metadata.seed_value = 0U;
    this->_generation_metadata.world_block_origin_x = 0;
    this->_generation_metadata.world_block_origin_z = 0;
    this->_generation_metadata.configuration_signature = 0U;
    this->_generation_metadata.completed_stage_mask = 0U;
    this->_generation_metadata.generator_version = 0U;
    this->_generation_metadata.valid = FT_FALSE;
    return ;
}

void game_voxel_chunk::clear_generation_metadata() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return ;
    if (pt_rwlock_strategy_wrlock(this->_access_lock) != FT_ERR_SUCCESS)
        return ;
    this->clear_generation_metadata_locked();
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return ;
}

ft_bool game_voxel_chunk::is_generation_protected() const noexcept
{
    ft_bool value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return (FT_FALSE);
    if (pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    value = this->_generation_protected;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

    int32_t game_voxel_chunk::set_generation_metadata(
    const game_voxel_generation_metadata &metadata) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::set_generation_metadata");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    int32_t error_code;

    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    this->_generation_metadata = metadata;
    this->_generation_metadata.valid = FT_TRUE;
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (this->set_error(FT_ERR_SUCCESS));
}

const game_voxel_generation_metadata &game_voxel_chunk::get_generation_metadata()
    const noexcept
{
    static thread_local game_voxel_generation_metadata snapshot;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_generation_metadata");
    snapshot = game_voxel_generation_metadata();
    if (this->_access_lock != ft_nullptr
        && pt_rwlock_strategy_rdlock(this->_access_lock) == FT_ERR_SUCCESS)
    {
        snapshot = this->_generation_metadata;
        (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    }
    return (snapshot);
}

ft_bool game_voxel_chunk::has_generation_metadata() const noexcept
{
    ft_bool value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return (FT_FALSE);
    if (pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    value = this->_generation_metadata.valid;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

ft_bool game_voxel_chunk::generation_metadata_matches(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    uint32_t configuration_signature) const noexcept
{
    ft_bool matches;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_access_lock == ft_nullptr)
        return (FT_FALSE);
    if (pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    matches = (this->_generation_metadata.valid != FT_FALSE
        && this->_generation_metadata.seed_value == seed_value
        && this->_generation_metadata.world_block_origin_x
            == world_block_origin_x
        && this->_generation_metadata.world_block_origin_z
            == world_block_origin_z
        && this->_generation_metadata.configuration_signature
            == configuration_signature);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (matches);
}

game_voxel_chunk_section &game_voxel_chunk::get_section(
    uint8_t section_index) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_section");
    if (section_index >= GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk::get_section",
            "section index is out of range");
        return (this->_sections[0]);
    }
    return (this->_sections[section_index]);
}

const game_voxel_chunk_section &game_voxel_chunk::get_section(
    uint8_t section_index) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_section const");
    if (section_index >= GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk::get_section const",
            "section index is out of range");
        return (this->_sections[0]);
    }
    return (this->_sections[section_index]);
}

uint32_t game_voxel_chunk::get_biome_id() const noexcept
{
    uint32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_biome_id");
    if (this->_access_lock == ft_nullptr
        || pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (0U);
    value = this->_biome_id;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

void game_voxel_chunk::set_biome_id(uint32_t biome_id) noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::set_biome_id");
    if (this->_access_lock == ft_nullptr)
        return ;
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return ;
    this->_biome_id = biome_id;
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return ;
}

int32_t game_voxel_chunk::record_dirty_edit(
    const game_block_edit_op &edit) noexcept
{
    int32_t error_code;

    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    (void)edit;
    this->_dirty = FT_TRUE;
    (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
    return (this->set_error(FT_ERR_SUCCESS));
}

ft_bool game_voxel_chunk::is_block_player_modified_locked(int32_t local_x,
    int32_t local_y, int32_t local_z) const noexcept
{
    uint16_t block_index;
    uint32_t override_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::is_block_player_modified");
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (FT_FALSE);
    block_index = static_cast<uint16_t>(local_x + (local_z << 4)
        + (local_y << 8));
    override_index = 0U;
    if (this->find_player_override(block_index, &override_index)
        == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool game_voxel_chunk::is_block_player_modified(int32_t local_x,
    int32_t local_y, int32_t local_z) const noexcept
{
    ft_bool value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::is_block_player_modified");
    if (this->_access_lock == ft_nullptr
        || pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    value = this->is_block_player_modified_locked(local_x, local_y, local_z);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

int32_t game_voxel_chunk::get_generated_block_locked(int32_t local_x,
    int32_t local_y, int32_t local_z, uint32_t *block_id) const noexcept
{
    uint16_t block_index;
    uint32_t override_index;
    uint8_t section_index;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_generated_block");
    if (block_id == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (game_voxel_chunk::set_error(FT_ERR_OUT_OF_RANGE));
    block_index = static_cast<uint16_t>(local_x + (local_z << 4)
        + (local_y << 8));
    override_index = 0U;
    error_code = this->find_player_override(block_index, &override_index);
    if (error_code == FT_ERR_SUCCESS)
    {
        *block_id = this->_player_overrides[override_index]
            .generated_block_id;
        return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
    }
    if (error_code != FT_ERR_NOT_FOUND)
        return (game_voxel_chunk::set_error(error_code));
    section_index = static_cast<uint8_t>(local_y >> 4);
    *block_id = this->_sections[section_index].get_block(
        game_voxel_chunk::local_index(local_x, local_y, local_z));
    return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::copy_blocks_locked(uint32_t *blocks_out,
    uint32_t block_count) const noexcept
{
    return (this->copy_region_locked(blocks_out, block_count, 0U, 0U,
        GAME_VOXEL_CHUNK_WIDTH, GAME_VOXEL_CHUNK_DEPTH));
}

int32_t game_voxel_chunk::copy_x_border_locked(uint32_t *blocks_out,
    uint32_t block_count, uint32_t local_x) const noexcept
{
    const uint32_t expected_count = GAME_VOXEL_CHUNK_HEIGHT * GAME_VOXEL_CHUNK_DEPTH;
    if (blocks_out == ft_nullptr || block_count != expected_count
        || local_x >= GAME_VOXEL_CHUNK_WIDTH)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    uint32_t section = 0U;
    while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        this->_sections[section].copy_rows(blocks_out, local_x, GAME_VOXEL_CHUNK_DEPTH,
            GAME_VOXEL_CHUNK_WIDTH);
        blocks_out += GAME_VOXEL_SECTION_EDGE * GAME_VOXEL_CHUNK_DEPTH;
        ++section;
    }
    return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::copy_region_locked(uint32_t *blocks_out,
    uint32_t block_count, uint32_t first_x, uint32_t first_z,
    uint32_t region_width, uint32_t region_depth) const noexcept
{
    if (blocks_out == ft_nullptr || region_width == 0U || region_depth == 0U
        || first_x >= GAME_VOXEL_CHUNK_WIDTH || first_z >= GAME_VOXEL_CHUNK_DEPTH
        || region_width > GAME_VOXEL_CHUNK_WIDTH - first_x
        || region_depth > GAME_VOXEL_CHUNK_DEPTH - first_z
        || block_count != region_width * region_depth * GAME_VOXEL_CHUNK_HEIGHT)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    uint32_t local_z = first_z;
    while (local_z < first_z + region_depth)
    {
        const uint32_t row_start = first_x + local_z * GAME_VOXEL_CHUNK_WIDTH;
        uint32_t section = 0U;
        while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
        {
            this->_sections[section].copy_rows(blocks_out, row_start, region_width, 1U);
            blocks_out += region_width * GAME_VOXEL_SECTION_EDGE;
            ++section;
        }
        ++local_z;
    }
    return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::copy_z_border_locked(uint32_t *blocks_out,
    uint32_t block_count, uint32_t local_z) const noexcept
{
    return (this->copy_region_locked(blocks_out, block_count, 0U, local_z,
        GAME_VOXEL_CHUNK_WIDTH, 1U));
}

int32_t game_voxel_chunk::get_generated_block(int32_t local_x,
    int32_t local_y, int32_t local_z, uint32_t *block_id) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_generated_block");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->get_generated_block_locked(local_x, local_y, local_z,
        block_id);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::read_block(int32_t local_x, int32_t local_y,
    int32_t local_z, uint32_t *block_id) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::read_block");
    if (this->_access_lock == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (game_voxel_chunk::set_error(error_code));
    error_code = this->read_block_locked(local_x, local_y, local_z, block_id);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::copy_blocks(uint32_t *blocks_out,
    uint32_t block_count) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::copy_blocks");
    if (this->_access_lock == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (game_voxel_chunk::set_error(error_code));
    error_code = this->copy_blocks_locked(blocks_out, block_count);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::copy_region(uint32_t *blocks_out,
    uint32_t block_count, uint32_t first_x, uint32_t first_z,
    uint32_t region_width, uint32_t region_depth) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::copy_region");
    if (this->_access_lock == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (game_voxel_chunk::set_error(error_code));
    error_code = this->copy_region_locked(blocks_out, block_count, first_x,
        first_z, region_width, region_depth);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::copy_x_border(uint32_t *blocks_out,
    uint32_t block_count, uint32_t local_x) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::copy_x_border");
    if (this->_access_lock == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (game_voxel_chunk::set_error(error_code));
    error_code = this->copy_x_border_locked(blocks_out, block_count, local_x);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

int32_t game_voxel_chunk::copy_z_border(uint32_t *blocks_out,
    uint32_t block_count, uint32_t local_z) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::copy_z_border");
    if (this->_access_lock == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (game_voxel_chunk::set_error(error_code));
    error_code = this->copy_z_border_locked(blocks_out, block_count, local_z);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (error_code);
}

uint32_t game_voxel_chunk::get_player_override_count() const noexcept
{
    uint32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_player_override_count");
    if (this->_access_lock == ft_nullptr
        || pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (0U);
    value = this->_player_override_count;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

int32_t game_voxel_chunk::get_player_override(uint32_t index,
    game_voxel_block_override *override_out) const noexcept
{
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_player_override");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    if (override_out == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    error_code = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (index >= this->_player_override_count)
        error_code = FT_ERR_OUT_OF_RANGE;
    else
    {
        *override_out = this->_player_overrides[this->player_override_first() + index];
        error_code = FT_ERR_SUCCESS;
    }
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (this->set_error(error_code));
}

uint64_t game_voxel_chunk::get_revision() const noexcept
{
    uint64_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_revision");
    if (this->_access_lock == ft_nullptr
        || pt_rwlock_strategy_rdlock(this->_access_lock) != FT_ERR_SUCCESS)
        return (0U);
    value = this->_revision;
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (value);
}

int32_t game_voxel_chunk::serialize(ft_byte_buffer &buffer) const noexcept
{
    ft_byte_buffer temporary_buffer;
    int32_t error_code;

    errno_abort_if_uninitialised(this->_initialised_state,
        "game_voxel_chunk::serialize");
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = temporary_buffer.initialize(buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->serialize_locked(temporary_buffer);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary_buffer.destroy();
        return (error_code);
    }
    error_code = buffer.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.move(temporary_buffer);
    if (error_code != FT_ERR_SUCCESS)
        (void)temporary_buffer.destroy();
    return (error_code);
}

int32_t game_voxel_chunk::serialize_locked(ft_byte_buffer &buffer) const noexcept
{
    if (this->_access_lock == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_STATE));
    const int32_t lock_error = pt_rwlock_strategy_rdlock(this->_access_lock);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->serialize_internal(buffer);
    (void)pt_rwlock_strategy_rdunlock(this->_access_lock);
    return (status);
}

int32_t game_voxel_chunk::serialize_internal(
    ft_byte_buffer &buffer) const noexcept
{
    uint8_t section_index;
    int32_t error_code;

    uint8_t header[GAME_VOXEL_CHUNK_PREFIX_BYTES + GAME_VOXEL_CHUNK_METADATA_BYTES];
    buffer_store_le<uint32_t>(header, GAME_VOXEL_CHUNK_MAGIC);
    buffer_store_le<uint32_t>(header + 4U, GAME_VOXEL_CHUNK_VERSION);
    uint8_t *metadata = header + GAME_VOXEL_CHUNK_PREFIX_BYTES;
    metadata[0] = this->_generation_metadata.valid;
    metadata[1] = this->_generation_protected;
    buffer_store_le(metadata + 2U, this->_generation_metadata.seed_value);
    buffer_store_le(metadata + 10U, static_cast<uint32_t>(this->_generation_metadata.world_block_origin_x));
    buffer_store_le(metadata + 14U, static_cast<uint32_t>(this->_generation_metadata.world_block_origin_z));
    buffer_store_le(metadata + 18U, this->_generation_metadata.configuration_signature);
    buffer_store_le(metadata + 22U, this->_generation_metadata.completed_stage_mask);
    buffer_store_le(metadata + 26U, this->_generation_metadata.generator_version);
    error_code = buffer.append(header, sizeof(header));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].serialize(buffer);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        section_index += 1;
    }
    uint8_t footer[16];
    buffer_store_le(footer, this->_biome_id);
    buffer_store_le(footer + 4U, this->_revision);
    buffer_store_le(footer + 12U, this->_player_override_count);
    error_code = buffer.append(footer, sizeof(footer));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (this->_player_override_count == 0U)
        return (FT_ERR_SUCCESS);
    return (game_voxel_serialize_overrides(buffer, this->_player_overrides + this->player_override_first(),
        this->_player_override_count));
}

int32_t game_voxel_chunk::deserialize(ft_byte_buffer &buffer) noexcept
{
    return (this->deserialize_from(buffer));
}

int32_t game_voxel_chunk::deserialize_view(const uint8_t *data, ft_size_t size) noexcept
{
    game_voxel_chunk_reader reader = {data, size, 0U};
    return (this->deserialize_from(reader));
}

template <typename Reader>
int32_t game_voxel_chunk::deserialize_from(Reader &buffer) noexcept
{
    game_voxel_chunk temporary_chunk;
    int32_t error_code;

    error_code = temporary_chunk.deserialize_internal(buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (this->_access_lock == ft_nullptr)
            return (this->set_error(FT_ERR_INVALID_STATE));
        error_code = pt_rwlock_strategy_wrlock(this->_access_lock);
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
        error_code = this->destroy_locked();
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
            return (this->set_error(error_code));
        }
        error_code = this->move_payload_locked(temporary_chunk);
        (void)pt_rwlock_strategy_wrunlock(this->_access_lock);
        return (this->set_error(error_code));
    }
    error_code = this->move(temporary_chunk);
    return (this->set_error(error_code));
}

template <typename Reader>
int32_t game_voxel_chunk::deserialize_internal(Reader &buffer) noexcept
{
    uint32_t magic;
    uint32_t version;
    uint8_t section_index;
    uint8_t metadata_valid;
    uint8_t generation_protected;
    uint64_t metadata_seed_value;
    uint32_t metadata_origin_x;
    uint32_t metadata_origin_z;
    uint32_t biome_id;
    uint64_t revision;
    uint32_t player_override_count;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t prefix[GAME_VOXEL_CHUNK_PREFIX_BYTES];
    if (buffer.read(prefix, sizeof(prefix)) != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    magic = buffer_load_le<uint32_t>(prefix);
    version = buffer_load_le<uint32_t>(prefix + 4U);
    if (magic != GAME_VOXEL_CHUNK_MAGIC
        || (version != GAME_VOXEL_CHUNK_VERSION
            && version != GAME_VOXEL_CHUNK_LEGACY_VERSION))
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    uint8_t metadata[GAME_VOXEL_CHUNK_METADATA_BYTES];
    if (buffer.read(metadata, sizeof(metadata)) != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    metadata_valid = metadata[0];
    generation_protected = metadata[1];
    metadata_seed_value = buffer_load_le<uint64_t>(metadata + 2U);
    metadata_origin_x = buffer_load_le<uint32_t>(metadata + 10U);
    metadata_origin_z = buffer_load_le<uint32_t>(metadata + 14U);
    this->_generation_metadata.configuration_signature = buffer_load_le<uint32_t>(metadata + 18U);
    this->_generation_metadata.completed_stage_mask = buffer_load_le<uint32_t>(metadata + 22U);
    this->_generation_metadata.generator_version = buffer_load_le<uint32_t>(metadata + 26U);
    if (metadata_valid > 1U)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    if (generation_protected > 1U)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    this->_generation_metadata.valid = metadata_valid;
    this->_generation_protected = generation_protected;
    this->_generation_metadata.seed_value = metadata_seed_value;
    this->_generation_metadata.world_block_origin_x = static_cast<int32_t>(
        metadata_origin_x);
    this->_generation_metadata.world_block_origin_z = static_cast<int32_t>(
        metadata_origin_z);
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].deserialize_from(buffer);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
        section_index += 1;
    }
    uint8_t footer[16];
    ft_size_t count_offset = 4U;
    revision = 0U;
    if (version == GAME_VOXEL_CHUNK_VERSION)
        count_offset = 12U;
    error_code = buffer.read(footer, count_offset + sizeof(uint32_t));
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    biome_id = buffer_load_le<uint32_t>(footer);
    if (version == GAME_VOXEL_CHUNK_VERSION)
        revision = buffer_load_le<uint64_t>(footer + 4U);
    player_override_count = buffer_load_le<uint32_t>(footer + count_offset);
    if (version == GAME_VOXEL_CHUNK_LEGACY_VERSION)
    {
        const ft_size_t journal_bytes = static_cast<ft_size_t>(player_override_count) * 24U;
        if (player_override_count > GAME_VOXEL_CHUNK_LEGACY_MAX_DIRTY_EDITS
            || journal_bytes > buffer.remaining())
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        }
        if (journal_bytes != 0U
            && buffer.set_read_position(buffer.read_position() + journal_bytes) != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_IO));
        }
        /* Version 4 stored a history journal, not authoritative overrides. */
        player_override_count = 0U;
    }
    else if (player_override_count > GAME_VOXEL_CHUNK_WIDTH
        * GAME_VOXEL_CHUNK_DEPTH * GAME_VOXEL_CHUNK_HEIGHT)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    if (player_override_count > 0U)
    {
        if (player_override_count * GAME_VOXEL_OVERRIDE_RECORD_BYTES > buffer.remaining())
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_OUT_OF_RANGE));
        }
        error_code = this->grow_player_overrides(player_override_count);
        if (error_code == FT_ERR_SUCCESS)
            error_code = game_voxel_deserialize_overrides(buffer, this->_player_overrides,
                player_override_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
    }
    this->_biome_id = biome_id;
    this->_revision = revision;
    this->_player_override_count = player_override_count;
    this->rebuild_player_override_index();
    this->_dirty = FT_FALSE;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_voxel_chunk::get_error");
    return (game_voxel_chunk::_last_error);
}

const char *game_voxel_chunk::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_voxel_chunk::get_error_str");
    return (ft_strerror(game_voxel_chunk::_last_error));
}

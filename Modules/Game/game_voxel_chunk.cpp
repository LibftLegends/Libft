#include "game_voxel_chunk.hpp"
#include "game_world_delta.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno_internal.hpp"
#include "../File/file_utils.hpp"
#include "../Printf/printf.hpp"
#include <cstdio>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#define GAME_VOXEL_CHUNK_MAGIC 0x474D4348U
#define GAME_VOXEL_CHUNK_VERSION 5U
#define GAME_VOXEL_CHUNK_LEGACY_VERSION 4U
#define GAME_VOXEL_CHUNK_LEGACY_MAX_DIRTY_EDITS 1048576U

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
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
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
    if (this->_indices != ft_nullptr)
        cma_free(this->_indices);
    this->_uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    this->_palette = ft_nullptr;
    this->_indices = ft_nullptr;
    this->_palette_size = 0;
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
    other._uniform_block_id = GAME_VOXEL_AIR_BLOCK;
    other._palette = ft_nullptr;
    other._indices = ft_nullptr;
    other._palette_size = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk_section::materialize(
    uint32_t second_block_id) noexcept
{
    uint16_t index;

    this->_palette = static_cast<uint32_t *>(cma_malloc(sizeof(uint32_t) * 256));
    this->_indices = static_cast<uint8_t *>(cma_malloc(
        GAME_VOXEL_SECTION_BLOCKS));
    if (this->_palette == ft_nullptr || this->_indices == ft_nullptr)
    {
        if (this->_palette != ft_nullptr)
            cma_free(this->_palette);
        if (this->_indices != ft_nullptr)
            cma_free(this->_indices);
        this->_palette = ft_nullptr;
        this->_indices = ft_nullptr;
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    this->_palette[0] = this->_uniform_block_id;
    this->_palette_size = 1;
    if (second_block_id != this->_uniform_block_id)
    {
        this->_palette[1] = second_block_id;
        this->_palette_size = 2;
    }
    index = 0;
    while (index < GAME_VOXEL_SECTION_BLOCKS)
    {
        this->_indices[index] = 0;
        index += 1;
    }
    return (this->set_error(FT_ERR_SUCCESS));
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

void game_voxel_chunk_section::collapse_if_uniform() noexcept
{
    uint16_t index;
    uint8_t first_index;

    if (this->_indices == ft_nullptr || this->_palette == ft_nullptr)
        return ;
    first_index = this->_indices[0];
    index = 1;
    while (index < GAME_VOXEL_SECTION_BLOCKS)
    {
        if (this->_indices[index] != first_index)
            return ;
        index += 1;
    }
    this->_uniform_block_id = this->_palette[first_index];
    cma_free(this->_palette);
    cma_free(this->_indices);
    this->_palette = ft_nullptr;
    this->_indices = ft_nullptr;
    this->_palette_size = 0;
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

int32_t game_voxel_chunk_section::set_block(uint16_t local_index,
    uint32_t block_id) noexcept
{
    uint8_t palette_index;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk_section::set_block");
    if (local_index >= GAME_VOXEL_SECTION_BLOCKS)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (this->_indices == ft_nullptr)
    {
        if (block_id == this->_uniform_block_id)
            return (this->set_error(FT_ERR_SUCCESS));
        error_code = this->materialize(block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = this->find_or_add_palette_index(block_id, &palette_index);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    this->_indices[local_index] = palette_index;
    this->collapse_if_uniform();
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
    uint16_t index;
    int32_t error_code;

    if (this->_indices == ft_nullptr)
    {
        error_code = buffer.append_u8(0);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (buffer.append_u32_le(this->_uniform_block_id));
    }
    error_code = buffer.append_u8(1);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u16_le(this->_palette_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0;
    while (index < this->_palette_size)
    {
        error_code = buffer.append_u32_le(this->_palette[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1;
    }
    return (buffer.append(this->_indices, GAME_VOXEL_SECTION_BLOCKS));
}

int32_t game_voxel_chunk_section::deserialize(
    ft_byte_buffer &buffer) noexcept
{
    uint8_t section_type;
    uint16_t index;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&section_type);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(error_code));
    }
    if (section_type == 0)
    {
        error_code = buffer.read_u32_le(&this->_uniform_block_id);
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
    error_code = this->materialize(GAME_VOXEL_AIR_BLOCK);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u16_le(&this->_palette_size);
    if (error_code != FT_ERR_SUCCESS || this->_palette_size == 0
        || this->_palette_size > 256)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    index = 0;
    while (index < this->_palette_size)
    {
        error_code = buffer.read_u32_le(&this->_palette[index]);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
        index += 1;
    }
    error_code = buffer.read(this->_indices, GAME_VOXEL_SECTION_BLOCKS);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(error_code));
    }
    index = 0;
    while (index < GAME_VOXEL_SECTION_BLOCKS)
    {
        if (this->_indices[index] >= this->_palette_size)
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_IO));
        }
        index += 1;
    }
    this->collapse_if_uniform();
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
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_voxel_chunk::grow_player_overrides(
    uint32_t minimum_capacity) noexcept
{
    uint32_t new_capacity;
    game_voxel_block_override *new_buffer;

    if (minimum_capacity <= this->_player_override_capacity)
        return (FT_ERR_SUCCESS);
    if (minimum_capacity > GAME_VOXEL_CHUNK_WIDTH
        * GAME_VOXEL_CHUNK_DEPTH * GAME_VOXEL_CHUNK_HEIGHT)
        return (FT_ERR_FULL);
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
    new_buffer = static_cast<game_voxel_block_override *>(cma_realloc(
        this->_player_overrides,
        sizeof(game_voxel_block_override) * new_capacity));
    if (new_buffer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    this->_player_overrides = new_buffer;
    this->_player_override_capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

int32_t game_voxel_chunk::find_player_override(uint16_t local_index,
    uint32_t *override_index) const noexcept
{
    uint32_t index;

    if (override_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_player_override_count)
    {
        if (this->_player_overrides[index].local_index == local_index)
        {
            *override_index = index;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

void game_voxel_chunk::remove_player_override(uint32_t override_index) noexcept
{
    uint32_t index;

    if (override_index >= this->_player_override_count)
        return ;
    index = override_index;
    while (index + 1U < this->_player_override_count)
    {
        this->_player_overrides[index] = this->_player_overrides[index + 1U];
        index += 1U;
    }
    this->_player_override_count -= 1U;
    return ;
}

game_voxel_chunk::~game_voxel_chunk() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
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

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_voxel_chunk::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        error_code = this->_sections[section_index].initialize();
        if (error_code != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->set_error(error_code));
        }
        section_index += 1;
    }
    this->_dirty = FT_FALSE;
    this->_generation_protected = FT_FALSE;
    this->clear_generation_metadata();
    this->_biome_id = 0U;
    this->_player_overrides = ft_nullptr;
    this->_player_override_count = 0U;
    this->_player_override_capacity = 0U;
    this->_revision = 0U;
    this->_last_request_valid = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::destroy() noexcept
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
    this->clear_generation_metadata();
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

int32_t game_voxel_chunk::move(game_voxel_chunk &other) noexcept
{
    uint8_t section_index;
    int32_t error_code;

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
            (void)this->destroy();
        this->_dirty = FT_FALSE;
        this->_generation_protected = FT_FALSE;
        this->clear_generation_metadata();
        this->_biome_id = 0U;
        this->_player_override_count = 0U;
        this->_player_override_capacity = 0U;
        this->_player_overrides = ft_nullptr;
        this->_revision = 0U;
        this->_last_request_valid = FT_FALSE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    section_index = 0;
    while (section_index < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        (void)this->_sections[section_index].destroy();
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
    this->_player_overrides = other._player_overrides;
    this->_player_override_count = other._player_override_count;
    this->_player_override_capacity = other._player_override_capacity;
    this->_revision = other._revision;
    this->_last_request_valid = other._last_request_valid;
    this->_last_request = other._last_request;
    this->_last_delta = other._last_delta;
    other.clear_generation_metadata();
    other._generation_protected = FT_FALSE;
    other._dirty = FT_FALSE;
    other._biome_id = 0U;
    other._player_overrides = ft_nullptr;
    other._player_override_count = 0U;
    other._player_override_capacity = 0U;
    other._revision = 0U;
    other._last_request_valid = FT_FALSE;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::read_block(int32_t local_x, int32_t local_y,
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
    game_block_edit_op edit;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_block");
    edit.world_x = local_x;
    edit.world_y = local_y;
    edit.world_z = local_z;
    edit.block_type = block_id;
    edit.tick = 0U;
    return (this->apply_block_edit(local_x, local_y, local_z, edit));
}

int32_t game_voxel_chunk::apply_block_edit(int32_t local_x, int32_t local_y,
    int32_t local_z, const game_block_edit_op &edit) noexcept
{
    uint8_t section_index;
    uint16_t block_index;
    uint16_t override_local_index;
    uint32_t override_index;
    uint32_t previous_block_id;
    uint32_t generated_block_id;
    ft_bool has_override;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_block_edit");
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    section_index = static_cast<uint8_t>(local_y >> 4);
    block_index = game_voxel_chunk::local_index(local_x, local_y, local_z);
    override_local_index = static_cast<uint16_t>(local_x
        + (local_z << 4) + (local_y << 8));
    previous_block_id = this->_sections[section_index].get_block(block_index);
    override_index = 0U;
    error_code = this->find_player_override(override_local_index,
        &override_index);
    if (error_code == FT_ERR_SUCCESS)
    {
        has_override = FT_TRUE;
        generated_block_id = this->_player_overrides[override_index]
            .generated_block_id;
    }
    else if (error_code == FT_ERR_NOT_FOUND)
    {
        has_override = FT_FALSE;
        generated_block_id = previous_block_id;
    }
    else
        return (this->set_error(error_code));
    if (previous_block_id == edit.block_type)
    {
        this->_generation_protected = FT_TRUE;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    if (this->_revision == UINT64_MAX)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (has_override == FT_FALSE
        && edit.block_type != generated_block_id)
    {
        error_code = this->grow_player_overrides(
            this->_player_override_count + 1U);
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    error_code = this->_sections[section_index].set_block(block_index,
        edit.block_type);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (edit.block_type == generated_block_id)
    {
        if (has_override == FT_TRUE)
            this->remove_player_override(override_index);
    }
    else if (has_override == FT_FALSE)
    {
        this->_player_overrides[this->_player_override_count].local_index =
            override_local_index;
        this->_player_overrides[this->_player_override_count]
            .generated_block_id = generated_block_id;
        this->_player_overrides[this->_player_override_count]
            .current_block_id = edit.block_type;
        this->_player_override_count += 1U;
    }
    else
    {
        this->_player_overrides[override_index].current_block_id =
            edit.block_type;
    }
    this->_dirty = FT_TRUE;
    this->_generation_protected = FT_TRUE;
    this->_revision += 1U;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::apply_authoritative_block_change(
    const game_block_change_request &request,
    game_block_delta *delta_out) noexcept
{
    game_block_edit_op edit;
    uint32_t current_block_id;
    uint64_t previous_revision;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_change");
    if (delta_out == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (request.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION)
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
    error_code = this->read_block(request.local_x, request.local_y,
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
    edit.world_x = request.local_x;
    edit.world_y = request.local_y;
    edit.world_z = request.local_z;
    edit.block_type = request.requested_block_id;
    edit.tick = 0U;
    error_code = this->apply_block_edit(request.local_x, request.local_y,
        request.local_z, edit);
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
    delta_out->player_modified = this->is_block_player_modified(
        request.local_x, request.local_y, request.local_z);
    delta_out->server_tick = 0U;
    delta_out->local_x = request.local_x;
    delta_out->local_y = request.local_y;
    delta_out->local_z = request.local_z;
    this->_last_request = request;
    this->_last_delta = *delta_out;
    this->_last_request_valid = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::apply_authoritative_block_delta(
    const game_block_delta &delta) noexcept
{
    game_block_edit_op edit;
    uint32_t current_block_id;
    uint32_t generated_block_id;
    uint64_t expected_revision;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::apply_authoritative_block_delta");
    if (delta.protocol_version != GAME_WORLD_DELTA_PROTOCOL_VERSION
        || delta.revision == 0U
        || delta.revision != delta.previous_revision + 1U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (delta.player_modified > 1U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (delta.previous_revision != this->_revision)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = this->read_block(delta.local_x, delta.local_y,
        delta.local_z, &current_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    error_code = this->get_generated_block(delta.local_x, delta.local_y,
        delta.local_z, &generated_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    expected_revision = this->_revision;
    if (delta.player_modified == FT_FALSE
        && delta.current_block_id != generated_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (delta.player_modified == FT_TRUE
        && delta.current_block_id == generated_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (current_block_id == delta.current_block_id)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    edit.world_x = delta.local_x;
    edit.world_y = delta.local_y;
    edit.world_z = delta.local_z;
    edit.block_type = delta.current_block_id;
    edit.tick = 0U;
    error_code = this->apply_block_edit(delta.local_x, delta.local_y,
        delta.local_z, edit);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (expected_revision == UINT64_MAX
        || this->_revision != expected_revision + 1U)
        return (this->set_error(FT_ERR_INVALID_STATE));
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_voxel_chunk::write_generated_block(int32_t local_x,
    int32_t local_y, int32_t local_z, uint32_t block_id) noexcept
{
    uint8_t section_index;
    uint16_t override_local_index;
    uint32_t override_index;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::write_generated_block");
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    section_index = static_cast<uint8_t>(local_y >> 4);
    override_local_index = static_cast<uint16_t>(local_x
        + (local_z << 4) + (local_y << 8));
    override_index = 0U;
    error_code = this->find_player_override(override_local_index,
        &override_index);
    if (error_code != FT_ERR_SUCCESS && error_code != FT_ERR_NOT_FOUND)
        return (this->set_error(error_code));
    if (error_code == FT_ERR_SUCCESS)
    {
        this->_player_overrides[override_index].generated_block_id = block_id;
        if (this->_player_overrides[override_index].current_block_id
            == block_id)
        {
            error_code = this->_sections[section_index].set_block(
                game_voxel_chunk::local_index(local_x, local_y, local_z),
                block_id);
            if (error_code != FT_ERR_SUCCESS)
                return (this->set_error(error_code));
            this->remove_player_override(override_index);
        }
        this->_dirty = FT_TRUE;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    error_code = this->_sections[section_index].set_block(
        game_voxel_chunk::local_index(local_x, local_y, local_z), block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    this->_dirty = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

ft_bool game_voxel_chunk::is_dirty() const noexcept
{
    return (this->_dirty);
}

void game_voxel_chunk::clear_dirty() noexcept
{
    this->clear_persistence_dirty();
    return ;
}

void game_voxel_chunk::clear_persistence_dirty() noexcept
{
    this->_dirty = FT_FALSE;
    return ;
}

void game_voxel_chunk::clear_generation_metadata() noexcept
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

ft_bool game_voxel_chunk::is_generation_protected() const noexcept
{
    return (this->_generation_protected);
}

int32_t game_voxel_chunk::set_generation_metadata(
    const game_voxel_generation_metadata &metadata) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::set_generation_metadata");
    this->_generation_metadata = metadata;
    this->_generation_metadata.valid = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

const game_voxel_generation_metadata &game_voxel_chunk::get_generation_metadata()
    const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_generation_metadata");
    return (this->_generation_metadata);
}

ft_bool game_voxel_chunk::has_generation_metadata() const noexcept
{
    return (this->_generation_metadata.valid);
}

ft_bool game_voxel_chunk::generation_metadata_matches(uint64_t seed_value,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    uint32_t configuration_signature) const noexcept
{
    if (this->_generation_metadata.valid == FT_FALSE)
        return (FT_FALSE);
    if (this->_generation_metadata.seed_value != seed_value
        || this->_generation_metadata.world_block_origin_x
            != world_block_origin_x
        || this->_generation_metadata.world_block_origin_z
            != world_block_origin_z
        || this->_generation_metadata.configuration_signature
            != configuration_signature)
        return (FT_FALSE);
    return (FT_TRUE);
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
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_biome_id");
    return (this->_biome_id);
}

void game_voxel_chunk::set_biome_id(uint32_t biome_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::set_biome_id");
    this->_biome_id = biome_id;
    return ;
}

ft_bool game_voxel_chunk::is_block_player_modified(int32_t local_x,
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

int32_t game_voxel_chunk::get_generated_block(int32_t local_x,
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

uint32_t game_voxel_chunk::get_player_override_count() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_player_override_count");
    return (this->_player_override_count);
}

int32_t game_voxel_chunk::get_player_override(uint32_t index,
    game_voxel_block_override *override_out) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_player_override");
    if (override_out == ft_nullptr)
        return (game_voxel_chunk::set_error(FT_ERR_INVALID_ARGUMENT));
    if (index >= this->_player_override_count)
        return (game_voxel_chunk::set_error(FT_ERR_OUT_OF_RANGE));
    *override_out = this->_player_overrides[index];
    return (game_voxel_chunk::set_error(FT_ERR_SUCCESS));
}

uint64_t game_voxel_chunk::get_revision() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_voxel_chunk::get_revision");
    return (this->_revision);
}

int32_t game_voxel_chunk::serialize(ft_byte_buffer &buffer) const noexcept
{
    ft_byte_buffer temporary_buffer;
    int32_t error_code;

    error_code = temporary_buffer.initialize(buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->serialize_internal(temporary_buffer);
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

int32_t game_voxel_chunk::serialize_internal(
    ft_byte_buffer &buffer) const noexcept
{
    uint8_t section_index;
    uint32_t override_index;
    int32_t error_code;

    error_code = buffer.append_u32_le(GAME_VOXEL_CHUNK_MAGIC);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(GAME_VOXEL_CHUNK_VERSION);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(this->_generation_metadata.valid);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(this->_generation_protected);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u64_le(this->_generation_metadata.seed_value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(static_cast<uint32_t>(
        this->_generation_metadata.world_block_origin_x));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(static_cast<uint32_t>(
        this->_generation_metadata.world_block_origin_z));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(
        this->_generation_metadata.configuration_signature);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(
        this->_generation_metadata.completed_stage_mask);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(this->_generation_metadata.generator_version);
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
    error_code = buffer.append_u32_le(this->_biome_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u64_le(this->_revision);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(this->_player_override_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    override_index = 0U;
    while (override_index < this->_player_override_count)
    {
        error_code = buffer.append_u16_le(this->_player_overrides[
            override_index].local_index);
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.append_u32_le(this->_player_overrides[
                override_index].generated_block_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.append_u32_le(this->_player_overrides[
                override_index].current_block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        override_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_voxel_chunk::deserialize(ft_byte_buffer &buffer) noexcept
{
    game_voxel_chunk temporary_chunk;
    int32_t error_code;

    error_code = temporary_chunk.deserialize_internal(buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    error_code = this->move(temporary_chunk);
    return (this->set_error(error_code));
}

int32_t game_voxel_chunk::deserialize_internal(ft_byte_buffer &buffer) noexcept
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
    uint32_t legacy_dirty_edit_count;
    uint32_t legacy_edit_index;
    uint32_t legacy_coordinate;
    uint64_t legacy_tick;
    uint16_t override_local_index;
    uint32_t override_generated_block_id;
    uint32_t override_current_block_id;
    uint32_t override_index;
    uint32_t duplicate_index;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    error_code = this->initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (buffer.read_u32_le(&magic) != FT_ERR_SUCCESS
        || buffer.read_u32_le(&version) != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    if (magic != GAME_VOXEL_CHUNK_MAGIC
        || (version != GAME_VOXEL_CHUNK_VERSION
            && version != GAME_VOXEL_CHUNK_LEGACY_VERSION))
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    error_code = buffer.read_u8(&metadata_valid);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u8(&generation_protected);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u64_le(&metadata_seed_value);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&metadata_origin_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&metadata_origin_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(
            &this->_generation_metadata.configuration_signature);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(
            &this->_generation_metadata.completed_stage_mask);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(
            &this->_generation_metadata.generator_version);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
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
        error_code = this->_sections[section_index].deserialize(buffer);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
        section_index += 1;
    }
    error_code = buffer.read_u32_le(&biome_id);
    revision = 0U;
    player_override_count = 0U;
    if (error_code == FT_ERR_SUCCESS && version == GAME_VOXEL_CHUNK_VERSION)
        error_code = buffer.read_u64_le(&revision);
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.read_u32_le(&player_override_count);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(FT_ERR_IO));
    }
    if (version == GAME_VOXEL_CHUNK_LEGACY_VERSION)
    {
        legacy_dirty_edit_count = player_override_count;
        if (legacy_dirty_edit_count > GAME_VOXEL_CHUNK_LEGACY_MAX_DIRTY_EDITS
            || static_cast<uint64_t>(legacy_dirty_edit_count) * 24U
                > buffer.remaining())
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        }
        legacy_edit_index = 0U;
        while (legacy_edit_index < legacy_dirty_edit_count)
        {
            error_code = buffer.read_u32_le(&legacy_coordinate);
            if (error_code == FT_ERR_SUCCESS)
                error_code = buffer.read_u32_le(&legacy_coordinate);
            if (error_code == FT_ERR_SUCCESS)
                error_code = buffer.read_u32_le(&legacy_coordinate);
            if (error_code == FT_ERR_SUCCESS)
                error_code = buffer.read_u32_le(&legacy_coordinate);
            if (error_code == FT_ERR_SUCCESS)
                error_code = buffer.read_u64_le(&legacy_tick);
            if (error_code != FT_ERR_SUCCESS)
            {
                (void)this->destroy();
                return (this->set_error(FT_ERR_IO));
            }
            legacy_edit_index += 1U;
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
        error_code = this->grow_player_overrides(player_override_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
    }
    override_index = 0U;
    while (override_index < player_override_count)
    {
        error_code = buffer.read_u16_le(&override_local_index);
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.read_u32_le(&override_generated_block_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = buffer.read_u32_le(&override_current_block_id);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(error_code));
        }
        if (this->find_player_override(override_local_index,
                &duplicate_index) == FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        }
        if (override_generated_block_id == override_current_block_id)
        {
            (void)this->destroy();
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        }
        this->_player_overrides[override_index].local_index =
            override_local_index;
        this->_player_overrides[override_index].generated_block_id =
            override_generated_block_id;
        this->_player_overrides[override_index].current_block_id =
            override_current_block_id;
        override_index += 1U;
    }
    this->_biome_id = biome_id;
    this->_revision = revision;
    this->_player_override_count = player_override_count;
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

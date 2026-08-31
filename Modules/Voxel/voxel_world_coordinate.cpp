#include "voxel_generation.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_internal.hpp"
#include "../Errno/errno.hpp"

voxel_world_chunk_coordinate::voxel_world_chunk_coordinate() noexcept
    : _chunk_x(), _chunk_z(),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

voxel_world_chunk_coordinate::~voxel_world_chunk_coordinate() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_world_chunk_coordinate::initialize() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    error_code = this->_chunk_x.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    error_code = this->_chunk_z.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_chunk_x.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_world_chunk_coordinate::initialize(
    const voxel_world_chunk_coordinate &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    error_code = this->_chunk_x.initialize(other._chunk_x);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    error_code = this->_chunk_z.initialize(other._chunk_z);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_chunk_x.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_world_chunk_coordinate::destroy() noexcept
{
    uint32_t first_error;
    uint32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    error_code = this->_chunk_x.destroy();
    if (error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->_chunk_z.destroy();
    if (error_code != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = error_code;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

uint32_t voxel_world_chunk_coordinate::move(
    voxel_world_chunk_coordinate &other) noexcept
{
    uint32_t error_code;

    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    error_code = this->_chunk_x.move(other._chunk_x);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    error_code = this->_chunk_z.move(other._chunk_z);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_chunk_x.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_world_chunk_coordinate::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_world_chunk_coordinate::set_chunk_coordinates(
    const char *chunk_x, const char *chunk_z) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || chunk_x == ft_nullptr || chunk_z == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_chunk_x.assign(chunk_x);
    if (ft_big_number::get_error() != FT_ERR_SUCCESS)
        return (ft_big_number::get_error());
    this->_chunk_z.assign(chunk_z);
    return (ft_big_number::get_error());
}

const ft_big_number &voxel_world_chunk_coordinate::chunk_x() const noexcept
{
    return (this->_chunk_x);
}

const ft_big_number &voxel_world_chunk_coordinate::chunk_z() const noexcept
{
    return (this->_chunk_z);
}

static uint64_t voxel_world_coordinate_hash_number(
    const ft_big_number &number, uint64_t hash) noexcept
{
    const char *digits;
    ft_size_t index;

    digits = number.c_str();
    index = 0U;
    while (digits[index] != '\0')
    {
        hash ^= static_cast<uint64_t>(
            static_cast<uint8_t>(digits[index]));
        hash *= UINT64_C(1099511628211);
        index += 1U;
    }
    return (hash);
}

uint64_t voxel_world_chunk_coordinate::hash() const noexcept
{
    uint64_t hash_value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    hash_value = UINT64_C(1469598103934665603);
    hash_value = voxel_world_coordinate_hash_number(this->_chunk_x,
        hash_value);
    hash_value = voxel_world_coordinate_hash_number(this->_chunk_z,
        hash_value ^ UINT64_C(0x9E3779B97F4A7C15));
    return (voxel_mix_u64(hash_value));
}

#endif

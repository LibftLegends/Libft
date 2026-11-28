#include "voxel_generation.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_internal.hpp"
#include "../Errno/errno.hpp"

static constexpr uint64_t voxel_hash_coordinate_digits(
    const char *digits, uint64_t hash_value) noexcept
{
    ft_size_t index = 0U;
    while (digits[index] != '\0')
    {
        hash_value ^= static_cast<uint64_t>(static_cast<uint8_t>(digits[index]));
        hash_value *= UINT64_C(1099511628211);
        index += 1U;
    }
    return (hash_value);
}

static constexpr uint64_t voxel_hash_world_coordinate(
    const char *chunk_x, const char *chunk_z) noexcept
{
    const uint64_t hash_x = voxel_hash_coordinate_digits(chunk_x,
        UINT64_C(1469598103934665603));
    const uint64_t hash_z = voxel_hash_coordinate_digits(chunk_z,
        hash_x ^ UINT64_C(0x9E3779B97F4A7C15));
    return (voxel_mix_u64(hash_z));
}

voxel_world_chunk_coordinate::voxel_world_chunk_coordinate() noexcept
    : _chunk_x(), _chunk_z(), _hash(0U),
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
    this->_hash = voxel_hash_world_coordinate("0", "0");
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
    this->_hash = other._hash;
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
    this->_hash = 0U;
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
    this->_hash = other._hash;
    other._hash = 0U;
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
    const int32_t x_error = ft_big_number::get_error();
    if (x_error != FT_ERR_SUCCESS)
        return (x_error);
    this->_chunk_z.assign(chunk_z);
    const int32_t z_error = ft_big_number::get_error();
    this->_hash = voxel_hash_world_coordinate(this->_chunk_x.c_str(), this->_chunk_z.c_str());
    return (z_error);
}

const ft_big_number &voxel_world_chunk_coordinate::chunk_x() const noexcept
{
    return (this->_chunk_x);
}

const ft_big_number &voxel_world_chunk_coordinate::chunk_z() const noexcept
{
    return (this->_chunk_z);
}

uint64_t voxel_world_chunk_coordinate::hash() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (0U);
    return (this->_hash);
}

#endif

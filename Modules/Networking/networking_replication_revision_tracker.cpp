#include "networking_replication_revision_tracker.hpp"

networking_replication_revision_tracker::networking_replication_revision_tracker() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _block_revision(0U),
      _light_revision(0U), _snapshot_generation(0U), _snapshot_ready(FT_FALSE)
{
    return ;
}

networking_replication_revision_tracker::~networking_replication_revision_tracker() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_replication_revision_tracker::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_block_revision = 0U;
    this->_light_revision = 0U;
    this->_snapshot_generation = 0U;
    this->_snapshot_ready = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_revision_tracker::destroy() noexcept
{
    this->_block_revision = 0U;
    this->_light_revision = 0U;
    this->_snapshot_generation = 0U;
    this->_snapshot_ready = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_revision_tracker::move(
    networking_replication_revision_tracker &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_INVALID_STATE);
    this->_block_revision = other._block_revision;
    this->_light_revision = other._light_revision;
    this->_snapshot_generation = other._snapshot_generation;
    this->_snapshot_ready = other._snapshot_ready;
    this->_initialised_state = other._initialised_state;
    other._block_revision = 0U;
    other._light_revision = 0U;
    other._snapshot_generation = 0U;
    other._snapshot_ready = FT_FALSE;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_revision_tracker::accept_snapshot(
    uint64_t block_revision, uint64_t light_revision,
    uint64_t snapshot_generation) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->can_accept_snapshot(block_revision, light_revision,
        snapshot_generation) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_block_revision = block_revision;
    this->_light_revision = light_revision;
    this->_snapshot_generation = snapshot_generation;
    this->_snapshot_ready = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_replication_revision_tracker::can_accept_snapshot(
    uint64_t block_revision, uint64_t light_revision,
    uint64_t snapshot_generation) const noexcept
{
    (void)snapshot_generation;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (block_revision == 0U || light_revision == 0U)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t networking_replication_revision_tracker::accept_block_delta(
    uint64_t base_revision, uint64_t final_revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->can_accept_block_delta(base_revision, final_revision) == FT_FALSE)
    {
        if (this->_snapshot_ready == FT_FALSE
            || base_revision != this->_block_revision)
            return (FT_ERR_INVALID_STATE);
        return (FT_ERR_OUT_OF_RANGE);
    }
    this->_block_revision = final_revision;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_replication_revision_tracker::can_accept_block_delta(
    uint64_t base_revision, uint64_t final_revision) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_snapshot_ready == FT_FALSE
        || base_revision != this->_block_revision
        || final_revision <= base_revision)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t networking_replication_revision_tracker::accept_light_delta(
    uint64_t base_revision, uint64_t final_revision,
    uint64_t source_block_revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->can_accept_light_delta(base_revision, final_revision,
        source_block_revision) == FT_FALSE)
    {
        if (this->_snapshot_ready == FT_FALSE
            || source_block_revision != this->_block_revision
            || base_revision != this->_light_revision)
            return (FT_ERR_INVALID_STATE);
        return (FT_ERR_OUT_OF_RANGE);
    }
    this->_light_revision = final_revision;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_replication_revision_tracker::can_accept_light_delta(
    uint64_t base_revision, uint64_t final_revision,
    uint64_t source_block_revision) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_snapshot_ready == FT_FALSE
        || source_block_revision != this->_block_revision
        || base_revision != this->_light_revision
        || final_revision <= base_revision)
        return (FT_FALSE);
    return (FT_TRUE);
}

uint64_t networking_replication_revision_tracker::get_block_revision() const noexcept
{
    return (this->_block_revision);
}

uint64_t networking_replication_revision_tracker::get_light_revision() const noexcept
{
    return (this->_light_revision);
}

uint64_t networking_replication_revision_tracker::get_snapshot_generation() const noexcept
{
    return (this->_snapshot_generation);
}

ft_bool networking_replication_revision_tracker::is_snapshot_ready() const noexcept
{
    return (this->_snapshot_ready);
}

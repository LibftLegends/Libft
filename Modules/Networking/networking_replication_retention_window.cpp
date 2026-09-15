#include "networking_replication_retention_window.hpp"

networking_replication_retention_window::networking_replication_retention_window() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _maximum_revisions(0U), _retained_count(0U), _oldest_revision(0U),
      _latest_revision(0U), _acknowledged_revision(0U)
{
    return ;
}

networking_replication_retention_window::~networking_replication_retention_window() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_replication_retention_window::initialize(
    uint32_t maximum_revisions) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (maximum_revisions == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_maximum_revisions = maximum_revisions;
    this->_retained_count = 0U;
    this->_oldest_revision = 0U;
    this->_latest_revision = 0U;
    this->_acknowledged_revision = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_retention_window::destroy() noexcept
{
    this->_maximum_revisions = 0U;
    this->_retained_count = 0U;
    this->_oldest_revision = 0U;
    this->_latest_revision = 0U;
    this->_acknowledged_revision = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_retention_window::move(
    networking_replication_retention_window &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_INVALID_STATE);
    this->_maximum_revisions = other._maximum_revisions;
    this->_retained_count = other._retained_count;
    this->_oldest_revision = other._oldest_revision;
    this->_latest_revision = other._latest_revision;
    this->_acknowledged_revision = other._acknowledged_revision;
    this->_initialised_state = other._initialised_state;
    other._maximum_revisions = 0U;
    other._retained_count = 0U;
    other._oldest_revision = 0U;
    other._latest_revision = 0U;
    other._acknowledged_revision = 0U;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_retention_window::append_revision(
    uint64_t revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (revision == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_retained_count > 0U
        && revision != this->_latest_revision + 1U)
        return (FT_ERR_INVALID_STATE);
    if (this->_retained_count == 0U)
        this->_oldest_revision = revision;
    else if (this->_retained_count == this->_maximum_revisions)
        this->_oldest_revision += 1U;
    else
        this->_retained_count += 1U;
    if (this->_retained_count == 0U)
        this->_retained_count = 1U;
    this->_latest_revision = revision;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_retention_window::acknowledge(
    uint64_t revision) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (revision > this->_latest_revision)
        return (FT_ERR_OUT_OF_RANGE);
    if (revision > this->_acknowledged_revision)
        this->_acknowledged_revision = revision;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_replication_retention_window::can_replay_from(
    uint64_t base_revision) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (this->_retained_count == 0U)
        return (base_revision == this->_latest_revision);
    if (base_revision < this->_oldest_revision - 1U
        || base_revision > this->_latest_revision)
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool networking_replication_retention_window::needs_snapshot(
    uint64_t base_revision) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (this->can_replay_from(base_revision) == FT_FALSE);
}

uint64_t networking_replication_retention_window::get_oldest_revision() const noexcept
{
    return (this->_oldest_revision);
}

uint64_t networking_replication_retention_window::get_latest_revision() const noexcept
{
    return (this->_latest_revision);
}

uint64_t networking_replication_retention_window::get_acknowledged_revision() const noexcept
{
    return (this->_acknowledged_revision);
}

uint32_t networking_replication_retention_window::get_retained_count() const noexcept
{
    return (this->_retained_count);
}

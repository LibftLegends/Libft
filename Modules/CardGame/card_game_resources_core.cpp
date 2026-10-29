#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"

card_game_resource_ledger::card_game_resource_ledger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _pool_count(0U),
      _unit_count(0U), _next_unit_id(1U), _pools(), _units()
{
    return ;
}

card_game_resource_ledger::~card_game_resource_ledger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_resource_ledger::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_pool_count = 0U;
    this->_unit_count = 0U;
    this->_next_unit_id = 1U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::destroy() noexcept
{
    this->_pool_count = 0U;
    this->_unit_count = 0U;
    this->_next_unit_id = 1U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::move(
    card_game_resource_ledger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_pool_count = other._pool_count;
    this->_unit_count = other._unit_count;
    this->_next_unit_id = other._next_unit_id;
    ft_memcpy(this->_pools, other._pools, sizeof(this->_pools));
    ft_memcpy(this->_units, other._units, sizeof(this->_units));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (other.destroy());
}

int32_t card_game_resource_ledger::find_pool(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_pool_count)
    {
        if (this->_pools[current_index].owner_id == owner_id
            && this->_pools[current_index].resource_type_id == resource_type_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_resource_ledger::find_unit(uint32_t unit_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || unit_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_unit_count)
    {
        if (this->_units[current_index].unit_id == unit_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

ft_bool card_game_resource_ledger::unit_matches(
    const card_game_resource_unit &unit,
    const card_game_resource_requirement &requirement) const noexcept
{
    if (requirement.resource_type_id != 0U
        && unit.resource_type_id != requirement.resource_type_id)
        return (FT_FALSE);
    if ((unit.tags & requirement.required_tags) != requirement.required_tags)
        return (FT_FALSE);
    if ((unit.tags & requirement.forbidden_tags) != 0U)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t card_game_resource_ledger::rebuild_pool(uint32_t pool_index) noexcept
{
    uint32_t unit_index;
    uint64_t current_amount;
    uint64_t locked_amount;
    uint64_t temporary_amount;

    if (pool_index >= this->_pool_count)
        return (FT_ERR_OUT_OF_RANGE);
    current_amount = 0U;
    locked_amount = 0U;
    temporary_amount = 0U;
    unit_index = 0U;
    while (unit_index < this->_unit_count)
    {
        if (this->_units[unit_index].owner_id
            == this->_pools[pool_index].owner_id
            && this->_units[unit_index].resource_type_id
            == this->_pools[pool_index].resource_type_id)
        {
            current_amount += this->_units[unit_index].amount;
            locked_amount += this->_units[unit_index].locked_amount;
            if (this->_units[unit_index].temporary != FT_FALSE)
                temporary_amount += this->_units[unit_index].amount;
        }
        unit_index += 1U;
    }
    if (current_amount > UINT32_MAX || locked_amount > UINT32_MAX
        || temporary_amount > UINT32_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    this->_pools[pool_index].current_amount =
        static_cast<uint32_t>(current_amount);
    this->_pools[pool_index].locked_amount =
        static_cast<uint32_t>(locked_amount);
    this->_pools[pool_index].temporary_amount =
        static_cast<uint32_t>(temporary_amount);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::register_pool(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t maximum_amount,
    uint32_t *pool_id) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || pool_id == ft_nullptr || resource_type_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_pool(owner_id, resource_type_id, &index) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_pool_count >= FT_CARD_GAME_MAX_RESOURCE_POOLS)
        return (FT_ERR_FULL);
    index = this->_pool_count;
    this->_pools[index].pool_id = index + 1U;
    this->_pools[index].owner_id = owner_id;
    this->_pools[index].resource_type_id = resource_type_id;
    this->_pools[index].maximum_amount = maximum_amount;
    this->_pools[index].current_amount = 0U;
    this->_pools[index].locked_amount = 0U;
    this->_pools[index].temporary_amount = 0U;
    this->_pool_count += 1U;
    *pool_id = index + 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::get_pool(uint32_t owner_id,
    uint32_t resource_type_id, card_game_resource_pool *pool) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || pool == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_pool(owner_id, resource_type_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *pool = this->_pools[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::get_unit(uint32_t unit_id,
    card_game_resource_unit *unit) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || unit == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_unit(unit_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *unit = this->_units[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::set_maximum(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t maximum_amount) noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find_pool(owner_id, resource_type_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (maximum_amount < this->_pools[index].current_amount
        - this->_pools[index].temporary_amount)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_pools[index].maximum_amount = maximum_amount;
    return (FT_ERR_SUCCESS);
}


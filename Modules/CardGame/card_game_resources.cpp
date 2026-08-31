#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"

int32_t card_game_resource_ledger::release_snapshot(
    card_game_resource_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->pools != ft_nullptr)
        cma_free(snapshot->pools);
    if (snapshot->units != ft_nullptr)
        cma_free(snapshot->units);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::get_snapshot(
    card_game_resource_snapshot *snapshot) const noexcept
{
    card_game_resource_pool *pools;
    card_game_resource_unit *units;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_resource_ledger::release_snapshot(snapshot)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    pools = ft_nullptr;
    units = ft_nullptr;
    if (this->_pool_count != 0U)
    {
        pools = static_cast<card_game_resource_pool *>(cma_malloc(
            static_cast<ft_size_t>(this->_pool_count)
                * sizeof(card_game_resource_pool)));
        if (pools == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(pools, this->_pools,
            static_cast<ft_size_t>(this->_pool_count)
                * sizeof(card_game_resource_pool));
    }
    if (this->_unit_count != 0U)
    {
        units = static_cast<card_game_resource_unit *>(cma_malloc(
            static_cast<ft_size_t>(this->_unit_count)
                * sizeof(card_game_resource_unit)));
        if (units == ft_nullptr)
        {
            if (pools != ft_nullptr)
                cma_free(pools);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(units, this->_units,
            static_cast<ft_size_t>(this->_unit_count)
                * sizeof(card_game_resource_unit));
    }
    snapshot->pool_count = this->_pool_count;
    snapshot->unit_count = this->_unit_count;
    snapshot->next_unit_id = this->_next_unit_id;
    snapshot->pools = pools;
    snapshot->units = units;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::clone_snapshot(
    const card_game_resource_snapshot &source,
    card_game_resource_snapshot *destination) noexcept
{
    if (destination == ft_nullptr
        || source.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || source.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || (source.pool_count != 0U && source.pools == ft_nullptr)
        || (source.unit_count != 0U && source.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_resource_ledger::release_snapshot(destination)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    destination->pool_count = source.pool_count;
    destination->unit_count = source.unit_count;
    destination->next_unit_id = source.next_unit_id;
    if (source.pool_count != 0U)
    {
        destination->pools = static_cast<card_game_resource_pool *>(cma_malloc(
            static_cast<ft_size_t>(source.pool_count)
                * sizeof(card_game_resource_pool)));
        if (destination->pools == ft_nullptr)
        {
            (void)card_game_resource_ledger::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->pools, source.pools,
            static_cast<ft_size_t>(source.pool_count)
                * sizeof(card_game_resource_pool));
    }
    if (source.unit_count != 0U)
    {
        destination->units = static_cast<card_game_resource_unit *>(cma_malloc(
            static_cast<ft_size_t>(source.unit_count)
                * sizeof(card_game_resource_unit)));
        if (destination->units == ft_nullptr)
        {
            (void)card_game_resource_ledger::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->units, source.units,
            static_cast<ft_size_t>(source.unit_count)
                * sizeof(card_game_resource_unit));
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::apply_snapshot(
    const card_game_resource_snapshot &snapshot) noexcept
{
    card_game_resource_ledger replacement;
    uint32_t index;
    uint32_t compare_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || snapshot.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || snapshot.next_unit_id == 0U
        || (snapshot.pool_count != 0U && snapshot.pools == ft_nullptr)
        || (snapshot.unit_count != 0U && snapshot.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.pool_count)
    {
        if (snapshot.pools[index].pool_id != index + 1U
            || snapshot.pools[index].resource_type_id == 0U
            || snapshot.pools[index].current_amount
                > snapshot.pools[index].maximum_amount
            || snapshot.pools[index].locked_amount
                > snapshot.pools[index].current_amount
            || snapshot.pools[index].temporary_amount
                > snapshot.pools[index].current_amount)
            return (FT_ERR_INVALID_ARGUMENT);
        compare_index = index + 1U;
        while (compare_index < snapshot.pool_count)
        {
            if (snapshot.pools[index].owner_id
                    == snapshot.pools[compare_index].owner_id
                && snapshot.pools[index].resource_type_id
                    == snapshot.pools[compare_index].resource_type_id)
                return (FT_ERR_INVALID_ARGUMENT);
            compare_index += 1U;
        }
        index += 1U;
    }
    index = 0U;
    while (index < snapshot.unit_count)
    {
        ft_bool matching_pool;

        if (snapshot.units[index].unit_id == 0U
            || snapshot.units[index].locked_amount > snapshot.units[index].amount
            || snapshot.units[index].temporary > FT_TRUE)
            return (FT_ERR_INVALID_ARGUMENT);
        matching_pool = FT_FALSE;
        compare_index = 0U;
        while (compare_index < snapshot.pool_count)
        {
            if (snapshot.units[index].owner_id
                    == snapshot.pools[compare_index].owner_id
                && snapshot.units[index].resource_type_id
                    == snapshot.pools[compare_index].resource_type_id)
            {
                matching_pool = FT_TRUE;
                break ;
            }
            compare_index += 1U;
        }
        if (matching_pool == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        compare_index = index + 1U;
        while (compare_index < snapshot.unit_count)
        {
            if (snapshot.units[index].unit_id
                == snapshot.units[compare_index].unit_id)
                return (FT_ERR_INVALID_ARGUMENT);
            compare_index += 1U;
        }
        index += 1U;
    }
    if (replacement.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    replacement._pool_count = snapshot.pool_count;
    replacement._unit_count = snapshot.unit_count;
    replacement._next_unit_id = snapshot.next_unit_id;
    if (snapshot.pool_count != 0U)
        ft_memcpy(replacement._pools, snapshot.pools,
            static_cast<ft_size_t>(snapshot.pool_count)
                * sizeof(card_game_resource_pool));
    if (snapshot.unit_count != 0U)
        ft_memcpy(replacement._units, snapshot.units,
            static_cast<ft_size_t>(snapshot.unit_count)
                * sizeof(card_game_resource_unit));
    index = 0U;
    while (index < replacement._pool_count)
    {
        if (replacement.rebuild_pool(index) != FT_ERR_SUCCESS
            || replacement._pools[index].current_amount
                != snapshot.pools[index].current_amount
            || replacement._pools[index].locked_amount
                != snapshot.pools[index].locked_amount
            || replacement._pools[index].temporary_amount
                != snapshot.pools[index].temporary_amount)
        {
            (void)replacement.destroy();
            return (FT_ERR_INVALID_ARGUMENT);
        }
        index += 1U;
    }
    this->_pool_count = replacement._pool_count;
    this->_unit_count = replacement._unit_count;
    this->_next_unit_id = replacement._next_unit_id;
    ft_memcpy(this->_pools, replacement._pools, sizeof(this->_pools));
    ft_memcpy(this->_units, replacement._units, sizeof(this->_units));
    (void)replacement.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_resource_ledger::snapshots_equal(
    const card_game_resource_snapshot &first,
    const card_game_resource_snapshot &second) noexcept
{
    if (first.pool_count != second.pool_count
        || first.unit_count != second.unit_count
        || first.next_unit_id != second.next_unit_id
        || (first.pool_count != 0U
            && (first.pools == ft_nullptr || second.pools == ft_nullptr))
        || (first.unit_count != 0U
            && (first.units == ft_nullptr || second.units == ft_nullptr)))
        return (FT_FALSE);
    if (first.pool_count != 0U
        && ft_memcmp(first.pools, second.pools,
            static_cast<ft_size_t>(first.pool_count)
                * sizeof(card_game_resource_pool)) != 0)
        return (FT_FALSE);
    if (first.unit_count != 0U
        && ft_memcmp(first.units, second.units,
            static_cast<ft_size_t>(first.unit_count)
                * sizeof(card_game_resource_unit)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

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

int32_t card_game_resource_ledger::add_units(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount, uint32_t tags,
    uint64_t expiry_epoch, ft_bool temporary, uint32_t *unit_id) noexcept
{
    uint32_t pool_index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || unit_id == ft_nullptr || amount == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_pool(owner_id, resource_type_id, &pool_index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_unit_count >= FT_CARD_GAME_MAX_RESOURCE_UNITS)
        return (FT_ERR_FULL);
    if (this->_next_unit_id == 0U)
        return (FT_ERR_FULL);
    if (this->_pools[pool_index].current_amount > UINT32_MAX - amount)
        return (FT_ERR_FULL);
    if (temporary == FT_FALSE
        && this->_pools[pool_index].current_amount
            - this->_pools[pool_index].temporary_amount + amount
            > this->_pools[pool_index].maximum_amount)
        return (FT_ERR_FULL);
    this->_units[this->_unit_count].unit_id = this->_next_unit_id;
    this->_units[this->_unit_count].owner_id = owner_id;
    this->_units[this->_unit_count].resource_type_id = resource_type_id;
    this->_units[this->_unit_count].amount = amount;
    this->_units[this->_unit_count].tags = tags;
    this->_units[this->_unit_count].expiry_epoch = expiry_epoch;
    this->_units[this->_unit_count].unlock_epoch = 0U;
    this->_units[this->_unit_count].temporary = temporary;
    this->_units[this->_unit_count].locked = FT_FALSE;
    this->_units[this->_unit_count].locked_amount = 0U;
    this->_next_unit_id += 1U;
    this->_unit_count += 1U;
    *unit_id = this->_units[this->_unit_count - 1U].unit_id;
    result = this->rebuild_pool(pool_index);
    if (result != FT_ERR_SUCCESS)
    {
        this->_unit_count -= 1U;
        this->_next_unit_id -= 1U;
        return (result);
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::lock_units(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount) noexcept
{
    uint32_t pool_index;
    uint32_t unit_index;
    uint32_t remaining;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || amount == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_pool(owner_id, resource_type_id, &pool_index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (amount > this->_pools[pool_index].current_amount
        - this->_pools[pool_index].locked_amount)
        return (FT_ERR_FULL);
    remaining = amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id)
        {
            uint32_t available_amount;
            uint32_t locked_now;

            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (available_amount == 0U)
            {
                unit_index += 1U;
                continue ;
            }
            locked_now = available_amount;
            if (locked_now >= remaining)
            {
                locked_now = remaining;
                remaining = 0U;
            }
            else
                remaining -= locked_now;
            this->_units[unit_index].locked_amount += locked_now;
            if (this->_units[unit_index].locked_amount
                == this->_units[unit_index].amount)
                this->_units[unit_index].locked = FT_TRUE;
        }
        unit_index += 1U;
    }
    return (this->rebuild_pool(pool_index));
}

int32_t card_game_resource_ledger::lock_units_until(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount,
    uint64_t unlock_epoch) noexcept
{
    uint32_t pool_index;
    uint32_t unit_index;
    uint32_t remaining;
    uint32_t available_amount;
    uint32_t locked_now;
    uint64_t eligible_amount;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || amount == 0U || unlock_epoch == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find_pool(owner_id, resource_type_id, &pool_index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (amount > this->_pools[pool_index].current_amount
        - this->_pools[pool_index].locked_amount)
        return (FT_ERR_FULL);
    eligible_amount = 0U;
    unit_index = 0U;
    while (unit_index < this->_unit_count)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id
            && (this->_units[unit_index].unlock_epoch == 0U
                || this->_units[unit_index].unlock_epoch == unlock_epoch))
            eligible_amount += this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
        unit_index += 1U;
    }
    if (eligible_amount < amount)
        return (FT_ERR_FULL);
    remaining = amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id)
        {
            if (this->_units[unit_index].unlock_epoch != 0U
                && this->_units[unit_index].unlock_epoch != unlock_epoch)
                {
                    unit_index += 1U;
                    continue ;
                }
            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (available_amount == 0U)
            {
                unit_index += 1U;
                continue ;
            }
            locked_now = available_amount;
            if (locked_now > remaining)
                locked_now = remaining;
            this->_units[unit_index].locked_amount += locked_now;
            this->_units[unit_index].unlock_epoch = unlock_epoch;
            if (this->_units[unit_index].locked_amount
                == this->_units[unit_index].amount)
                this->_units[unit_index].locked = FT_TRUE;
            remaining -= locked_now;
        }
        unit_index += 1U;
    }
    if (remaining != 0U)
        return (FT_ERR_FULL);
    return (this->rebuild_pool(pool_index));
}

int32_t card_game_resource_ledger::refresh(uint32_t epoch) noexcept
{
    uint32_t unit_index;
    uint32_t write_index;
    uint32_t pool_index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    write_index = 0U;
    unit_index = 0U;
    while (unit_index < this->_unit_count)
    {
        if (this->_units[unit_index].expiry_epoch != 0U
            && this->_units[unit_index].expiry_epoch <= epoch)
        {
            unit_index += 1U;
            continue ;
        }
        if (this->_units[unit_index].unlock_epoch != 0U
            && this->_units[unit_index].unlock_epoch <= epoch)
        {
            this->_units[unit_index].locked_amount = 0U;
            this->_units[unit_index].unlock_epoch = 0U;
            this->_units[unit_index].locked = FT_FALSE;
        }
        if (write_index != unit_index)
            this->_units[write_index] = this->_units[unit_index];
        write_index += 1U;
        unit_index += 1U;
    }
    this->_unit_count = write_index;
    pool_index = 0U;
    while (pool_index < this->_pool_count)
    {
        result = this->rebuild_pool(pool_index);
        if (result != FT_ERR_SUCCESS)
            return (result);
        pool_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::create_payment_plan(uint32_t owner_id,
    const card_game_resource_requirement &requirement,
    card_game_payment_plan *plan) const noexcept
{
    return (this->create_payment_plan_excluding(owner_id, requirement,
        ft_nullptr, 0U, plan));
}

int32_t card_game_resource_ledger::create_payment_plan_excluding(
    uint32_t owner_id, const card_game_resource_requirement &requirement,
    const card_game_payment_unit *reserved_units, uint32_t reserved_count,
    card_game_payment_plan *plan) const noexcept
{
    uint32_t unit_index;
    uint32_t remaining;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan == ft_nullptr || requirement.amount == 0U
        || reserved_count > FT_CARD_GAME_MAX_PAYMENT_UNITS
        || (reserved_count != 0U && reserved_units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    plan->count = 0U;
    plan->total_amount = 0U;
    remaining = requirement.amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        uint32_t reserved_amount;
        uint32_t reserved_index;

        reserved_amount = 0U;
        reserved_index = 0U;
        while (reserved_index < reserved_count)
        {
            if (reserved_units[reserved_index].unit_id
                == this->_units[unit_index].unit_id)
            {
                if (reserved_amount > UINT32_MAX
                    - reserved_units[reserved_index].amount)
                    return (FT_ERR_FULL);
                reserved_amount += reserved_units[reserved_index].amount;
            }
            reserved_index += 1U;
        }
        if (this->_units[unit_index].owner_id == owner_id
            && this->unit_matches(this->_units[unit_index], requirement))
        {
            uint32_t available_amount;

            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (reserved_amount >= available_amount)
                available_amount = 0U;
            else
                available_amount -= reserved_amount;
            if (available_amount == 0U)
            {
                unit_index += 1U;
                continue ;
            }
            if (plan->count >= FT_CARD_GAME_MAX_PAYMENT_UNITS)
                return (FT_ERR_FULL);
            plan->units[plan->count].unit_id = this->_units[unit_index].unit_id;
            if (available_amount >= remaining)
            {
                plan->units[plan->count].amount = remaining;
                plan->total_amount += remaining;
                remaining = 0U;
            }
            else
            {
                plan->units[plan->count].amount = available_amount;
                plan->total_amount += available_amount;
                remaining -= available_amount;
            }
            plan->count += 1U;
        }
        unit_index += 1U;
    }
    if (remaining != 0U)
        return (FT_ERR_FULL);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::spend(
    const card_game_payment_plan &plan) noexcept
{
    uint32_t payment_index;
    uint32_t unique_index;
    uint32_t unique_count;
    uint32_t pool_index;
    uint32_t unit_ids[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t unit_indexes[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t unit_usage[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t unit_index;
    uint64_t total_amount;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan.count > FT_CARD_GAME_MAX_PAYMENT_UNITS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (plan.count == 0U)
    {
        if (plan.total_amount != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_SUCCESS);
    }
    unique_count = 0U;
    total_amount = 0U;
    payment_index = 0U;
    while (payment_index < plan.count)
    {
        result = this->find_unit(plan.units[payment_index].unit_id, &unit_index);
        if (result != FT_ERR_SUCCESS || plan.units[payment_index].amount == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (total_amount > UINT32_MAX - plan.units[payment_index].amount)
            return (FT_ERR_INVALID_ARGUMENT);
        total_amount += plan.units[payment_index].amount;
        unique_index = 0U;
        while (unique_index < unique_count
            && unit_ids[unique_index] != plan.units[payment_index].unit_id)
            unique_index += 1U;
        if (unique_index == unique_count)
        {
            unit_ids[unique_count] = plan.units[payment_index].unit_id;
            unit_indexes[unique_count] = unit_index;
            unit_usage[unique_count] = 0U;
            unique_count += 1U;
        }
        if (unit_usage[unique_index] > UINT32_MAX
            - plan.units[payment_index].amount
            || unit_usage[unique_index] + plan.units[payment_index].amount
                > this->_units[unit_indexes[unique_index]].amount
                    - this->_units[unit_indexes[unique_index]].locked_amount)
            return (FT_ERR_INVALID_ARGUMENT);
        unit_usage[unique_index] += plan.units[payment_index].amount;
        payment_index += 1U;
    }
    if (total_amount != plan.total_amount)
        return (FT_ERR_INVALID_ARGUMENT);
    payment_index = 0U;
    while (payment_index < unique_count)
    {
        this->_units[unit_indexes[payment_index]].amount -=
            unit_usage[payment_index];
        payment_index += 1U;
    }
    unit_index = 0U;
    while (unit_index < this->_unit_count)
    {
        if (this->_units[unit_index].amount == 0U)
        {
            this->_units[unit_index] = this->_units[this->_unit_count - 1U];
            this->_unit_count -= 1U;
            continue ;
        }
        unit_index += 1U;
    }
    pool_index = 0U;
    while (pool_index < this->_pool_count)
    {
        result = this->rebuild_pool(pool_index);
        if (result != FT_ERR_SUCCESS)
            return (result);
        pool_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::create_cost_plan(uint32_t owner_id,
    const card_game_cost &cost, uint32_t variable_amount,
    card_game_cost_plan *plan) const noexcept
{
    uint32_t alternative_index;
    uint32_t component_index;
    uint32_t component_count;
    card_game_payment_unit reserved_units[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t reserved_count;
    const card_game_resource_requirement *requirements;
    card_game_cost_plan candidate;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan == ft_nullptr || cost.component_count
            > FT_CARD_GAME_MAX_COST_COMPONENTS
        || cost.alternative_count > FT_CARD_GAME_MAX_COST_ALTERNATIVES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (cost.component_count == 0U && cost.alternative_count == 0U
        && variable_amount != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    alternative_index = UINT32_MAX;
    component_count = cost.component_count;
    requirements = cost.components;
    if (cost.alternative_count != 0U)
    {
        alternative_index = 0U;
        while (alternative_index < cost.alternative_count)
        {
            candidate.selected_alternative = alternative_index;
            candidate.component_count =
                cost.alternative_component_counts[alternative_index];
            if (candidate.component_count <= FT_CARD_GAME_MAX_COST_COMPONENTS)
            {
                component_count = candidate.component_count;
                requirements = cost.alternatives[alternative_index];
                reserved_count = 0U;
                component_index = 0U;
                while (component_index < component_count)
                {
                    card_game_resource_requirement requirement =
                        requirements[component_index];
                    if (component_index == 0U)
                    {
                        if (requirement.amount > UINT32_MAX - variable_amount)
                            return (FT_ERR_INVALID_ARGUMENT);
                        requirement.amount += variable_amount;
                    }
                    result = this->create_payment_plan_excluding(owner_id,
                        requirement, reserved_units, reserved_count,
                        &candidate.components[component_index]);
                    if (result != FT_ERR_SUCCESS)
                        break ;
                    if (reserved_count
                        + candidate.components[component_index].count
                        > FT_CARD_GAME_MAX_PAYMENT_UNITS)
                    {
                        result = FT_ERR_FULL;
                        break ;
                    }
                    {
                        uint32_t excluded_index;

                        excluded_index = 0U;
                        while (excluded_index
                            < candidate.components[component_index].count)
                        {
                            reserved_units[reserved_count].unit_id = candidate
                                .components[component_index]
                                .units[excluded_index].unit_id;
                            reserved_units[reserved_count].amount = candidate
                                .components[component_index]
                                .units[excluded_index].amount;
                            reserved_count += 1U;
                            excluded_index += 1U;
                        }
                    }
                    component_index += 1U;
                }
                if (component_index == component_count)
                    break ;
            }
            alternative_index += 1U;
        }
        if (alternative_index == cost.alternative_count)
            return (FT_ERR_FULL);
    }
    else
    {
        candidate.selected_alternative = UINT32_MAX;
        candidate.component_count = component_count;
        reserved_count = 0U;
        component_index = 0U;
        while (component_index < component_count)
        {
            card_game_resource_requirement requirement =
                requirements[component_index];
            if (component_index == 0U)
            {
                if (requirement.amount > UINT32_MAX - variable_amount)
                    return (FT_ERR_INVALID_ARGUMENT);
                requirement.amount += variable_amount;
            }
            result = this->create_payment_plan_excluding(owner_id, requirement,
                reserved_units, reserved_count,
                &candidate.components[component_index]);
            if (result != FT_ERR_SUCCESS)
                return (result);
            if (reserved_count + candidate.components[component_index].count
                > FT_CARD_GAME_MAX_PAYMENT_UNITS)
                return (FT_ERR_FULL);
            {
                uint32_t excluded_index;

                excluded_index = 0U;
                while (excluded_index
                    < candidate.components[component_index].count)
                {
                    reserved_units[reserved_count].unit_id = candidate
                        .components[component_index]
                        .units[excluded_index].unit_id;
                    reserved_units[reserved_count].amount = candidate
                        .components[component_index]
                        .units[excluded_index].amount;
                    reserved_count += 1U;
                    excluded_index += 1U;
                }
            }
            component_index += 1U;
        }
    }
    candidate.combined.count = 0U;
    candidate.combined.total_amount = 0U;
    component_index = 0U;
    while (component_index < candidate.component_count)
    {
        uint32_t payment_index;

        payment_index = 0U;
        while (payment_index < candidate.components[component_index].count)
        {
            if (candidate.combined.count >= FT_CARD_GAME_MAX_PAYMENT_UNITS
                || candidate.combined.total_amount > UINT32_MAX
                    - candidate.components[component_index].units[payment_index]
                        .amount)
                return (FT_ERR_FULL);
            candidate.combined.units[candidate.combined.count] =
                candidate.components[component_index].units[payment_index];
            candidate.combined.count += 1U;
            candidate.combined.total_amount +=
                candidate.components[component_index].units[payment_index].amount;
            payment_index += 1U;
        }
        component_index += 1U;
    }
    *plan = candidate;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::spend_cost(
    const card_game_cost_plan &plan) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan.component_count > FT_CARD_GAME_MAX_COST_COMPONENTS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->spend(plan.combined));
}

uint32_t card_game_resource_ledger::pool_count() const noexcept
{
    return (this->_pool_count);
}

uint32_t card_game_resource_ledger::unit_count() const noexcept
{
    return (this->_unit_count);
}

int32_t card_game_allowance_ledger::release_snapshot(
    card_game_allowance_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (snapshot->allowances != ft_nullptr)
        cma_free(snapshot->allowances);
    ft_bzero(snapshot, sizeof(*snapshot));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::get_snapshot(
    card_game_allowance_snapshot *snapshot) const noexcept
{
    card_game_action_allowance *allowances;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_allowance_ledger::release_snapshot(snapshot)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    allowances = ft_nullptr;
    if (this->_count != 0U)
    {
        allowances = static_cast<card_game_action_allowance *>(cma_malloc(
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_action_allowance)));
        if (allowances == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(allowances, this->_allowances,
            static_cast<ft_size_t>(this->_count)
                * sizeof(card_game_action_allowance));
    }
    snapshot->count = this->_count;
    snapshot->next_id = this->_next_id;
    snapshot->allowances = allowances;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::clone_snapshot(
    const card_game_allowance_snapshot &source,
    card_game_allowance_snapshot *destination) noexcept
{
    if (destination == ft_nullptr
        || source.count > FT_CARD_GAME_MAX_ALLOWANCES
        || source.next_id == 0U
        || (source.count != 0U && source.allowances == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (card_game_allowance_ledger::release_snapshot(destination)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    destination->count = source.count;
    destination->next_id = source.next_id;
    if (source.count != 0U)
    {
        destination->allowances =
            static_cast<card_game_action_allowance *>(cma_malloc(
                static_cast<ft_size_t>(source.count)
                    * sizeof(card_game_action_allowance)));
        if (destination->allowances == ft_nullptr)
        {
            (void)card_game_allowance_ledger::release_snapshot(destination);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(destination->allowances, source.allowances,
            static_cast<ft_size_t>(source.count)
                * sizeof(card_game_action_allowance));
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::apply_snapshot(
    const card_game_allowance_snapshot &snapshot) noexcept
{
    uint32_t index;
    uint32_t compare_index;
    uint32_t predicate_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.count > FT_CARD_GAME_MAX_ALLOWANCES
        || snapshot.next_id == 0U
        || (snapshot.count != 0U && snapshot.allowances == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.count)
    {
        if (snapshot.allowances[index].allowance_id == 0U
            || snapshot.allowances[index].maximum_uses == 0U
            || snapshot.allowances[index].remaining_uses
                > snapshot.allowances[index].maximum_uses)
            return (FT_ERR_INVALID_ARGUMENT);
        if (snapshot.allowances[index].predicate_id != 0U
            && this->find_predicate(snapshot.allowances[index].predicate_id,
                &predicate_index) != FT_ERR_SUCCESS)
            return (FT_ERR_NOT_FOUND);
        compare_index = index + 1U;
        while (compare_index < snapshot.count)
        {
            if (snapshot.allowances[index].allowance_id
                == snapshot.allowances[compare_index].allowance_id)
                return (FT_ERR_INVALID_ARGUMENT);
            compare_index += 1U;
        }
        index += 1U;
    }
    ft_memcpy(this->_allowances, snapshot.allowances,
        static_cast<ft_size_t>(snapshot.count)
            * sizeof(card_game_action_allowance));
    this->_count = snapshot.count;
    this->_next_id = snapshot.next_id;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_allowance_ledger::snapshots_equal(
    const card_game_allowance_snapshot &first,
    const card_game_allowance_snapshot &second) noexcept
{
    if (first.count != second.count || first.next_id != second.next_id
        || (first.count != 0U
            && (first.allowances == ft_nullptr
                || second.allowances == ft_nullptr)))
        return (FT_FALSE);
    if (first.count != 0U
        && ft_memcmp(first.allowances, second.allowances,
            static_cast<ft_size_t>(first.count)
                * sizeof(card_game_action_allowance)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

card_game_allowance_ledger::card_game_allowance_ledger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _count(0U),
      _next_id(1U), _allowances(), _predicate_count(0U), _predicates()
{
    return ;
}

card_game_allowance_ledger::~card_game_allowance_ledger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_allowance_ledger::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_count = 0U;
    this->_next_id = 1U;
    this->_predicate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::destroy() noexcept
{
    this->_count = 0U;
    this->_next_id = 1U;
    this->_predicate_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::move(
    card_game_allowance_ledger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_count = other._count;
    this->_next_id = other._next_id;
    this->_predicate_count = other._predicate_count;
    ft_memcpy(this->_allowances, other._allowances,
        sizeof(this->_allowances));
    ft_memcpy(this->_predicates, other._predicates,
        sizeof(this->_predicates));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (other.destroy());
}

int32_t card_game_allowance_ledger::find(uint32_t allowance_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || allowance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_count)
    {
        if (this->_allowances[current_index].allowance_id == allowance_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_allowance_ledger::find_predicate(uint32_t predicate_id,
    uint32_t *index) const noexcept
{
    uint32_t current_index;

    if (index == ft_nullptr || predicate_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    current_index = 0U;
    while (current_index < this->_predicate_count)
    {
        if (this->_predicates[current_index].predicate_id == predicate_id)
        {
            *index = current_index;
            return (FT_ERR_SUCCESS);
        }
        current_index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

ft_bool card_game_allowance_ledger::matches(
    const card_game_action_allowance &allowance, uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch) const noexcept
{
    if (allowance.owner_id != owner_id || allowance.action_id != action_id
        || allowance.remaining_uses == 0U
        || (allowance.action_tags & action_tags) != allowance.action_tags)
        return (FT_FALSE);
    if (allowance.expiry_epoch != 0U && allowance.expiry_epoch <= epoch)
        return (FT_FALSE);
    if (allowance.predicate_id != 0U)
    {
        uint32_t predicate_index;

        if (this->find_predicate(allowance.predicate_id, &predicate_index)
            != FT_ERR_SUCCESS)
            return (FT_FALSE);
        if (this->_predicates[predicate_index].predicate(action_id, action_tags,
            this->_predicates[predicate_index].user_data) == FT_FALSE)
            return (FT_FALSE);
    }
    return (FT_TRUE);
}

int32_t card_game_allowance_ledger::register_predicate(
    uint32_t predicate_id, card_game_allowance_predicate predicate,
    void *user_data) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || predicate_id == 0U || predicate == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_predicate(predicate_id, &index) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_predicate_count >= FT_CARD_GAME_MAX_ALLOWANCE_PREDICATES)
        return (FT_ERR_FULL);
    index = this->_predicate_count;
    this->_predicates[index].predicate_id = predicate_id;
    this->_predicates[index].predicate = predicate;
    this->_predicates[index].user_data = user_data;
    this->_predicate_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::grant(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint32_t uses,
    uint64_t expiry_epoch, uint32_t source_instance, uint32_t source_effect_id,
    uint32_t predicate_id, uint32_t predicate_context_id,
    uint32_t *allowance_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance_id == ft_nullptr || action_id == 0U || uses == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_count >= FT_CARD_GAME_MAX_ALLOWANCES)
        return (FT_ERR_FULL);
    if (predicate_id != 0U)
    {
        uint32_t predicate_index;

        if (this->find_predicate(predicate_id, &predicate_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_NOT_FOUND);
    }
    this->_allowances[this->_count].allowance_id = this->_next_id;
    this->_allowances[this->_count].owner_id = owner_id;
    this->_allowances[this->_count].action_id = action_id;
    this->_allowances[this->_count].action_tags = action_tags;
    this->_allowances[this->_count].remaining_uses = uses;
    this->_allowances[this->_count].maximum_uses = uses;
    this->_allowances[this->_count].expiry_epoch = expiry_epoch;
    this->_allowances[this->_count].source_instance = source_instance;
    this->_allowances[this->_count].source_effect_id = source_effect_id;
    this->_allowances[this->_count].predicate_id = predicate_id;
    this->_allowances[this->_count].predicate_context_id = predicate_context_id;
    *allowance_id = this->_next_id;
    this->_next_id += 1U;
    this->_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::get(uint32_t allowance_id,
    card_game_action_allowance *allowance) const noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->find(allowance_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *allowance = this->_allowances[index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::count_eligible(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch,
    uint32_t *count) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = 0U;
    index = 0U;
    while (index < this->_count)
    {
        if (this->matches(this->_allowances[index], owner_id, action_id,
            action_tags, epoch) != FT_FALSE)
            *count += 1U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::consume(uint32_t allowance_id,
    uint32_t owner_id, uint32_t action_id, uint32_t action_tags,
    uint64_t epoch) noexcept
{
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    result = this->find(allowance_id, &index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->matches(this->_allowances[index], owner_id, action_id,
        action_tags, epoch) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    this->_allowances[index].remaining_uses -= 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_allowance_ledger::consume_first(uint32_t owner_id,
    uint32_t action_id, uint32_t action_tags, uint64_t epoch,
    uint32_t *allowance_id) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || allowance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_count)
    {
        if (this->matches(this->_allowances[index], owner_id, action_id,
            action_tags, epoch) != FT_FALSE)
        {
            this->_allowances[index].remaining_uses -= 1U;
            *allowance_id = this->_allowances[index].allowance_id;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_PERMISSION_DENIED);
}

int32_t card_game_allowance_ledger::reset_epoch(uint64_t epoch) noexcept
{
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < this->_count)
    {
        if (this->_allowances[index].expiry_epoch != 0U
            && this->_allowances[index].expiry_epoch <= epoch)
        {
            this->_allowances[index] = this->_allowances[this->_count - 1U];
            this->_count -= 1U;
            continue ;
        }
        this->_allowances[index].remaining_uses =
            this->_allowances[index].maximum_uses;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

uint32_t card_game_allowance_ledger::size() const noexcept
{
    return (this->_count);
}

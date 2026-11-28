#include "card_game_resources.hpp"
#include <algorithm>

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
    this->_pools[pool_index].current_amount += amount;
    if (temporary != FT_FALSE)
        this->_pools[pool_index].temporary_amount += amount;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::lock_units(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount) noexcept
{
    return (this->lock_units_internal(owner_id, resource_type_id, amount, 0U));
}

int32_t card_game_resource_ledger::lock_units_until(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount,
    uint64_t unlock_epoch) noexcept
{
    if (unlock_epoch == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->lock_units_internal(owner_id, resource_type_id, amount,
        unlock_epoch));
}

int32_t card_game_resource_ledger::lock_units_internal(uint32_t owner_id,
    uint32_t resource_type_id, uint32_t amount,
    uint64_t unlock_epoch) noexcept
{
    uint32_t pool_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED || amount == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t result = this->find_pool(owner_id, resource_type_id, &pool_index);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card_game_resource_pool &pool = this->_pools[pool_index];
    if (amount > pool.current_amount - pool.locked_amount)
        return (FT_ERR_FULL);
    uint32_t required_splits = 0U;
    uint32_t remaining = amount;
    uint32_t unit_index = 0U;
    const uint32_t initial_unit_count = this->_unit_count;
    while (unit_index < initial_unit_count && remaining != 0U)
    {
        const card_game_resource_unit &unit = this->_units[unit_index];
        if (unit.owner_id == owner_id && unit.resource_type_id == resource_type_id)
        {
            const uint32_t available = unit.amount - unit.locked_amount;
            if (available != 0U && unit.locked_amount != 0U
                && unit.unlock_epoch != unlock_epoch)
                required_splits += 1U;
            remaining -= std::min(available, remaining);
        }
        unit_index += 1U;
    }
    if (remaining != 0U || required_splits > FT_CARD_GAME_MAX_RESOURCE_UNITS
            - initial_unit_count
        || (required_splits != 0U && (this->_next_unit_id == 0U
            || required_splits > UINT32_MAX - this->_next_unit_id)))
        return (FT_ERR_FULL);
    remaining = amount;
    unit_index = 0U;
    while (unit_index < initial_unit_count && remaining != 0U)
    {
        card_game_resource_unit &unit = this->_units[unit_index];
        if (unit.owner_id == owner_id && unit.resource_type_id == resource_type_id
            && unit.amount != unit.locked_amount)
        {
            const uint32_t locked_now = std::min(unit.amount - unit.locked_amount,
                remaining);
            if (unit.locked_amount != 0U && unit.unlock_epoch != unlock_epoch)
                this->split_unit_lock(unit_index, locked_now, unlock_epoch);
            else
            {
                unit.locked_amount += locked_now;
                if (unlock_epoch != 0U)
                    unit.unlock_epoch = unlock_epoch;
                if (unit.locked_amount == unit.amount)
                    unit.locked = FT_TRUE;
            }
            remaining -= locked_now;
        }
        unit_index += 1U;
    }
    pool.locked_amount += amount;
    return (FT_ERR_SUCCESS);
}
void card_game_resource_ledger::split_unit_lock(uint32_t unit_index,
    uint32_t amount, uint64_t unlock_epoch) noexcept
{
    card_game_resource_unit *source_unit;
    card_game_resource_unit *locked_unit;
    uint32_t locked_unit_index;

    source_unit = &this->_units[unit_index];
    locked_unit_index = this->_unit_count;
    locked_unit = &this->_units[locked_unit_index];
    *locked_unit = *source_unit;
    locked_unit->unit_id = this->_next_unit_id;
    locked_unit->amount = amount;
    locked_unit->locked_amount = amount;
    locked_unit->locked = FT_TRUE;
    locked_unit->unlock_epoch = unlock_epoch;
    source_unit->amount -= amount;
    if (source_unit->locked_amount == source_unit->amount)
        source_unit->locked = FT_TRUE;
    else
        source_unit->locked = FT_FALSE;
    this->_next_unit_id += 1U;
    this->_unit_count += 1U;
    return ;
}

int32_t card_game_resource_ledger::refresh(uint32_t epoch) noexcept
{
    uint32_t unit_index;
    uint32_t write_index;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    write_index = 0U;
    unit_index = 0U;
    while (unit_index < this->_unit_count)
    {
        card_game_resource_unit &unit = this->_units[unit_index];
        const ft_bool expires = unit.expiry_epoch != 0U && unit.expiry_epoch <= epoch;
        const ft_bool unlocks = unit.unlock_epoch != 0U && unit.unlock_epoch <= epoch;
        if (expires || unlocks)
        {
            uint32_t pool_index;
            const int32_t result = this->find_pool(unit.owner_id,
                unit.resource_type_id, &pool_index);
            if (result != FT_ERR_SUCCESS)
                return (result);
            card_game_resource_pool &pool = this->_pools[pool_index];
            pool.locked_amount -= unit.locked_amount;
            if (expires)
            {
                pool.current_amount -= unit.amount;
                if (unit.temporary != FT_FALSE)
                    pool.temporary_amount -= unit.amount;
                unit_index += 1U;
                continue ;
            }
            unit.locked_amount = 0U;
            unit.unlock_epoch = 0U;
            unit.locked = FT_FALSE;
        }
        if (write_index != unit_index)
            this->_units[write_index] = this->_units[unit_index];
        write_index += 1U;
        unit_index += 1U;
    }
    this->_unit_count = write_index;
    return (FT_ERR_SUCCESS);
}

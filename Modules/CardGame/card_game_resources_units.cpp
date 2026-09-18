#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"

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
    uint32_t required_splits;
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
    required_splits = 0U;
    remaining = amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id)
        {
            uint32_t available_amount;

            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (available_amount != 0U)
            {
                if (available_amount > remaining)
                    available_amount = remaining;
                if (this->_units[unit_index].locked_amount != 0U
                    && this->_units[unit_index].unlock_epoch != 0U)
                    required_splits += 1U;
                remaining -= available_amount;
            }
        }
        unit_index += 1U;
    }
    if (remaining != 0U || required_splits
        > FT_CARD_GAME_MAX_RESOURCE_UNITS - this->_unit_count
        || (required_splits != 0U && this->_next_unit_id
            == UINT32_MAX))
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
            if (this->_units[unit_index].locked_amount != 0U
                && this->_units[unit_index].unlock_epoch != 0U)
            {
                locked_now = available_amount;
                if (locked_now > remaining)
                    locked_now = remaining;
                result = this->split_unit_lock(unit_index, locked_now, 0U);
                if (result != FT_ERR_SUCCESS)
                    return (result);
                remaining -= locked_now;
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
    uint32_t required_splits;
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
            && this->_units[unit_index].resource_type_id == resource_type_id)
            eligible_amount += this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
        unit_index += 1U;
    }
    if (eligible_amount < amount)
        return (FT_ERR_FULL);
    required_splits = 0U;
    remaining = amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id)
        {
            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (available_amount != 0U)
            {
                if (available_amount > remaining)
                    available_amount = remaining;
                if (this->_units[unit_index].locked_amount != 0U
                    && this->_units[unit_index].unlock_epoch != unlock_epoch)
                    required_splits += 1U;
                remaining -= available_amount;
            }
        }
        unit_index += 1U;
    }
    if (remaining != 0U || required_splits
        > FT_CARD_GAME_MAX_RESOURCE_UNITS - this->_unit_count
        || (required_splits != 0U && this->_next_unit_id
            == UINT32_MAX))
        return (FT_ERR_FULL);
    remaining = amount;
    unit_index = 0U;
    while (unit_index < this->_unit_count && remaining > 0U)
    {
        if (this->_units[unit_index].owner_id == owner_id
            && this->_units[unit_index].resource_type_id == resource_type_id)
        {
            available_amount = this->_units[unit_index].amount
                - this->_units[unit_index].locked_amount;
            if (available_amount == 0U)
            {
                unit_index += 1U;
                continue ;
            }
            if (this->_units[unit_index].locked_amount != 0U
                && this->_units[unit_index].unlock_epoch != unlock_epoch)
            {
                locked_now = available_amount;
                if (locked_now > remaining)
                    locked_now = remaining;
                result = this->split_unit_lock(unit_index, locked_now,
                    unlock_epoch);
                if (result != FT_ERR_SUCCESS)
                    return (result);
                remaining -= locked_now;
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

int32_t card_game_resource_ledger::split_unit_lock(uint32_t unit_index,
    uint32_t amount, uint64_t unlock_epoch) noexcept
{
    card_game_resource_unit *source_unit;
    card_game_resource_unit *locked_unit;
    uint32_t locked_unit_index;

    if (unit_index >= this->_unit_count || amount == 0U
        || amount > this->_units[unit_index].amount
            - this->_units[unit_index].locked_amount
        || this->_unit_count >= FT_CARD_GAME_MAX_RESOURCE_UNITS
        || this->_next_unit_id == 0U || this->_next_unit_id == UINT32_MAX)
        return (FT_ERR_FULL);
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
    return (FT_ERR_SUCCESS);
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

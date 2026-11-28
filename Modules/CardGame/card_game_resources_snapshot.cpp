#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"
#include <algorithm>

struct card_game_snapshot_pool_key
{
    uint64_t key;
    uint32_t original_index;
};

struct card_game_snapshot_pool_totals
{
    uint64_t current_amount;
    uint64_t locked_amount;
    uint64_t temporary_amount;
};

static uint64_t card_game_resource_key(uint32_t owner_id,
    uint32_t resource_type_id) noexcept
{
    return ((static_cast<uint64_t>(owner_id) << 32U) | resource_type_id);
}

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
    card_game_resource_pool *old_pools;
    card_game_resource_unit *old_units;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot == ft_nullptr)
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
    old_pools = snapshot->pools;
    old_units = snapshot->units;
    snapshot->pool_count = this->_pool_count;
    snapshot->unit_count = this->_unit_count;
    snapshot->next_unit_id = this->_next_unit_id;
    snapshot->pools = pools;
    snapshot->units = units;
    if (old_pools != ft_nullptr)
        cma_free(old_pools);
    if (old_units != ft_nullptr)
        cma_free(old_units);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::clone_snapshot(
    const card_game_resource_snapshot &source,
    card_game_resource_snapshot *destination) noexcept
{
    card_game_resource_pool *pools;
    card_game_resource_unit *units;
    card_game_resource_pool *old_pools;
    card_game_resource_unit *old_units;

    if (destination == ft_nullptr
        || source.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || source.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || (source.pool_count != 0U && source.pools == ft_nullptr)
        || (source.unit_count != 0U && source.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (destination == &source)
        return (FT_ERR_SUCCESS);
    pools = ft_nullptr;
    units = ft_nullptr;
    if (source.pool_count != 0U)
    {
        pools = static_cast<card_game_resource_pool *>(cma_malloc(
            static_cast<ft_size_t>(source.pool_count)
                * sizeof(card_game_resource_pool)));
        if (pools == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(pools, source.pools,
            static_cast<ft_size_t>(source.pool_count)
                * sizeof(card_game_resource_pool));
    }
    if (source.unit_count != 0U)
    {
        units = static_cast<card_game_resource_unit *>(cma_malloc(
            static_cast<ft_size_t>(source.unit_count)
                * sizeof(card_game_resource_unit)));
        if (units == ft_nullptr)
        {
            if (pools != ft_nullptr)
                cma_free(pools);
            return (FT_ERR_NO_MEMORY);
        }
        ft_memcpy(units, source.units,
            static_cast<ft_size_t>(source.unit_count)
                * sizeof(card_game_resource_unit));
    }
    old_pools = destination->pools;
    old_units = destination->units;
    destination->pool_count = source.pool_count;
    destination->unit_count = source.unit_count;
    destination->next_unit_id = source.next_unit_id;
    destination->pools = pools;
    destination->units = units;
    if (old_pools != ft_nullptr)
        cma_free(old_pools);
    if (old_units != ft_nullptr)
        cma_free(old_units);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::apply_snapshot(
    const card_game_resource_snapshot &snapshot) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || snapshot.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || (snapshot.pool_count != 0U && snapshot.pools == ft_nullptr)
        || (snapshot.unit_count != 0U && snapshot.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    card_game_snapshot_pool_key pool_keys[FT_CARD_GAME_MAX_RESOURCE_POOLS];
    card_game_snapshot_pool_totals pool_totals[FT_CARD_GAME_MAX_RESOURCE_POOLS];
    ft_bool pool_keys_ascending = FT_TRUE;
    uint32_t index = 0U;
    while (index < snapshot.pool_count)
    {
        const card_game_resource_pool &pool = snapshot.pools[index];

        if (pool.pool_id != index + 1U || pool.resource_type_id == 0U
            || pool.current_amount > pool.maximum_amount
            || pool.locked_amount > pool.current_amount
            || pool.temporary_amount > pool.current_amount)
            return (FT_ERR_INVALID_ARGUMENT);
        pool_keys[index] = {card_game_resource_key(pool.owner_id,
            pool.resource_type_id), index};
        pool_totals[index] = {};
        if (index != 0U && pool_keys[index - 1U].key >= pool_keys[index].key)
            pool_keys_ascending = FT_FALSE;
        index += 1U;
    }
    if (pool_keys_ascending == FT_FALSE)
    {
        std::sort(pool_keys, pool_keys + snapshot.pool_count,
            [](const card_game_snapshot_pool_key &first,
                const card_game_snapshot_pool_key &second)
            {
                return (first.key < second.key);
            });
        index = 1U;
        while (index < snapshot.pool_count)
        {
            if (pool_keys[index - 1U].key == pool_keys[index].key)
                return (FT_ERR_INVALID_ARGUMENT);
            index += 1U;
        }
    }
    uint32_t unit_ids[FT_CARD_GAME_MAX_RESOURCE_UNITS];
    uint32_t highest_unit_id = 0U;
    ft_bool unit_ids_ascending = FT_TRUE;
    const card_game_snapshot_pool_key *pool_keys_begin = pool_keys;
    const card_game_snapshot_pool_key *pool_keys_end = pool_keys + snapshot.pool_count;
    index = 0U;
    while (index < snapshot.unit_count)
    {
        const card_game_resource_unit &unit = snapshot.units[index];

        if (unit.unit_id == 0U || unit.locked_amount > unit.amount
            || unit.temporary > FT_TRUE)
            return (FT_ERR_INVALID_ARGUMENT);
        unit_ids[index] = unit.unit_id;
        if (index != 0U && unit_ids[index - 1U] >= unit.unit_id)
            unit_ids_ascending = FT_FALSE;
        if (unit.unit_id > highest_unit_id)
            highest_unit_id = unit.unit_id;
        const uint64_t key = card_game_resource_key(unit.owner_id,
            unit.resource_type_id);
        const card_game_snapshot_pool_key *match = std::lower_bound(pool_keys_begin,
            pool_keys_end, key,
            [](const card_game_snapshot_pool_key &entry, uint64_t value)
            {
                return (entry.key < value);
            });
        if (match == pool_keys_end || match->key != key)
            return (FT_ERR_INVALID_ARGUMENT);
        card_game_snapshot_pool_totals &totals = pool_totals[match->original_index];
        totals.current_amount += unit.amount;
        totals.locked_amount += unit.locked_amount;
        if (unit.temporary != FT_FALSE)
            totals.temporary_amount += unit.amount;
        index += 1U;
    }
    if (unit_ids_ascending == FT_FALSE)
    {
        std::sort(unit_ids, unit_ids + snapshot.unit_count);
        if (std::adjacent_find(unit_ids, unit_ids + snapshot.unit_count)
            != unit_ids + snapshot.unit_count)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    if ((snapshot.unit_count == 0U && snapshot.next_unit_id == 0U)
        || (snapshot.next_unit_id != 0U
            && snapshot.next_unit_id <= highest_unit_id)
        || (snapshot.next_unit_id == 0U
            && highest_unit_id != UINT32_MAX))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < snapshot.pool_count)
    {
        const card_game_snapshot_pool_totals &totals = pool_totals[index];
        const card_game_resource_pool &pool = snapshot.pools[index];

        if (totals.current_amount != pool.current_amount
            || totals.locked_amount != pool.locked_amount
            || totals.temporary_amount != pool.temporary_amount)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    ft_memmove(this->_pools, snapshot.pools,
        snapshot.pool_count * sizeof(*this->_pools));
    ft_memmove(this->_units, snapshot.units,
        snapshot.unit_count * sizeof(*this->_units));
    this->_pool_count = snapshot.pool_count;
    this->_unit_count = snapshot.unit_count;
    this->_next_unit_id = snapshot.next_unit_id;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_resource_ledger::matches_snapshot(
    const card_game_resource_snapshot &snapshot) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_pool_count != snapshot.pool_count
        || this->_unit_count != snapshot.unit_count
        || this->_next_unit_id != snapshot.next_unit_id)
        return (FT_FALSE);
    if (this->_pool_count != 0U && (snapshot.pools == ft_nullptr
            || ft_memcmp(this->_pools, snapshot.pools,
                this->_pool_count * sizeof(*this->_pools)) != 0))
        return (FT_FALSE);
    return (this->_unit_count == 0U || (snapshot.units != ft_nullptr
        && ft_memcmp(this->_units, snapshot.units,
            this->_unit_count * sizeof(*this->_units)) == 0));
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

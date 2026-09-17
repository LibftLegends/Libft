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
    uint32_t highest_unit_id;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.pool_count > FT_CARD_GAME_MAX_RESOURCE_POOLS
        || snapshot.unit_count > FT_CARD_GAME_MAX_RESOURCE_UNITS
        || (snapshot.pool_count != 0U && snapshot.pools == ft_nullptr)
        || (snapshot.unit_count != 0U && snapshot.units == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    highest_unit_id = 0U;
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
        if (snapshot.units[index].unit_id > highest_unit_id)
            highest_unit_id = snapshot.units[index].unit_id;
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
    if ((snapshot.unit_count == 0U && snapshot.next_unit_id == 0U)
        || (snapshot.next_unit_id != 0U
            && snapshot.next_unit_id <= highest_unit_id)
        || (snapshot.next_unit_id == 0U
            && highest_unit_id != UINT32_MAX))
        return (FT_ERR_INVALID_ARGUMENT);
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

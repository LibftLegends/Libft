#include "card_game_resources.hpp"
#include <algorithm>

int32_t card_game_resource_ledger::create_payment_plan(uint32_t owner_id,
    const card_game_resource_requirement &requirement,
    card_game_payment_plan *plan) const noexcept
{
    return (this->create_payment_plan_internal(owner_id, requirement,
        ft_nullptr, plan));
}

int32_t card_game_resource_ledger::create_payment_plan_internal(
    uint32_t owner_id, const card_game_resource_requirement &requirement,
    uint32_t *reserved_amounts, card_game_payment_plan *plan) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan == ft_nullptr || requirement.amount == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    card_game_payment_plan candidate;
    candidate.count = 0U;
    uint32_t remaining = requirement.amount;
    uint32_t unit_index = 0U;
    while (unit_index < this->_unit_count && remaining != 0U)
    {
        const card_game_resource_unit &unit = this->_units[unit_index];
        if (unit.owner_id == owner_id && this->unit_matches(unit, requirement))
        {
            uint32_t available_amount = unit.amount - unit.locked_amount;
            if (reserved_amounts != ft_nullptr)
                available_amount -= reserved_amounts[unit_index];
            if (available_amount != 0U)
            {
                if (candidate.count == FT_CARD_GAME_MAX_PAYMENT_UNITS)
                    return (FT_ERR_FULL);
                const uint32_t amount = std::min(available_amount, remaining);
                candidate.units[candidate.count] = {unit.unit_id, amount};
                candidate.count += 1U;
                remaining -= amount;
                if (reserved_amounts != ft_nullptr)
                    reserved_amounts[unit_index] += amount;
            }
        }
        unit_index += 1U;
    }
    if (remaining != 0U)
        return (FT_ERR_FULL);
    ft_memcpy(plan->units, candidate.units, candidate.count * sizeof(*plan->units));
    plan->count = candidate.count;
    plan->total_amount = requirement.amount;
    return (FT_ERR_SUCCESS);
}
int32_t card_game_resource_ledger::spend(
    const card_game_payment_plan &plan) noexcept
{
    uint32_t payment_index;
    uint32_t unique_count;
    uint32_t unit_indexes[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t pool_indexes[FT_CARD_GAME_MAX_PAYMENT_UNITS];
    uint32_t unit_usage[FT_CARD_GAME_MAX_RESOURCE_UNITS];
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
    ft_bzero(unit_usage, this->_unit_count * sizeof(*unit_usage));
    while (payment_index < plan.count)
    {
        result = this->find_unit(plan.units[payment_index].unit_id, &unit_index);
        if (result != FT_ERR_SUCCESS || plan.units[payment_index].amount == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (total_amount > UINT32_MAX - plan.units[payment_index].amount)
            return (FT_ERR_INVALID_ARGUMENT);
        total_amount += plan.units[payment_index].amount;
        const card_game_resource_unit &unit = this->_units[unit_index];
        const uint32_t amount = plan.units[payment_index].amount;
        if (amount > unit.amount - unit.locked_amount - unit_usage[unit_index])
            return (FT_ERR_INVALID_ARGUMENT);
        if (unit_usage[unit_index] == 0U)
        {
            result = this->find_pool(unit.owner_id, unit.resource_type_id,
                &pool_indexes[unique_count]);
            if (result != FT_ERR_SUCCESS)
                return (result);
            unit_indexes[unique_count] = unit_index;
            unique_count += 1U;
        }
        unit_usage[unit_index] += amount;
        payment_index += 1U;
    }
    if (total_amount != plan.total_amount)
        return (FT_ERR_INVALID_ARGUMENT);
    payment_index = 0U;
    while (payment_index < unique_count)
    {
        unit_index = unit_indexes[payment_index];
        card_game_resource_unit &unit = this->_units[unit_index];
        card_game_resource_pool &pool = this->_pools[pool_indexes[payment_index]];
        const uint32_t amount = unit_usage[unit_index];
        unit.amount -= amount;
        pool.current_amount -= amount;
        if (unit.temporary != FT_FALSE)
            pool.temporary_amount -= amount;
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
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::create_cost_plan_components(
    uint32_t owner_id, const card_game_resource_requirement *requirements,
    uint32_t variable_amount, card_game_cost_plan *candidate) const noexcept
{
    uint32_t reserved_amounts[FT_CARD_GAME_MAX_RESOURCE_UNITS];
    uint32_t reserved_count = 0U;
    uint32_t component_index = 0U;

    ft_bzero(reserved_amounts, this->_unit_count * sizeof(*reserved_amounts));
    while (component_index < candidate->component_count)
    {
        card_game_resource_requirement requirement = requirements[component_index];
        if (component_index == 0U)
            requirement.amount += variable_amount;
        card_game_payment_plan &payment = candidate->components[component_index];
        const int32_t result = this->create_payment_plan_internal(owner_id,
            requirement, reserved_amounts, &payment);
        if (result != FT_ERR_SUCCESS)
            return (result);
        reserved_count += payment.count;
        if (reserved_count > FT_CARD_GAME_MAX_PAYMENT_UNITS)
            return (FT_ERR_FULL);
        component_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_resource_ledger::create_cost_plan(uint32_t owner_id,
    const card_game_cost &cost, uint32_t variable_amount,
    card_game_cost_plan *plan) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || plan == ft_nullptr || cost.component_count
            > FT_CARD_GAME_MAX_COST_COMPONENTS
        || cost.alternative_count > FT_CARD_GAME_MAX_COST_ALTERNATIVES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (cost.component_count == 0U && cost.alternative_count == 0U
        && variable_amount != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    card_game_cost_plan candidate;
    uint32_t choice_count = cost.alternative_count;
    if (choice_count == 0U)
        choice_count = 1U;
    uint32_t choice_index = 0U;
    while (choice_index < choice_count)
    {
        const card_game_resource_requirement *requirements = cost.components;
        candidate.selected_alternative = UINT32_MAX;
        candidate.component_count = cost.component_count;
        if (cost.alternative_count != 0U)
        {
            requirements = cost.alternatives[choice_index];
            candidate.selected_alternative = choice_index;
            candidate.component_count = cost.alternative_component_counts[choice_index];
        }
        if (candidate.component_count <= FT_CARD_GAME_MAX_COST_COMPONENTS)
        {
            if (candidate.component_count != 0U
                && requirements[0].amount > UINT32_MAX - variable_amount)
                return (FT_ERR_INVALID_ARGUMENT);
            const int32_t result = this->create_cost_plan_components(owner_id,
                requirements, variable_amount, &candidate);
            if (result == FT_ERR_SUCCESS)
                break ;
            if (cost.alternative_count == 0U)
                return (result);
        }
        choice_index += 1U;
    }
    if (choice_index == choice_count)
        return (FT_ERR_FULL);
    uint64_t total_amount = 0U;
    uint32_t component_index = 0U;
    while (component_index < candidate.component_count)
    {
        total_amount += candidate.components[component_index].total_amount;
        component_index += 1U;
    }
    if (total_amount > UINT32_MAX)
        return (FT_ERR_FULL);

    plan->selected_alternative = candidate.selected_alternative;
    plan->component_count = candidate.component_count;
    plan->combined.count = 0U;
    plan->combined.total_amount = static_cast<uint32_t>(total_amount);
    component_index = 0U;
    while (component_index < candidate.component_count)
    {
        const card_game_payment_plan &payment = candidate.components[component_index];
        card_game_payment_plan &output = plan->components[component_index];
        output.count = payment.count;
        output.total_amount = payment.total_amount;
        ft_memcpy(output.units, payment.units, payment.count * sizeof(*output.units));
        ft_memcpy(plan->combined.units + plan->combined.count, payment.units,
            payment.count * sizeof(*payment.units));
        plan->combined.count += payment.count;
        component_index += 1U;
    }
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

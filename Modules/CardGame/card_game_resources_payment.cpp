#include "card_game_resources.hpp"
#include "../CMA/CMA.hpp"

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


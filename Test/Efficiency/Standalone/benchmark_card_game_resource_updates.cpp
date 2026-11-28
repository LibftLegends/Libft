#include "../../../Modules/CardGame/card_game_resources.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2)
        return (1);
    const ft_bool spend = ft_strcmp(arguments[1], "--spend") == 0;
    const ft_bool cost = ft_strcmp(arguments[1], "--cost") == 0;
    if (!spend && !cost && ft_strcmp(arguments[1], "--refresh") != 0)
        return (1);

    card_game_resource_ledger ledger;
    card_game_cost requirements{};
    card_game_cost_plan cost_plan{};
    card_game_payment_plan payment{};
    constexpr uint32_t POOLS = FT_CARD_GAME_MAX_RESOURCE_POOLS;
    constexpr uint32_t UNITS = FT_CARD_GAME_MAX_RESOURCE_UNITS;
    constexpr uint32_t UNIT_AMOUNT = 1000000U;
    constexpr uint32_t ITERATIONS = 20000U;
    if (ledger.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < POOLS)
    {
        uint32_t pool_id;
        if (ledger.register_pool(1U, index + 1U, UNIT_AMOUNT * UNITS / POOLS,
                &pool_id) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    index = 0U;
    while (index < UNITS)
    {
        uint32_t unit_id;
        if (ledger.add_units(1U, index % POOLS + 1U, UNIT_AMOUNT, 0U, 0U,
                FT_FALSE, &unit_id) != FT_ERR_SUCCESS)
            return (1);
        if (index >= UNITS - FT_CARD_GAME_MAX_PAYMENT_UNITS)
            payment.units[payment.count++] = {unit_id, 1U};
        ++index;
    }
    payment.total_amount = payment.count;
    requirements.component_count = FT_CARD_GAME_MAX_COST_COMPONENTS;
    index = 0U;
    while (index < requirements.component_count)
    {
        requirements.components[index] = {POOLS - index, 4U * UNIT_AMOUNT, 0U, 0U};
        ++index;
    }
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        int32_t status;
        if (spend)
            status = ledger.spend(payment);
        else if (cost)
            status = ledger.create_cost_plan(1U, requirements, 0U, &cost_plan);
        else
            status = ledger.refresh(1U);
        if (status != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    card_game_resource_pool last{};
    if (ledger.get_pool(1U, POOLS, &last) != FT_ERR_SUCCESS)
        return (1);
    uint32_t checksum = last.current_amount + ledger.unit_count();
    if (cost)
    {
        index = 0U;
        while (index < cost_plan.combined.count)
        {
            checksum = checksum * 31U + cost_plan.combined.units[index].unit_id;
            checksum = checksum * 31U + cost_plan.combined.units[index].amount;
            ++index;
        }
    }
    std::printf("resource updates checksum %" PRIu32 " %.3f ns/op\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}

#include "networking_replication_apply_budget.hpp"

networking_replication_apply_budget::networking_replication_apply_budget() noexcept
    : maximum_messages(0U), maximum_payload_bytes(0U),
      maximum_operations(0U), messages_used(0U), payload_bytes_used(0U),
      operations_used(0U)
{
    return ;
}

networking_replication_apply_budget::~networking_replication_apply_budget() noexcept
{
    return ;
}

int32_t networking_replication_apply_budget_initialize(
    networking_replication_apply_budget &budget, uint32_t maximum_messages,
    uint32_t maximum_payload_bytes, uint32_t maximum_operations) noexcept
{
    if (maximum_messages == 0U || maximum_payload_bytes == 0U
        || maximum_operations == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    budget.maximum_messages = maximum_messages;
    budget.maximum_payload_bytes = maximum_payload_bytes;
    budget.maximum_operations = maximum_operations;
    budget.messages_used = 0U;
    budget.payload_bytes_used = 0U;
    budget.operations_used = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_apply_budget_reset(
    networking_replication_apply_budget &budget) noexcept
{
    if (budget.maximum_messages == 0U
        || budget.maximum_payload_bytes == 0U
        || budget.maximum_operations == 0U)
        return (FT_ERR_NOT_INITIALISED);
    budget.messages_used = 0U;
    budget.payload_bytes_used = 0U;
    budget.operations_used = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t networking_replication_apply_budget_consume(
    networking_replication_apply_budget &budget, uint32_t payload_bytes,
    uint32_t operations) noexcept
{
    if (budget.maximum_messages == 0U
        || budget.maximum_payload_bytes == 0U
        || budget.maximum_operations == 0U)
        return (FT_ERR_NOT_INITIALISED);
    if (networking_replication_apply_budget_can_consume(budget,
        payload_bytes, operations) == FT_FALSE)
        return (FT_ERR_FULL);
    budget.messages_used += 1U;
    budget.payload_bytes_used += payload_bytes;
    budget.operations_used += operations;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_replication_apply_budget_can_consume(
    const networking_replication_apply_budget &budget, uint32_t payload_bytes,
    uint32_t operations) noexcept
{
    if (budget.maximum_messages == 0U
        || budget.maximum_payload_bytes == 0U
        || budget.maximum_operations == 0U)
        return (FT_FALSE);
    if (budget.messages_used >= budget.maximum_messages
        || budget.payload_bytes_used > budget.maximum_payload_bytes
        || budget.operations_used > budget.maximum_operations)
        return (FT_FALSE);
    if (payload_bytes > budget.maximum_payload_bytes
        - budget.payload_bytes_used
        || operations > budget.maximum_operations
        - budget.operations_used)
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool networking_replication_apply_budget_has_capacity(
    const networking_replication_apply_budget &budget) noexcept
{
    if (budget.maximum_messages == 0U
        || budget.maximum_payload_bytes == 0U
        || budget.maximum_operations == 0U)
        return (FT_FALSE);
    if (budget.messages_used >= budget.maximum_messages
        || budget.payload_bytes_used > budget.maximum_payload_bytes
        || budget.operations_used > budget.maximum_operations)
        return (FT_FALSE);
    return (FT_TRUE);
}

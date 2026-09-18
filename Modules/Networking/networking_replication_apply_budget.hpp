#ifndef NETWORKING_REPLICATION_APPLY_BUDGET_HPP
#define NETWORKING_REPLICATION_APPLY_BUDGET_HPP

#include "../Errno/errno.hpp"
#include <cstdint>

/*
 * Per-pump budget token for a replicated client. It prevents a large batch
 * from monopolising the caller's frame or tick. Consuming a request is
 * transactional: FT_ERR_FULL leaves every counter unchanged.
 */
struct networking_replication_apply_budget
{
    uint32_t maximum_messages;
    uint32_t maximum_payload_bytes;
    uint32_t maximum_operations;
    uint32_t messages_used;
    uint32_t payload_bytes_used;
    uint32_t operations_used;

    networking_replication_apply_budget() noexcept;
    ~networking_replication_apply_budget() noexcept;
};

int32_t networking_replication_apply_budget_initialize(
    networking_replication_apply_budget &budget, uint32_t maximum_messages,
    uint32_t maximum_payload_bytes, uint32_t maximum_operations) noexcept;
int32_t networking_replication_apply_budget_reset(
    networking_replication_apply_budget &budget) noexcept;
int32_t networking_replication_apply_budget_consume(
    networking_replication_apply_budget &budget, uint32_t payload_bytes,
    uint32_t operations) noexcept;
ft_bool networking_replication_apply_budget_can_consume(
    const networking_replication_apply_budget &budget, uint32_t payload_bytes,
    uint32_t operations) noexcept;
ft_bool networking_replication_apply_budget_has_capacity(
    const networking_replication_apply_budget &budget) noexcept;

#endif

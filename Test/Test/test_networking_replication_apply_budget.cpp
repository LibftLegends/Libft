#include "../../Modules/Networking/networking_replication_apply_budget.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_replication_apply_budget_consumes_within_limits)
{
    networking_replication_apply_budget budget;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_initialize(budget, 2U, 100U, 10U));
    FT_ASSERT_EQ(FT_TRUE,
        networking_replication_apply_budget_has_capacity(budget));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_consume(budget, 40U, 4U));
    FT_ASSERT_EQ(1U, budget.messages_used);
    FT_ASSERT_EQ(40U, budget.payload_bytes_used);
    FT_ASSERT_EQ(4U, budget.operations_used);
    return (1);
}

FT_TEST(test_networking_replication_apply_budget_rejects_without_partial_commit)
{
    networking_replication_apply_budget budget;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_initialize(budget, 1U, 50U, 5U));
    FT_ASSERT_EQ(FT_ERR_FULL,
        networking_replication_apply_budget_consume(budget, 51U, 1U));
    FT_ASSERT_EQ(0U, budget.messages_used);
    FT_ASSERT_EQ(0U, budget.payload_bytes_used);
    FT_ASSERT_EQ(0U, budget.operations_used);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_consume(budget, 50U, 5U));
    FT_ASSERT_EQ(FT_FALSE,
        networking_replication_apply_budget_has_capacity(budget));
    return (1);
}

FT_TEST(test_networking_replication_apply_budget_reset_starts_next_pump)
{
    networking_replication_apply_budget budget;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_initialize(budget, 1U, 50U, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_consume(budget, 10U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_replication_apply_budget_reset(budget));
    FT_ASSERT_EQ(0U, budget.messages_used);
    FT_ASSERT_EQ(FT_TRUE,
        networking_replication_apply_budget_has_capacity(budget));
    return (1);
}

FT_TEST(test_networking_replication_apply_budget_rejects_invalid_limits)
{
    networking_replication_apply_budget budget;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_replication_apply_budget_initialize(budget, 0U, 1U, 1U));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        networking_replication_apply_budget_reset(budget));
    return (1);
}

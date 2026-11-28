#include "../../Modules/Networking/networking_replication_client.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

struct networking_replication_client_test_state
{
    uint32_t snapshots;
    uint32_t block_deltas;
    uint32_t light_deltas;
    ft_bool fail_callback;
};

static int32_t networking_replication_client_test_snapshot(
    const ft_byte_buffer &payload, void *user_data) noexcept
{
    networking_replication_client_test_state *state;

    (void)payload;
    state = static_cast<networking_replication_client_test_state *>(user_data);
    state->snapshots += 1U;
    if (state->fail_callback != FT_FALSE)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t networking_replication_client_test_block_delta(
    const ft_byte_buffer &payload, void *user_data) noexcept
{
    networking_replication_client_test_state *state;

    (void)payload;
    state = static_cast<networking_replication_client_test_state *>(user_data);
    state->block_deltas += 1U;
    if (state->fail_callback != FT_FALSE)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t networking_replication_client_test_light_delta(
    const ft_byte_buffer &payload, void *user_data) noexcept
{
    networking_replication_client_test_state *state;

    (void)payload;
    state = static_cast<networking_replication_client_test_state *>(user_data);
    state->light_deltas += 1U;
    if (state->fail_callback != FT_FALSE)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_networking_replication_client_stages_and_budgets_updates)
{
    networking_replication_client client;
    networking_replication_client_test_state state;
    ft_byte_buffer payload;

    state.snapshots = 0U;
    state.block_deltas = 0U;
    state.light_deltas = 0U;
    state.fail_callback = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.append_u8(7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(4U, 5U, 2U, 32U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.set_callbacks(
        networking_replication_client_test_snapshot,
        networking_replication_client_test_block_delta,
        networking_replication_client_test_light_delta, &state));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        client.apply_block_delta(0U, 1U, payload, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client.apply_snapshot(10U, 20U, 1U, payload, 1U));
    FT_ASSERT_EQ(FT_TRUE, client.is_snapshot_ready());
    FT_ASSERT_EQ(10U, client.get_block_revision());
    FT_ASSERT_EQ(20U, client.get_light_revision());
    FT_ASSERT_EQ(1U, state.snapshots);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        client.apply_light_delta(20U, 21U, 9U, payload, 1U));
    FT_ASSERT_EQ(0U, state.light_deltas);
    state.fail_callback = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_INTERNAL,
        client.apply_block_delta(10U, 11U, payload, 1U));
    FT_ASSERT_EQ(10U, client.get_block_revision());
    state.fail_callback = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client.apply_block_delta(10U, 11U, payload, 1U));
    FT_ASSERT_EQ(11U, client.get_block_revision());
    FT_ASSERT_EQ(FT_ERR_FULL,
        client.apply_light_delta(20U, 21U, 11U, payload, 1U));
    FT_ASSERT_EQ(0U, state.light_deltas);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.reset_budget());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client.apply_light_delta(20U, 21U, 11U, payload, 1U));
    FT_ASSERT_EQ(21U, client.get_light_revision());
    FT_ASSERT_EQ(1U, state.light_deltas);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.destroy());
    return (1);
}

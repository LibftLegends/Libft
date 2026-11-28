#include "../../Modules/Networking/networking_replication_retention_window.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_replication_retention_window_tracks_contiguous_revisions)
{
    networking_replication_retention_window window;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.initialize(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(6U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(7U));
    FT_ASSERT_EQ(5U, window.get_oldest_revision());
    FT_ASSERT_EQ(7U, window.get_latest_revision());
    FT_ASSERT_EQ(3U, window.get_retained_count());
    FT_ASSERT(window.can_replay_from(4U));
    FT_ASSERT(window.can_replay_from(7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.destroy());
    return (1);
}

FT_TEST(test_networking_replication_retention_window_evicts_oldest_revision)
{
    networking_replication_retention_window window;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.initialize(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(3U));
    FT_ASSERT_EQ(2U, window.get_oldest_revision());
    FT_ASSERT_EQ(FT_FALSE, window.can_replay_from(0U));
    FT_ASSERT_EQ(FT_TRUE, window.needs_snapshot(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.destroy());
    return (1);
}

FT_TEST(test_networking_replication_retention_window_acknowledges_idempotently)
{
    networking_replication_retention_window window;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.initialize(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(11U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.acknowledge(11U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.acknowledge(10U));
    FT_ASSERT_EQ(11U, window.get_acknowledged_revision());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, window.acknowledge(12U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.destroy());
    return (1);
}

FT_TEST(test_networking_replication_retention_window_rejects_revision_gaps)
{
    networking_replication_retention_window window;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.initialize(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.append_revision(20U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, window.append_revision(22U));
    FT_ASSERT_EQ(20U, window.get_latest_revision());
    FT_ASSERT_EQ(1U, window.get_retained_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window.destroy());
    return (1);
}

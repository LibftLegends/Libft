#include "../../Modules/Networking/networking_replication_revision_tracker.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_replication_revision_tracker_requires_snapshot)
{
    networking_replication_revision_tracker tracker;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, tracker.accept_block_delta(0U, 1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        tracker.accept_light_delta(0U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.destroy());
    return (1);
}

FT_TEST(test_networking_replication_revision_tracker_accepts_contiguous_updates)
{
    networking_replication_revision_tracker tracker;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.accept_snapshot(10U, 20U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.accept_block_delta(10U, 11U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        tracker.accept_light_delta(20U, 21U, 11U));
    FT_ASSERT_EQ(11U, tracker.get_block_revision());
    FT_ASSERT_EQ(21U, tracker.get_light_revision());
    FT_ASSERT_EQ(3U, tracker.get_snapshot_generation());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.destroy());
    return (1);
}

FT_TEST(test_networking_replication_revision_tracker_rejects_gaps_and_stale_light)
{
    networking_replication_revision_tracker tracker;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.accept_snapshot(4U, 6U, 7U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, tracker.accept_block_delta(3U, 5U));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        tracker.accept_light_delta(6U, 7U, 3U));
    FT_ASSERT_EQ(4U, tracker.get_block_revision());
    FT_ASSERT_EQ(6U, tracker.get_light_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.destroy());
    return (1);
}

FT_TEST(test_networking_replication_revision_tracker_move_transfers_state)
{
    networking_replication_revision_tracker source;
    networking_replication_revision_tracker destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.accept_snapshot(8U, 9U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_TRUE, destination.is_snapshot_ready());
    FT_ASSERT_EQ(8U, destination.get_block_revision());
    FT_ASSERT_EQ(9U, destination.get_light_revision());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

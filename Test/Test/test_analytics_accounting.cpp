#include "../test_internal.hpp"
#include "../../Modules/Analytics/analytics.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_analytics_accounting_counts_each_scope_once_and_accepts_zero)
{
    analytics_session session;
    analytics_region_statistics statistics;
    uint32_t region_id;
    uint64_t percentile;

    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("accounting",
        "test", &region_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.record_scope(region_id, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.record_scope(region_id, 5U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.record_scope(region_id, 10U, 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(3U, statistics.invocation_count);
    FT_ASSERT_EQ(15U, statistics.inclusive_nanoseconds);
    FT_ASSERT_EQ(10U, statistics.exclusive_nanoseconds);
    FT_ASSERT_EQ(0U, statistics.minimum_nanoseconds);
    FT_ASSERT_EQ(10U, statistics.maximum_nanoseconds);
    FT_ASSERT_EQ(5U, statistics.percentile_50_nanoseconds);
    FT_ASSERT_EQ(5U, statistics.percentile_95_nanoseconds);
    FT_ASSERT_EQ(5U, statistics.percentile_99_nanoseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_percentile(region_id,
        50U, &percentile));
    FT_ASSERT_EQ(5U, percentile);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, session.get_region_percentile(
        region_id, 90U, &percentile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

FT_TEST(test_analytics_accounting_reports_dropped_scopes_and_breakdowns)
{
    analytics_session session;
    analytics_region_statistics statistics;
    analytics_frame_statistics frame;
    uint32_t region_id;
    uint32_t index;

    region_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.register_region("overflow",
        "test", &region_id));
    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_EVENTS + 1U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, session.record_scope(region_id, 1U, 1U));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.note_dropped_scope());
    FT_ASSERT_EQ(1U, session.get_dropped_scope_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_region_statistics(region_id,
        &statistics));
    FT_ASSERT_EQ(FT_ANALYTICS_MAX_THREAD_EVENTS + 1U,
        statistics.invocation_count);
    frame = {};
    frame.frame_number = 7U;
    frame.duration_nanoseconds = 1U;
    frame.breakdown_count = FT_ANALYTICS_MAX_FRAME_BREAKDOWN;
    index = 0U;
    while (index < FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
    {
        frame.breakdown[index].region_id = index;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.publish_frame(frame));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.get_latest_frame(&frame));
    FT_ASSERT_EQ(7U, frame.frame_number);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.destroy());
    return (1);
}

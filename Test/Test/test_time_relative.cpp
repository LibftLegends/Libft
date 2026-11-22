#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include <limits>
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

FT_TEST(test_time_add_helpers_apply_fixed_units)
{
    FT_ASSERT_EQ(static_cast<t_time>(1), time_add_seconds(0, 1));
    FT_ASSERT_EQ(static_cast<t_time>(60), time_add_minutes(0, 1));
    FT_ASSERT_EQ(static_cast<t_time>(3600), time_add_hours(0, 1));
    FT_ASSERT_EQ(static_cast<t_time>(604800), time_add_weeks(0, 1));
    return (1);
}

FT_TEST(test_time_add_days_applies_whole_day_offsets)
{
    t_time time_value;

    time_value = time_add_days(0, 1);
    FT_ASSERT_EQ(static_cast<t_time>(86400), time_value);
    time_value = time_add_days(0, -1);
    FT_ASSERT_EQ(static_cast<t_time>(-86400), time_value);
    time_value = time_add_days(3600, 2);
    FT_ASSERT_EQ(static_cast<t_time>(176400), time_value);
    return (1);
}

FT_TEST(test_time_floor_helpers_round_down_negative_and_positive_values)
{
    FT_ASSERT_EQ(static_cast<t_time>(123), time_floor_to_second(123));
    FT_ASSERT_EQ(static_cast<t_time>(-1), time_floor_to_second(-1));
    FT_ASSERT_EQ(static_cast<t_time>(120), time_floor_to_minute(179));
    FT_ASSERT_EQ(static_cast<t_time>(-60), time_floor_to_minute(-1));
    FT_ASSERT_EQ(static_cast<t_time>(7200), time_floor_to_hour(7201));
    FT_ASSERT_EQ(static_cast<t_time>(-3600), time_floor_to_hour(-1));
    FT_ASSERT_EQ(static_cast<t_time>(86400), time_floor_to_day(86400 + 1));
    FT_ASSERT_EQ(static_cast<t_time>(-86400), time_floor_to_day(-1));
    FT_ASSERT_EQ(static_cast<t_time>(604800), time_floor_to_week(604800 + 1));
    FT_ASSERT_EQ(static_cast<t_time>(-604800), time_floor_to_week(-1));
    return (1);
}

FT_TEST(test_time_ceiling_helpers_round_up_values)
{
    FT_ASSERT_EQ(static_cast<t_time>(0), time_ceiling_to_second(0));
    FT_ASSERT_EQ(static_cast<t_time>(60), time_ceiling_to_minute(1));
    FT_ASSERT_EQ(static_cast<t_time>(60), time_ceiling_to_minute(60));
    FT_ASSERT_EQ(static_cast<t_time>(3600), time_ceiling_to_hour(1));
    FT_ASSERT_EQ(static_cast<t_time>(86400), time_ceiling_to_day(1));
    FT_ASSERT_EQ(static_cast<t_time>(604800), time_ceiling_to_week(1));
    FT_ASSERT_EQ(static_cast<t_time>(0), time_ceiling_to_minute(-1));
    FT_ASSERT_EQ(static_cast<t_time>(0), time_ceiling_to_hour(-1));
    return (1);
}

FT_TEST(test_time_month_floor_and_ceiling_are_calendar_aware)
{
    t_time march_first;
    t_time march_last_second;
    t_time leap_day_midday;

    march_first = 1709251200;
    march_last_second = 1711929599;
    leap_day_midday = 1709208000;
    FT_ASSERT_EQ(march_first, time_floor_to_month(march_last_second));
    FT_ASSERT_EQ(static_cast<t_time>(1711929600), time_ceiling_to_month(march_last_second));
    FT_ASSERT_EQ(static_cast<t_time>(1706745600), time_floor_to_month(leap_day_midday));
    FT_ASSERT_EQ(static_cast<t_time>(1709251200), time_ceiling_to_month(leap_day_midday));
    return (1);
}

FT_TEST(test_time_quarter_floor_and_ceiling_are_calendar_aware)
{
    t_time quarter_start;
    t_time quarter_middle;
    t_time quarter_boundary;
    t_time expected_floor;
    t_time expected_ceiling;
    std::tm dummy_time;

    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-04-01T00:00:00Z", &dummy_time, &quarter_start));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-05-15T12:34:56Z", &dummy_time, &quarter_middle));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-06-30T23:59:59Z", &dummy_time, &quarter_boundary));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-04-01T00:00:00Z", &dummy_time, &expected_floor));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-07-01T00:00:00Z", &dummy_time, &expected_ceiling));
    FT_ASSERT_EQ(expected_floor, time_floor_to_quarter(quarter_start));
    FT_ASSERT_EQ(expected_floor, time_floor_to_quarter(quarter_middle));
    FT_ASSERT_EQ(expected_floor, time_floor_to_quarter(quarter_boundary));
    FT_ASSERT_EQ(expected_floor, time_ceiling_to_quarter(quarter_start));
    FT_ASSERT_EQ(expected_ceiling, time_ceiling_to_quarter(quarter_middle));
    FT_ASSERT_EQ(expected_ceiling, time_ceiling_to_quarter(quarter_boundary));
    return (1);
}

FT_TEST(test_time_add_months_clamps_month_end_and_preserves_time_of_day)
{
    t_time january_timestamp;
    t_time leap_year_timestamp;
    t_time reverse_timestamp;
    t_time expected_timestamp;
    t_time expected_leap_timestamp;
    t_time expected_reverse_timestamp;
    std::tm dummy_time;

    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2023-01-31T12:34:56Z", &dummy_time, &january_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-01-31T12:34:56Z", &dummy_time, &leap_year_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-03-31T12:34:56Z", &dummy_time, &reverse_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2023-02-28T12:34:56Z", &dummy_time, &expected_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-02-29T12:34:56Z", &dummy_time, &expected_leap_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-02-29T12:34:56Z", &dummy_time, &expected_reverse_timestamp));
    FT_ASSERT_EQ(expected_timestamp, time_add_months(january_timestamp, 1));
    FT_ASSERT_EQ(expected_leap_timestamp, time_add_months(leap_year_timestamp, 1));
    FT_ASSERT_EQ(expected_reverse_timestamp, time_add_months(reverse_timestamp, -1));
    return (1);
}

FT_TEST(test_time_add_quarters_clamps_month_end_and_preserves_time_of_day)
{
    t_time january_timestamp;
    t_time reverse_timestamp;
    t_time middle_timestamp;
    t_time expected_timestamp;
    t_time expected_reverse_timestamp;
    t_time expected_middle_timestamp;
    std::tm dummy_time;

    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-01-31T12:34:56Z", &dummy_time, &january_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-07-31T12:34:56Z", &dummy_time, &reverse_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-05-15T12:34:56Z", &dummy_time, &middle_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-04-30T12:34:56Z", &dummy_time, &expected_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-04-30T12:34:56Z", &dummy_time, &expected_reverse_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-08-15T12:34:56Z", &dummy_time, &expected_middle_timestamp));
    FT_ASSERT_EQ(expected_timestamp, time_add_quarters(january_timestamp, 1));
    FT_ASSERT_EQ(expected_reverse_timestamp, time_add_quarters(reverse_timestamp, -1));
    FT_ASSERT_EQ(expected_middle_timestamp, time_add_quarters(middle_timestamp, 1));
    return (1);
}

FT_TEST(test_time_add_years_clamps_leap_day_and_preserves_time_of_day)
{
    t_time leap_day_timestamp;
    t_time non_leap_timestamp;
    t_time reverse_timestamp;
    t_time expected_leap_shift;
    t_time expected_non_leap_shift;
    t_time expected_reverse_shift;
    std::tm dummy_time;

    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2020-02-29T12:34:56Z", &dummy_time, &leap_day_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2021-02-28T12:34:56Z", &dummy_time, &expected_leap_shift));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2023-01-31T12:34:56Z", &dummy_time, &non_leap_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2024-01-31T12:34:56Z", &dummy_time, &reverse_timestamp));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2022-01-31T12:34:56Z", &dummy_time, &expected_non_leap_shift));
    FT_ASSERT_EQ(FT_TRUE, time_parse_rfc3339("2023-01-31T12:34:56Z", &dummy_time, &expected_reverse_shift));
    FT_ASSERT_EQ(expected_leap_shift, time_add_years(leap_day_timestamp, 1));
    FT_ASSERT_EQ(expected_non_leap_shift, time_add_years(non_leap_timestamp, -1));
    FT_ASSERT_EQ(expected_reverse_shift, time_add_years(reverse_timestamp, -1));
    return (1);
}

FT_TEST(test_time_add_days_clamps_at_bounds)
{
    t_time maximum_time;
    t_time minimum_time;

    maximum_time = std::numeric_limits<t_time>::max();
    minimum_time = std::numeric_limits<t_time>::min();
    FT_ASSERT_EQ(maximum_time, time_add_days(maximum_time, 1));
    FT_ASSERT_EQ(minimum_time, time_add_days(minimum_time, -1));
    return (1);
}

FT_TEST(test_time_rounding_to_minutes_handles_minimum_and_maximum)
{
    const int64_t minimum_whole_minute = INT64_MIN - (INT64_MIN % 60);
    const int64_t maximum_whole_minute = INT64_MAX - (INT64_MAX % 60);
    FT_ASSERT_EQ(INT64_MIN, time_floor_to_minute(INT64_MIN));
    FT_ASSERT_EQ(minimum_whole_minute, time_ceiling_to_minute(INT64_MIN));
    FT_ASSERT_EQ(maximum_whole_minute, time_floor_to_minute(INT64_MAX));
    FT_ASSERT_EQ(INT64_MAX, time_ceiling_to_minute(INT64_MAX));
    FT_ASSERT_EQ(-60, time_floor_to_minute(-1));
    FT_ASSERT_EQ(0, time_ceiling_to_minute(-1));
    return (1);
}

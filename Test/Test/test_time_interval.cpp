#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_time_format_interval_uses_rfc3339_endpoints)
{
    ft_string *interval_string;

    interval_string = time_format_interval(0, 3600);
    FT_ASSERT(interval_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interval_string->get_error());
    FT_ASSERT_STR_EQ("1970-01-01T00:00:00Z/1970-01-01T01:00:00Z", interval_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, interval_string->destroy());
    delete interval_string;
    return (1);
}

FT_TEST(test_time_parse_interval_start_and_end_round_trip_duration)
{
    t_time start_time;
    t_time end_time;
    t_duration_milliseconds duration;
    ft_bool parse_result;

    start_time = 0;
    end_time = 0;
    duration = time_duration_ms_create(0);
    parse_result = time_parse_interval("1970-01-01T00:00:00Z/1970-01-01T01:00:00Z",
            &start_time, &end_time, &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<t_time>(0), start_time);
    FT_ASSERT_EQ(static_cast<t_time>(3600), end_time);
    FT_ASSERT_EQ(static_cast<int64_t>(3600000), duration.milliseconds);
    return (1);
}

FT_TEST(test_time_parse_interval_supports_duration_on_either_side)
{
    t_time start_time;
    t_time end_time;
    t_duration_milliseconds duration;
    ft_bool parse_result;

    start_time = 0;
    end_time = 0;
    duration = time_duration_ms_create(0);

    parse_result = time_parse_interval("1970-01-01T00:00:00Z/PT2H",
            &start_time, &end_time, &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<t_time>(0), start_time);
    FT_ASSERT_EQ(static_cast<t_time>(7200), end_time);
    FT_ASSERT_EQ(static_cast<int64_t>(7200000), duration.milliseconds);

    parse_result = time_parse_interval("PT2H/1970-01-01T02:00:00Z",
            &start_time, &end_time, &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<t_time>(0), start_time);
    FT_ASSERT_EQ(static_cast<t_time>(7200), end_time);
    FT_ASSERT_EQ(static_cast<int64_t>(7200000), duration.milliseconds);
    return (1);
}

FT_TEST(test_time_parse_interval_rejects_subsecond_duration_offsets)
{
    t_time start_time;
    t_time end_time;
    ft_bool parse_result;

    start_time = 0;
    end_time = 0;
    parse_result = time_parse_interval("1970-01-01T00:00:00Z/PT1.5S",
            &start_time, &end_time, ft_nullptr);
    FT_ASSERT_EQ(FT_FALSE, parse_result);
    return (1);
}

FT_TEST(test_time_parse_interval_accepts_long_left_component)
{
    const char input[] = "PT0000000000000000000000000000000000000000"
        "00000000000000000000000000002H/1970-01-01T02:00:00Z";
    t_time start_time = -1;
    t_time end_time = -1;
    t_duration_milliseconds duration = time_duration_ms_create(0);
    FT_ASSERT_EQ(FT_TRUE, time_parse_interval(input, &start_time, &end_time,
            &duration));
    FT_ASSERT_EQ(0, start_time);
    FT_ASSERT_EQ(7200, end_time);
    FT_ASSERT_EQ(7200000, duration.milliseconds);
    return (1);
}

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

FT_TEST(test_time_format_rfc3339_uses_z_for_utc)
{
    ft_string *result_string;

    result_string = time_format_rfc3339(0);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ("1970-01-01T00:00:00Z", result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_format_rfc3339_with_offset_matches_offset_suffix)
{
    ft_string expected_string;
    ft_string *result_string;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_string.initialize("1970-01-01T05:30:00+05:30"));
    result_string = time_format_rfc3339_with_offset(0, 330);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ(expected_string.c_str(), result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_parse_rfc3339_accepts_z_and_offsets)
{
    std::tm base_time;
    std::tm offset_time;
    t_time base_timestamp;
    t_time offset_timestamp;
    ft_bool parse_result;

    ft_memset(&base_time, 0, sizeof(base_time));
    ft_memset(&offset_time, 0, sizeof(offset_time));
    base_timestamp = 0;
    offset_timestamp = 0;
    parse_result = time_parse_rfc3339("2024-03-01T12:34:56Z", &base_time, &base_timestamp);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    parse_result = time_parse_rfc3339("2024-03-01T15:04:56+02:30", &offset_time, &offset_timestamp);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(base_timestamp, offset_timestamp);
    FT_ASSERT_EQ(base_time.tm_year, offset_time.tm_year);
    FT_ASSERT_EQ(base_time.tm_mon, offset_time.tm_mon);
    FT_ASSERT_EQ(base_time.tm_mday, offset_time.tm_mday);
    FT_ASSERT_EQ(base_time.tm_hour, offset_time.tm_hour);
    FT_ASSERT_EQ(base_time.tm_min, offset_time.tm_min);
    FT_ASSERT_EQ(base_time.tm_sec, offset_time.tm_sec);
    return (1);
}

FT_TEST(test_time_parse_timezone_offset_formats_minutes)
{
    int32_t offset_minutes;
    ft_bool parse_result;

    offset_minutes = 0;
    parse_result = time_parse_timezone_offset("+05:30", &offset_minutes);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(330, offset_minutes);
    parse_result = time_parse_timezone_offset("Z", &offset_minutes);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(0, offset_minutes);
    return (1);
}

FT_TEST(test_time_parse_duration_supports_fractional_seconds)
{
    t_duration_milliseconds duration;
    ft_bool parse_result;

    parse_result = time_parse_duration("PT1H30M15.250S", &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<int64_t>(5415250), duration.milliseconds);
    parse_result = time_parse_duration("-PT2M", &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<int64_t>(-120000), duration.milliseconds);
    return (1);
}

FT_TEST(test_time_parse_duration_rejects_unsupported_calendar_units)
{
    t_duration_milliseconds duration;
    ft_bool parse_result;

    parse_result = time_parse_duration("P1Y", &duration);
    FT_ASSERT_EQ(FT_FALSE, parse_result);
    parse_result = time_parse_duration("PT", &duration);
    FT_ASSERT_EQ(FT_FALSE, parse_result);
    return (1);
}

FT_TEST(test_time_format_duration_round_trips_with_parse)
{
    t_duration_milliseconds duration;
    ft_string expected_string;
    ft_string *result_string;
    ft_bool parse_result;
    const char *formatted_text;

    duration = time_duration_ms_create(5415250);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_string.initialize("PT1H30M15.250S"));
    result_string = time_format_duration(duration);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ(expected_string.c_str(), result_string->c_str());
    formatted_text = result_string->c_str();
    parse_result = time_parse_duration(formatted_text, &duration);
    FT_ASSERT_EQ(FT_TRUE, parse_result);
    FT_ASSERT_EQ(static_cast<int64_t>(5415250), duration.milliseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_format_duration_handles_zero_and_negative_values)
{
    t_duration_milliseconds duration;
    ft_string *result_string;

    duration = time_duration_ms_create(0);
    result_string = time_format_duration(duration);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_STR_EQ("PT0S", result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;

    duration = time_duration_ms_create(-120000);
    result_string = time_format_duration(duration);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_STR_EQ("-PT2M", result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_format_duration_preserves_extreme_values_and_fraction_width)
{
    ft_string *minimum = time_format_duration(time_duration_ms_create(INT64_MIN));
    FT_ASSERT(minimum != ft_nullptr);
    FT_ASSERT_STR_EQ("-P106751991167DT7H12M55.808S", minimum->c_str());
    delete minimum;
    ft_string *one_ms = time_format_relative(time_duration_ms_create(1));
    FT_ASSERT(one_ms != ft_nullptr);
    FT_ASSERT_STR_EQ("in PT0.001S", one_ms->c_str());
    delete one_ms;
    return (1);
}

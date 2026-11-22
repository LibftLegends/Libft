#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstddef>
#include <ctime>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_time_format_iso8601_with_offset_zero)
{
    ft_string expected_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_string.initialize("1970-01-01T00:00:00+00:00"));
    ft_string *result_string;

    result_string = time_format_iso8601_with_offset(0, 0);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ(expected_string.c_str(), result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_format_iso8601_with_offset_positive)
{
    ft_string expected_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_string.initialize("1970-01-01T05:30:00+05:30"));
    ft_string *result_string;

    result_string = time_format_iso8601_with_offset(0, 330);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ(expected_string.c_str(), result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_format_iso8601_with_offset_negative)
{
    ft_string expected_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_string.initialize("1969-12-31T16:00:00-08:00"));
    ft_string *result_string;

    result_string = time_format_iso8601_with_offset(0, -480);
    FT_ASSERT(result_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->get_error());
    FT_ASSERT_STR_EQ(expected_string.c_str(), result_string->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result_string->destroy());
    delete result_string;
    return (1);
}

FT_TEST(test_time_convert_timezone_success)
{
    t_time converted_time;
    bool conversion_result;

    conversion_result = time_convert_timezone(3600, 120, -300, &converted_time);
    FT_ASSERT(conversion_result);
    FT_ASSERT_EQ(static_cast<t_time>(-21600), converted_time);
    return (1);
}

FT_TEST(test_time_convert_timezone_invalid_argument)
{
    bool conversion_result;

    conversion_result = time_convert_timezone(0, 0, 0, NULL);
    FT_ASSERT_EQ(false, conversion_result);
    return (1);
}

FT_TEST(test_time_get_local_offset_requires_output)
{
    bool offset_result;

    offset_result = time_get_local_offset(0, NULL, NULL);
    FT_ASSERT_EQ(false, offset_result);
    return (1);
}

FT_TEST(test_time_timezone_local_offset_matches_the_local_calendar)
{
    constexpr t_time TIMESTAMP = 1719835200;
    int32_t offset = 0;
    ft_bool daylight_saving = FT_FALSE;
    FT_ASSERT(time_get_local_offset(TIMESTAMP, &offset, &daylight_saving));
    t_time_info local{};
    time_local(TIMESTAMP, &local);
    FT_ASSERT_EQ(local.is_daylight_saving > 0, daylight_saving);
    char calendar[24];
    FT_ASSERT_EQ(19U, time_strftime(calendar, sizeof(calendar), "%Y-%m-%dT%H:%M:%S", &local));
    ft_string *formatted = time_format_iso8601_with_offset(TIMESTAMP, offset);
    FT_ASSERT(formatted != ft_nullptr);
    const int32_t difference = ft_strncmp(calendar, formatted->c_str(), 19U);
    delete formatted;
    FT_ASSERT_EQ(0, difference);
    return (1);
}

FT_TEST(test_time_timezone_conversion_reports_overflow_without_writing)
{
    t_time converted = 17;
    FT_ASSERT_EQ(FT_FALSE, time_convert_timezone(INT64_MAX, 0, 1, &converted));
    FT_ASSERT_EQ(17, converted);
    FT_ASSERT_EQ(FT_FALSE, time_convert_timezone(INT64_MIN, 1, 0, &converted));
    FT_ASSERT_EQ(17, converted);
    FT_ASSERT_EQ(FT_TRUE, time_convert_timezone(INT64_MIN, 1, 1, &converted));
    FT_ASSERT_EQ(INT64_MIN, converted);
    return (1);
}

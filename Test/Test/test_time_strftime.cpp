#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static t_time_info create_sample_time_info(void)
{
    t_time_info sample_time_info;

    sample_time_info.mutex = ft_nullptr;
    sample_time_info.thread_safe_enabled = false;
    sample_time_info.seconds = 0;
    sample_time_info.minutes = 0;
    sample_time_info.hours = 0;
    sample_time_info.month_day = 1;
    sample_time_info.month = 0;
    sample_time_info.year = 0;
    sample_time_info.week_day = 0;
    sample_time_info.year_day = 0;
    sample_time_info.is_daylight_saving = 0;
    return (sample_time_info);
}

FT_TEST(test_time_strftime_null_buffer_sets_errno)
{
    t_time_info sample_time_info;

    sample_time_info = create_sample_time_info();
    size_t format_result = time_strftime(static_cast<char *>(ft_nullptr), 16, "%Y", &sample_time_info);
    FT_ASSERT_EQ(static_cast<size_t>(0), format_result);
    return (1);
}

FT_TEST(test_time_strftime_zero_size_sets_errno)
{
    char buffer[8];
    t_time_info sample_time_info;

    buffer[0] = 'X';
    sample_time_info = create_sample_time_info();
    size_t format_result = time_strftime(buffer, 0, "%Y", &sample_time_info);
    FT_ASSERT_EQ(static_cast<size_t>(0), format_result);
    FT_ASSERT_EQ('X', buffer[0]);
    return (1);
}

FT_TEST(test_time_strftime_null_format_sets_errno)
{
    char buffer[8];
    t_time_info sample_time_info;

    buffer[0] = 'X';
    sample_time_info = create_sample_time_info();
    size_t format_result = time_strftime(buffer, sizeof(buffer), static_cast<const char *>(ft_nullptr), &sample_time_info);
    FT_ASSERT_EQ(static_cast<size_t>(0), format_result);
    FT_ASSERT_EQ('X', buffer[0]);
    return (1);
}

FT_TEST(test_time_strftime_success_resets_errno)
{
    char buffer[32];
    t_time_info sample_time_info;
    const char  *expected_string;
    size_t  expected_length;

    sample_time_info = create_sample_time_info();
    sample_time_info.year = 124;
    sample_time_info.month = 10;
    sample_time_info.month_day = 5;
    size_t format_result = time_strftime(buffer, sizeof(buffer), "%Y-%m-%d", &sample_time_info);
    expected_string = "2024-11-05";
    expected_length = ft_strlen(expected_string);
    FT_ASSERT_EQ(expected_length, format_result);
    FT_ASSERT_EQ(0, ft_strcmp(expected_string, buffer));
    return (1);
}

FT_TEST(test_time_strftime_preserves_partial_fields_and_unknown_conversions)
{
    char buffer[12];
    t_time_info time_info = create_sample_time_info();
    time_info.year = 124;
    time_info.month = 10;
    time_info.month_day = 5;
    FT_ASSERT_EQ(5U, time_strftime(buffer, 6U, "%Y%m", &time_info));
    FT_ASSERT_STR_EQ("20241", buffer);
    FT_ASSERT_EQ(7U, time_strftime(buffer, sizeof(buffer), "%q %% %d%", &time_info));
    FT_ASSERT_STR_EQ("q % 05%", buffer);
    return (1);
}

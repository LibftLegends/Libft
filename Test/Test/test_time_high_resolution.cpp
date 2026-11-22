#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_time_high_resolution_now_requires_output)
{
    bool    result;

    result = time_high_resolution_now(ft_nullptr);
    FT_ASSERT(result == false);
    return (1);
}

FT_TEST(test_time_high_resolution_now_reports_success)
{
    t_high_resolution_time_point first_point;
    bool                         result;

    result = time_high_resolution_now(&first_point);
    FT_ASSERT(result == true);
    return (1);
}

FT_TEST(test_time_monotonic_scalar_and_point_share_the_same_clock)
{
    const int64_t before = time_monotonic();
    const t_monotonic_time_point point = time_monotonic_point_now();
    const int64_t after = time_monotonic();
    FT_ASSERT(before <= point.milliseconds && point.milliseconds <= after);
    FT_ASSERT(point.mutex == ft_nullptr);
    FT_ASSERT_EQ(FT_FALSE, point.thread_safe_enabled);
    return (1);
}

FT_TEST(test_time_high_resolution_diff_seconds_tracks_elapsed_time)
{
    t_high_resolution_time_point start_point;
    t_high_resolution_time_point end_point;
    long long                    elapsed_ns;
    double                       elapsed_seconds;

    FT_ASSERT(time_high_resolution_now(&start_point) == true);
    time_sleep_ms(2);
    FT_ASSERT(time_high_resolution_now(&end_point) == true);
    elapsed_ns = time_high_resolution_diff_ns(start_point, end_point);
    FT_ASSERT(elapsed_ns >= 0);
    elapsed_seconds = time_high_resolution_diff_seconds(start_point, end_point);
    FT_ASSERT(elapsed_seconds >= 0.0);
    FT_ASSERT(elapsed_seconds >= static_cast<double>(elapsed_ns) / 1000000000.0 - 1e-9);
    return (1);
}

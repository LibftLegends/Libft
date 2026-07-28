#include "../test_internal.hpp"
#include "../../Modules/Time/time_timer.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <climits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_time_timer_start_rejects_negative_duration)
{
    time_timer timer;
    int64_t update_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(-5);
    update_result = timer.update();
    FT_ASSERT_EQ(static_cast<int64_t>(-1), update_result);
    return (1);
}

FT_TEST(test_time_timer_update_counts_down_to_completion)
{
    time_timer timer;
    int64_t remaining_before_wait;
    int64_t update_after_completion;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(25);
    remaining_before_wait = timer.update();
    FT_ASSERT(remaining_before_wait >= 0);
    FT_ASSERT(remaining_before_wait <= 25);

    time_sleep_ms(40);
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.update());

    update_after_completion = timer.update();
    FT_ASSERT_EQ(static_cast<int64_t>(-1), update_after_completion);
    return (1);
}

FT_TEST(test_time_timer_add_time_extends_active_timer)
{
    time_timer timer;
    int64_t remaining_after_add;
    int64_t remaining_midway;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(750);
    remaining_after_add = timer.add_time(60);
    FT_ASSERT(remaining_after_add > 0);
    FT_ASSERT(remaining_after_add <= 810);

    time_sleep_ms(350);
    remaining_midway = timer.update();
    FT_ASSERT(remaining_midway > 0);

    time_sleep_ms(500);
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.update());
    return (1);
}

FT_TEST(test_time_timer_add_time_detects_overflow)
{
    time_timer timer;
    int64_t add_result;
    int64_t remaining_after_failure;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(LONG_MAX - 10);
    add_result = timer.add_time(100);
    FT_ASSERT_EQ(static_cast<int64_t>(-1), add_result);

    remaining_after_failure = timer.update();
    FT_ASSERT(remaining_after_failure > 0);
    return (1);
}

FT_TEST(test_time_timer_add_time_requires_running_timer)
{
    time_timer timer;
    int64_t add_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    add_result = timer.add_time(5);
    FT_ASSERT_EQ(static_cast<int64_t>(-1), add_result);
    return (1);
}

FT_TEST(test_time_timer_remove_time_can_force_expiration)
{
    time_timer timer;
    int64_t update_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(50);
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.remove_time(100));

    update_result = timer.update();
    FT_ASSERT_EQ(static_cast<int64_t>(-1), update_result);
    return (1);
}

FT_TEST(test_time_timer_remove_time_requires_running_timer)
{
    time_timer timer;
    int64_t remove_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    remove_result = timer.remove_time(10);
    FT_ASSERT_EQ(static_cast<int64_t>(-1), remove_result);

    timer.start(20);
    remove_result = timer.remove_time(-5);
    FT_ASSERT_EQ(static_cast<int64_t>(-1), remove_result);
    return (1);
}

FT_TEST(test_time_timer_sleep_remaining_waits_until_finished)
{
    time_timer timer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(25);
    timer.sleep_remaining();
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.update());
    return (1);
}

FT_TEST(test_time_timer_sleep_remaining_sets_error_on_inactive_timer)
{
    time_timer timer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.sleep_remaining();
    FT_ASSERT_EQ(static_cast<int64_t>(-1), timer.update());
    return (1);
}

FT_TEST(test_time_timer_restart_after_completion)
{
    time_timer timer;
    int64_t result_after_restart;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, timer.initialize());
    timer.start(10);
    time_sleep_ms(20);
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.update());

    timer.start(30);
    result_after_restart = timer.update();
    FT_ASSERT(result_after_restart > 0);

    time_sleep_ms(40);
    FT_ASSERT_EQ(static_cast<int64_t>(0), timer.update());
    return (1);
}

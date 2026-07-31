#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_bool active_clock_elapsed_at_least(uint64_t elapsed_microseconds,
    uint64_t minimum_microseconds)
{
    if (elapsed_microseconds < minimum_microseconds)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_time_active_clock_start_and_report_microseconds)
{
    t_active_clock clock;
    uint64_t elapsed_microseconds;

    time_active_clock_init(&clock);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_start(&clock));
    time_sleep_ms(10);
    elapsed_microseconds = time_active_clock_report(&clock);
    FT_ASSERT_EQ(FT_TRUE, active_clock_elapsed_at_least(elapsed_microseconds,
        5000));
    FT_ASSERT_EQ(FT_TRUE, clock.running);
    FT_ASSERT_EQ(FT_FALSE, time_active_clock_start(ft_nullptr));
    return (1);
}

FT_TEST(test_time_active_clock_stop_and_resume_excludes_pause)
{
    t_active_clock clock;
    uint64_t elapsed_before_stop;
    uint64_t elapsed_while_stopped;
    uint64_t elapsed_after_resume;

    time_active_clock_init(&clock);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_start(&clock));
    time_sleep_ms(8);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_stop(&clock));
    elapsed_before_stop = time_active_clock_report(&clock);
    time_sleep_ms(20);
    elapsed_while_stopped = time_active_clock_report(&clock);
    FT_ASSERT_EQ(elapsed_before_stop, elapsed_while_stopped);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_resume(&clock));
    time_sleep_ms(8);
    elapsed_after_resume = time_active_clock_report(&clock);
    FT_ASSERT_EQ(FT_TRUE, active_clock_elapsed_at_least(
        elapsed_after_resume, elapsed_before_stop + 4000));
    return (1);
}

FT_TEST(test_time_active_clock_restart_and_independent_clocks)
{
    t_active_clock first_clock;
    t_active_clock second_clock;
    uint64_t first_before_restart;
    uint64_t first_after_restart;
    uint64_t second_elapsed;

    time_active_clock_init(&first_clock);
    time_active_clock_init(&second_clock);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_start(&first_clock));
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_start(&second_clock));
    time_sleep_ms(8);
    first_before_restart = time_active_clock_report(&first_clock);
    FT_ASSERT_EQ(FT_TRUE, time_active_clock_restart(&first_clock));
    first_after_restart = time_active_clock_report(&first_clock);
    second_elapsed = time_active_clock_report(&second_clock);
    FT_ASSERT_EQ(FT_TRUE, first_before_restart >= 4000);
    FT_ASSERT_EQ(FT_TRUE, first_after_restart < first_before_restart);
    FT_ASSERT_EQ(FT_TRUE, second_elapsed >= 4000);
    return (1);
}

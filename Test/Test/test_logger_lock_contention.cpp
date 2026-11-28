#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <chrono>
#include <limits>
#include <thread>

FT_TEST(test_logger_contention_samples_idle_registry)
{
    s_log_lock_contention_sample sample;
    ft_size_t count = 1U;
    ft_log_enable_lock_contention_sampling(FT_FALSE);
    ft_log_enable_lock_contention_sampling(FT_TRUE);
    const int32_t error = ft_log_sample_lock_contention(&sample, 1U, &count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error);
    FT_ASSERT_EQ(0U, count);
    return (1);
}

FT_TEST(test_logger_contention_counts_waiters_that_exceed_output_capacity)
{
    pt_mutex mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_enter(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.lock());
    const auto wait_for_mutex = [&mutex]()
    {
        if (mutex.lock() == FT_ERR_SUCCESS)
            (void)mutex.unlock();
        return ;
    };
    std::thread first(wait_for_mutex);
    std::thread second(wait_for_mutex);
    ft_log_set_lock_contention_priority_threshold(0);
    ft_log_set_lock_contention_sampling_interval(0U);
    s_log_lock_contention_sample sample;
    s_log_lock_contention_statistics statistics;
    ft_size_t count = 0U;
    int32_t sample_error;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do
    {
        ft_log_lock_contention_reset_statistics();
        sample_error = ft_log_sample_lock_contention(&sample, 1U, &count);
        (void)ft_log_lock_contention_get_statistics(&statistics);
        if (sample_error != FT_ERR_SUCCESS || statistics.total_samples >= 2U)
            break ;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (std::chrono::steady_clock::now() < deadline);

    ft_log_set_lock_contention_sampling_interval(std::numeric_limits<uint32_t>::max());
    ft_size_t throttled_count = 1U;
    const int32_t throttled_error = ft_log_sample_lock_contention(&sample, 1U, &throttled_count);
    (void)mutex.unlock();
    first.join();
    second.join();
    const int32_t exit_error = pt_lock_tracking::notify_thread_exit(THREAD_ID);
    ft_log_enable_lock_contention_sampling(FT_FALSE);
    ft_log_enable_lock_contention_sampling(FT_TRUE);
    ft_log_set_lock_contention_sampling_interval(250U);
    ft_log_set_lock_contention_priority_threshold(500);
    ft_log_lock_contention_reset_statistics();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sample_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, throttled_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exit_error);
    FT_ASSERT_EQ(0U, throttled_count);
    FT_ASSERT_EQ(1U, count);
    FT_ASSERT_EQ(static_cast<const void *>(&mutex), sample.mutex_pointer);
    FT_ASSERT_EQ(THREAD_ID, sample.owner_thread);
    FT_ASSERT(sample.waiting_thread != 0);
    FT_ASSERT_EQ(FT_TRUE, sample.priority_inversion);
    FT_ASSERT_EQ(2U, statistics.total_samples);
    FT_ASSERT_EQ(1U, statistics.skipped_samples);
    FT_ASSERT_EQ(2U, statistics.priority_inversions);
    FT_ASSERT(statistics.average_wait_ms <= static_cast<double>(statistics.longest_wait_ms));
    return (1);
}

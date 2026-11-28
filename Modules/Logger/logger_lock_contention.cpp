#include "logger_internal.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_lock_tracking.hpp"
#include "../Time/time.hpp"

struct s_logger_waiter_snapshot
{
    pt_lock_wait_snapshot_vector waiters;

    s_logger_waiter_snapshot() : waiters{}
    {
        return ;
    }

    ~s_logger_waiter_snapshot()
    {
        pt_buffer_destroy(this->waiters);
        return ;
    }
};

static pthread_mutex_t g_lock_contention_mutex = PTHREAD_MUTEX_INITIALIZER;
static s_logger_waiter_snapshot g_lock_contention_snapshot;
static ft_bool g_lock_contention_sampling_enabled = FT_TRUE;
static uint32_t g_lock_contention_sample_interval_ms = 250U;
static int64_t g_lock_contention_last_sample_ms = 0;
static int64_t g_lock_contention_priority_threshold_ms = 500;
static ft_size_t g_lock_contention_total_samples = 0U;
static ft_size_t g_lock_contention_priority_inversions = 0U;
static ft_size_t g_lock_contention_skipped_samples = 0U;
static double g_lock_contention_wait_total_ms = 0.0;
static int64_t g_lock_contention_longest_wait_ms = 0;

void ft_log_enable_lock_contention_sampling(ft_bool enable)
{
    if (pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return ;
    g_lock_contention_sampling_enabled = enable;
    if (enable == FT_FALSE)
    {
        g_lock_contention_last_sample_ms = 0;
        pt_buffer_destroy(g_lock_contention_snapshot.waiters);
    }
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return ;
}

void ft_log_set_lock_contention_sampling_interval(uint32_t interval_ms)
{
    if (pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return ;
    g_lock_contention_sample_interval_ms = interval_ms;
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return ;
}

void ft_log_set_lock_contention_priority_threshold(int64_t threshold_ms)
{
    if (threshold_ms < 0)
        threshold_ms = 0;
    if (pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return ;
    g_lock_contention_priority_threshold_ms = threshold_ms;
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return ;
}

static void logger_lock_contention_collect(const pt_lock_wait_snapshot_vector &waiters,
    s_log_lock_contention_sample *samples, ft_size_t output_count)
{
    if (waiters.size == 0U)
        return ;
    const int64_t wall_now_ms = time_now_ms();
    ft_size_t index = 0U;
    double wait_total_ms = 0.0;
    int64_t longest_wait_ms = g_lock_contention_longest_wait_ms;
    ft_size_t priority_inversions = 0U;
    while (index < waiters.size)
    {
        const s_pt_lock_wait_snapshot &waiter = waiters.data[index];
        int64_t wait_duration_ms = 0;
        if (waiter.wait_started_ms > 0 && wall_now_ms >= waiter.wait_started_ms)
            wait_duration_ms = wall_now_ms - waiter.wait_started_ms;
        const ft_bool priority_inversion =
            wait_duration_ms >= g_lock_contention_priority_threshold_ms
            && waiter.owner_thread != 0 && waiter.waiting_thread != 0;
        if (index < output_count)
        {
            samples[index].mutex_pointer = waiter.mutex_pointer;
            samples[index].owner_thread = waiter.owner_thread;
            samples[index].waiting_thread = waiter.waiting_thread;
            samples[index].wait_duration_ms = wait_duration_ms;
            samples[index].priority_inversion = priority_inversion;
        }
        if (wait_duration_ms > longest_wait_ms)
            longest_wait_ms = wait_duration_ms;
        priority_inversions += priority_inversion;
        wait_total_ms += static_cast<double>(wait_duration_ms);
        index += 1U;
    }
    g_lock_contention_total_samples += waiters.size;
    g_lock_contention_skipped_samples += waiters.size - output_count;
    g_lock_contention_priority_inversions += priority_inversions;
    g_lock_contention_longest_wait_ms = longest_wait_ms;
    g_lock_contention_wait_total_ms += wait_total_ms;
    return ;
}

int32_t ft_log_sample_lock_contention(s_log_lock_contention_sample *samples,
    ft_size_t capacity, ft_size_t *entry_count)
{
    if (entry_count != ft_nullptr)
        *entry_count = 0U;
    if (samples == ft_nullptr || capacity == 0U)
        return (FT_ERR_INTERNAL);
    if (pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return (FT_ERR_INTERNAL);
    if (g_lock_contention_sampling_enabled == FT_FALSE)
    {
        (void)pthread_mutex_unlock(&g_lock_contention_mutex);
        return (FT_ERR_SUCCESS);
    }
    const int64_t now_ms = time_monotonic();
    if (g_lock_contention_last_sample_ms != 0 && g_lock_contention_sample_interval_ms > 0U
        && now_ms - g_lock_contention_last_sample_ms < g_lock_contention_sample_interval_ms)
    {
        (void)pthread_mutex_unlock(&g_lock_contention_mutex);
        return (FT_ERR_SUCCESS);
    }
    pt_lock_wait_snapshot_vector &waiters = g_lock_contention_snapshot.waiters;
    if (pt_lock_tracking::snapshot_waiters(waiters) != FT_ERR_SUCCESS)
    {
        (void)pthread_mutex_unlock(&g_lock_contention_mutex);
        return (FT_ERR_INTERNAL);
    }
    g_lock_contention_last_sample_ms = now_ms;
    ft_size_t output_count = waiters.size;
    if (output_count > capacity)
        output_count = capacity;
    logger_lock_contention_collect(waiters, samples, output_count);
    if (entry_count != ft_nullptr)
        *entry_count = output_count;
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_lock_contention_get_statistics(s_log_lock_contention_statistics *statistics)
{
    if (statistics == ft_nullptr || pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return (FT_ERR_INTERNAL);
    statistics->total_samples = g_lock_contention_total_samples;
    statistics->priority_inversions = g_lock_contention_priority_inversions;
    statistics->skipped_samples = g_lock_contention_skipped_samples;
    statistics->longest_wait_ms = g_lock_contention_longest_wait_ms;
    statistics->average_wait_ms = 0.0;
    if (g_lock_contention_total_samples > 0U)
        statistics->average_wait_ms = g_lock_contention_wait_total_ms
            / static_cast<double>(g_lock_contention_total_samples);
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_log_lock_contention_reset_statistics()
{
    if (pthread_mutex_lock(&g_lock_contention_mutex) != 0)
        return ;
    g_lock_contention_total_samples = 0U;
    g_lock_contention_priority_inversions = 0U;
    g_lock_contention_skipped_samples = 0U;
    g_lock_contention_wait_total_ms = 0.0;
    g_lock_contention_longest_wait_ms = 0;
    (void)pthread_mutex_unlock(&g_lock_contention_mutex);
    return ;
}

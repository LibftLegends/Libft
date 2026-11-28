#include "../test_internal.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include <cerrno>
#include <atomic>
#ifndef LIBFT_TEST_BUILD
#endif

static void *pthread_atomic_test_routine(void *argument)
{
    std::atomic<int> *started_flag;

    started_flag = static_cast<std::atomic<int>*>(argument);
    if (started_flag != ft_nullptr)
        started_flag->store(1, std::memory_order_release);
    return (ft_nullptr);
}

FT_TEST(test_pt_thread_create_updates_errno)
{
    pthread_t thread;
    int failure_result;
    int routine_started;

    routine_started = 0;
    failure_result = pt_thread_create(&thread, ft_nullptr, ft_nullptr, &routine_started);
    FT_ASSERT_EQ(EINVAL, failure_result);
    return (1);
}

FT_TEST(test_pt_thread_join_rejects_invalid_thread)
{
    pthread_t invalid_thread;
    int failure_result;

    invalid_thread = 0;
    failure_result = pt_thread_join(invalid_thread, ft_nullptr);
    FT_ASSERT(failure_result != 0);
    return (1);
}

#if defined(__linux__) || defined(_WIN32)
static void *pthread_timed_join_test_wait(void *argument)
{
    auto *released = static_cast<std::atomic<uint32_t> *>(argument);
    while (released->load(std::memory_order_acquire) == 0U)
        pt_thread_wait_uint32(released, 0U);
    return (argument);
}

FT_TEST(test_pthread_thread_timed_join_times_out_and_can_be_retried)
{
    std::atomic<uint32_t> released{0U};
    pthread_t thread;
    void *returned = ft_nullptr;
    FT_ASSERT_EQ(0, pt_thread_create(&thread, ft_nullptr,
            pthread_timed_join_test_wait, &released));
    const int32_t timeout = pt_thread_timed_join(thread, &returned, 10);
    released.store(1U, std::memory_order_release);
    (void)pt_thread_wake_one_uint32(&released);
    const int32_t retry = pt_thread_timed_join(thread, &returned, 2000);
    FT_ASSERT_EQ(ETIMEDOUT, timeout);
    FT_ASSERT_EQ(0, retry);
    FT_ASSERT_EQ(static_cast<void *>(&released), returned);
    return (1);
}

#endif

FT_TEST(test_pt_thread_detach_updates_errno)
{
    pthread_t thread;
    int failure_result;
    int detach_result;
    std::atomic<int> routine_started;

    failure_result = pt_thread_detach(static_cast<pthread_t>(0));
    FT_ASSERT(failure_result != 0);
    routine_started.store(0, std::memory_order_relaxed);
    FT_ASSERT_EQ(0, pt_thread_create(&thread, ft_nullptr,
        pthread_atomic_test_routine, &routine_started));
    detach_result = pt_thread_detach(thread);
    FT_ASSERT_EQ(0, detach_result);
    while (routine_started.load(std::memory_order_acquire) == 0)
        pt_thread_sleep(10);
    return (1);
}

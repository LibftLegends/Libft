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

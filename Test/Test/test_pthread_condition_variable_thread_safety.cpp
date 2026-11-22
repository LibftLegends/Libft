#include "../test_internal.hpp"
#include "../../Modules/PThread/condition.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <cerrno>
#include <limits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_pthread_condition_variable_rejects_overflowing_relative_deadline)
{
    pt_condition_variable condition;
    pt_mutex mutex;
    struct timespec duration{};
    duration.tv_sec = std::numeric_limits<decltype(duration.tv_sec)>::max();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, condition.wait_for(mutex, duration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.destroy());
    return (1);
}

FT_TEST(test_pt_condition_variable_state_guard_controls)
{
    pt_condition_variable condition;
    bool lock_acquired;

    FT_ASSERT(condition.is_thread_safe() == false);

    FT_ASSERT_EQ(0, condition.enable_thread_safety());
    FT_ASSERT(condition.is_thread_safe());

    lock_acquired = false;
    FT_ASSERT_EQ(0, condition.lock_state(&lock_acquired));
    FT_ASSERT(lock_acquired);
    condition.unlock_state(lock_acquired);

    condition.disable_thread_safety();
    FT_ASSERT(condition.is_thread_safe() == false);
    return (1);
}

FT_TEST(test_pt_condition_variable_state_guard_blocks)
{
    pt_condition_variable condition;
    bool main_lock_acquired;
    std::atomic<bool> worker_started;
    std::atomic<bool> worker_locked;
    std::atomic<bool> worker_finished;
    std::thread worker_thread;

    FT_ASSERT_EQ(0, condition.enable_thread_safety());
    FT_ASSERT(condition.is_thread_safe());

    main_lock_acquired = false;
    FT_ASSERT_EQ(0, condition.lock_state(&main_lock_acquired));
    FT_ASSERT(main_lock_acquired);

    worker_started.store(false);
    worker_locked.store(false);
    worker_finished.store(false);
    worker_thread = std::thread([&condition, &worker_started, &worker_locked, &worker_finished]() {
        bool worker_lock_acquired;

        worker_lock_acquired = false;
        worker_started.store(true);
        if (condition.lock_state(&worker_lock_acquired) != 0)
        {
            worker_locked.store(false);
            worker_finished.store(true);
            return ;
        }
        worker_locked.store(worker_lock_acquired);
        condition.unlock_state(worker_lock_acquired);
        worker_finished.store(true);
    });

    while (!worker_started.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    FT_ASSERT(worker_locked.load() == false);

    condition.unlock_state(main_lock_acquired);
    worker_thread.join();

    FT_ASSERT(worker_finished.load());
    FT_ASSERT(worker_locked.load());

    condition.disable_thread_safety();
    FT_ASSERT(condition.is_thread_safe() == false);
    return (1);
}

FT_TEST(test_pt_condition_variable_wait_for_times_out_with_thread_safety)
{
    pt_condition_variable condition;
    pt_mutex mutex_object;
    std::atomic<bool> worker_ready;
    std::atomic<int> wait_result;
    std::thread worker_thread;

    FT_ASSERT_EQ(0, condition.enable_thread_safety());
    FT_ASSERT(condition.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());

    worker_ready.store(false);
    wait_result.store(0);
    worker_thread = std::thread([&condition, &mutex_object, &worker_ready, &wait_result]() {
        struct timespec wait_duration;

        wait_duration.tv_sec = 0;
        wait_duration.tv_nsec = 2000000;
        int lock_result;

        lock_result = mutex_object.lock();
        if (lock_result != FT_ERR_SUCCESS)
        {
            wait_result.store(-1);
            worker_ready.store(true);
            return ;
        }
        worker_ready.store(true);
        int wait_call_result;

        wait_call_result = condition.wait_for(mutex_object, wait_duration);
        wait_result.store(wait_call_result);
        int unlock_result;

        unlock_result = mutex_object.unlock();
        if (unlock_result != FT_ERR_SUCCESS)
        {
            wait_result.store(-2);
            return ;
        }
    });

    while (!worker_ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    worker_thread.join();

    FT_ASSERT(wait_result.load() == ETIMEDOUT);

    condition.disable_thread_safety();
    FT_ASSERT(condition.is_thread_safe() == false);
    return (1);
}

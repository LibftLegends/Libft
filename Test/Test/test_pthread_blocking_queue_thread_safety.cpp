#include "../test_internal.hpp"
#include "../../Modules/Threading/task_scheduler.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_internal.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/queue.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <chrono>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_blocking_queue_enable_thread_safety_controls)
{
    ft_blocking_queue<int32_t> queue;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.initialize());
    FT_ASSERT_EQ(false, queue.is_thread_safe());
    FT_ASSERT_EQ(0, queue.enable_thread_safety());
    FT_ASSERT_EQ(true, queue.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.disable_thread_safety());
    FT_ASSERT_EQ(false, queue.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.destroy());
    return (1);
}

FT_TEST(test_ft_blocking_queue_lock_cycle_resets_errno)
{
    ft_blocking_queue<int32_t> queue;
    bool lock_acquired;
    int32_t value;

    lock_acquired = false;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.initialize());
    FT_ASSERT_EQ(0, queue.enable_thread_safety());
    FT_ASSERT_EQ(0, queue.lock(&lock_acquired));
    FT_ASSERT_EQ(true, lock_acquired);
    queue.unlock(lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.push(42));
    FT_ASSERT_EQ(true, queue.pop(value));
    FT_ASSERT_EQ(42, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.destroy());
    return (1);
}

FT_TEST(test_threading_blocking_queue_burst_wakes_multiple_waiters)
{
    ft_blocking_queue<int32_t> queue;
    std::atomic<bool> running{true};
    std::atomic<uint32_t> completed{0U};
    std::atomic<int32_t> sum{0};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.enable_thread_safety());
    const auto consume = [&]()
    {
        int32_t value = 0;
        if (queue.wait_pop(value, running))
        {
            sum.fetch_add(value);
            completed.fetch_add(1U);
        }
        return ;
    };
    std::thread first(consume);
    std::thread second(consume);
    const auto parked_by = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (queue._waiting_workers.load() != 2U && std::chrono::steady_clock::now() < parked_by)
        std::this_thread::yield();
    const ft_bool parked = queue._waiting_workers.load() == 2U;
    bool lock_acquired = false;
    const int32_t lock_error = queue.lock(&lock_acquired);
    const int32_t first_push = queue.push(40);
    const int32_t second_push = queue.push(2);
    queue.unlock(lock_acquired);
    const auto done_by = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (completed.load() != 2U && std::chrono::steady_clock::now() < done_by)
        std::this_thread::yield();
    const uint32_t naturally_completed = completed.load();
    running.store(false);
    queue.shutdown();
    first.join();
    second.join();
    FT_ASSERT_EQ(FT_TRUE, parked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lock_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_push);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_push);
    FT_ASSERT_EQ(2U, naturally_completed);
    FT_ASSERT_EQ(42, sum.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.destroy());
    return (1);
}

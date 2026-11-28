#include "../test_internal.hpp"
#include "../../Modules/Threading/task_scheduler.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include "../../Modules/Errno/errno.hpp"
#include <chrono>
#include <unistd.h>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_task_scheduler_cancel_after_handle)
{
    ft_task_scheduler scheduler_instance(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.initialize());
    {
        std::atomic<int> execution_count;

        execution_count.store(0);
        auto schedule_result = scheduler_instance.schedule_after(std::chrono::milliseconds(100),
            [&execution_count]()
        {
            execution_count.fetch_add(1);
            return ;
        });
        ft_future<void> delayed_future;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, delayed_future.move(schedule_result.key));
        ft_scheduled_task_handle handle_value = schedule_result.get_value();
        FT_ASSERT(handle_value.valid());
        (void)delayed_future;
        bool cancel_result;

        cancel_result = handle_value.cancel();
        FT_ASSERT(cancel_result);
        usleep(150000);
        int executed_times;

        executed_times = execution_count.load();
        FT_ASSERT_EQ(0, executed_times);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.destroy());
    }
    return (1);
}

FT_TEST(test_task_scheduler_cancel_periodic_handle)
{
    ft_task_scheduler scheduler_instance(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.initialize());
    {
        std::atomic<int> execution_count;

        execution_count.store(0);
        ft_scheduled_task_handle periodic_handle = scheduler_instance.schedule_every(std::chrono::milliseconds(120),
            [&execution_count]()
        {
            execution_count.fetch_add(1);
            return ;
        });
        FT_ASSERT(periodic_handle.valid());
        bool cancel_result;

        cancel_result = periodic_handle.cancel();
        FT_ASSERT(cancel_result);
        usleep(200000);
        int executed_times;

        executed_times = execution_count.load();
        FT_ASSERT_EQ(0, executed_times);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.destroy());
    }
    return (1);
}
FT_TEST(test_threading_scheduler_heap_keeps_payloads_ordered_after_removal)
{
    ft_task_scheduler scheduler;
    const int64_t deadlines[] = {50, 100, 60, 110, 120, 70, 80};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler._scheduled.initialize());
    ft_size_t index = 0U;
    while (index < sizeof(deadlines) / sizeof(deadlines[0]))
    {
        ft_task_scheduler::scheduled_task task;
        FT_ASSERT(task.is_initialised());
        task._time = time_monotonic_point_create(deadlines[index]);
        task._trace_id = static_cast<uint64_t>(deadlines[index]) + 1U;
        task._state.reset(new ft_scheduled_task_state());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, task._state.get_error());
        if (deadlines[index] == 70)
            task._state->cancel();
        FT_ASSERT(scheduler.scheduled_heap_push(ft_move(task)));
        ++index;
    }
    FT_ASSERT(scheduler.scheduled_remove_index(4U));
    FT_ASSERT(scheduler.scheduled_remove_index(0U));
    const int64_t expected[] = {60, 70, 80, 100, 110};
    index = 0U;
    while (index < sizeof(expected) / sizeof(expected[0]))
    {
        ft_task_scheduler::scheduled_task task;
        FT_ASSERT(scheduler.scheduled_heap_pop(task));
        FT_ASSERT_EQ(expected[index], task._time.milliseconds);
        FT_ASSERT_EQ(static_cast<uint64_t>(expected[index]) + 1U, task._trace_id);
        FT_ASSERT_EQ(1, task._state.use_count());
        FT_ASSERT_EQ(expected[index] == 70, task._state->is_cancelled());
        ++index;
    }
    FT_ASSERT_EQ(0U, scheduler._scheduled.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler._scheduled.destroy());
    return (1);
}

FT_TEST(test_task_scheduler_dispatches_timers_during_concurrent_cancellation)
{
    constexpr uint32_t TASK_COUNT = 128U;
    std::atomic<uint32_t> completed(0U);
    ft_task_scheduler scheduler(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_thread producer([&scheduler, &completed]()
    {
        uint32_t index = 0U;
        while (index < TASK_COUNT)
        {
            auto result = scheduler.schedule_after(std::chrono::milliseconds(1),
                [&completed]()
            {
                completed.fetch_add(1U);
                return ;
            });
            ++index;
        }
        return ;
    });
    FT_ASSERT(producer.joinable());
    ft_bool cancelled = FT_TRUE;
    uint32_t index = 0U;
    while (index < TASK_COUNT)
    {
        auto result = scheduler.schedule_after(std::chrono::seconds(30), []()
        {
            return ;
        });
        if (!result.get_value().cancel())
        {
            cancelled = FT_FALSE;
            break ;
        }
        ++index;
    }
    producer.join();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (completed.load() < TASK_COUNT && std::chrono::steady_clock::now() < deadline)
        usleep(1000);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.destroy());
    FT_ASSERT_EQ(FT_TRUE, cancelled);
    FT_ASSERT_EQ(TASK_COUNT, completed.load());
    return (1);
}

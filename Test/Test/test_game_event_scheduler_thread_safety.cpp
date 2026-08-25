#include "../test_internal.hpp"
#include "../../Modules/Game/game_event_scheduler.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Template/vector.hpp"
#include <new>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct scheduler_schedule_args
{
    game_event_scheduler *scheduler_pointer;
    ft_vector<ft_sharedptr<game_event> > *event_batch;
    int thread_index;
    int events_per_thread;
    int result_code;
};

static void *scheduler_schedule_task(void *argument)
{
    scheduler_schedule_args *arguments;
    int index;
    ft_sharedptr<game_event> event_instance;

    arguments = static_cast<scheduler_schedule_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->events_per_thread)
    {
        event_instance = (*(arguments->event_batch))[index];
        if (!event_instance)
        {
            arguments->result_code = FT_ERR_NO_MEMORY;
            return (ft_nullptr);
        }
        arguments->scheduler_pointer->schedule_event(event_instance);
        arguments->result_code = arguments->scheduler_pointer->get_error();
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_game_event_scheduler_concurrent_schedule)
{
    game_event_scheduler *scheduler_instance;
    pthread_t *threads;
    scheduler_schedule_args *arguments;
    ft_vector<ft_sharedptr<game_event> > event_batches[4];
    ft_sharedptr<game_event> event_instance;
    int thread_index;
    int create_result;
    int join_result;
    int events_per_thread;
    int expected_total;
    ft_vector<ft_sharedptr<game_event> > events;
    int created_thread_count;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    long join_timeout_ms;

    events_per_thread = 32;
    expected_total = 4 * events_per_thread;
    created_thread_count = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    join_timeout_ms = 5000;
    scheduler_instance = new (std::nothrow) game_event_scheduler();
    FT_ASSERT(scheduler_instance != ft_nullptr);
    threads = new (std::nothrow) pthread_t[4];
    FT_ASSERT(threads != ft_nullptr);
    arguments = new (std::nothrow) scheduler_schedule_args[4];
    FT_ASSERT(arguments != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance->enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    thread_index = 0;
    while (thread_index < 4)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event_batches[thread_index].initialize());
        int32_t event_index = 0;
        while (event_index < events_per_thread)
        {
            event_instance = ft_sharedptr<game_event>(new (std::nothrow)
                game_event());
            FT_ASSERT(event_instance);
            FT_ASSERT_EQ(FT_ERR_SUCCESS, event_instance->initialize());
            event_instance->set_id(thread_index * events_per_thread
                + event_index);
            event_instance->set_duration(event_index + 1);
            event_batches[thread_index].push_back(event_instance);
            FT_ASSERT_EQ(FT_ERR_SUCCESS,
                event_batches[thread_index].get_error());
            event_index += 1;
        }
        thread_index += 1;
    }
    thread_index = 0;
    while (thread_index < 4)
    {
        arguments[thread_index].scheduler_pointer = scheduler_instance;
        arguments[thread_index].event_batch = &event_batches[thread_index];
        arguments[thread_index].thread_index = thread_index;
        arguments[thread_index].events_per_thread = events_per_thread;
        arguments[thread_index].result_code = FT_ERR_SUCCESS;
        if (test_failed == 0)
        {
            create_result = pt_thread_create(&threads[thread_index], ft_nullptr,
                    scheduler_schedule_task, &arguments[thread_index]);
            if (create_result != 0 && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "create_result == 0";
                failure_line = __LINE__;
            }
            if (create_result == 0)
                created_thread_count += 1;
        }
        else
            threads[thread_index] = 0;
        thread_index += 1;
    }
    thread_index = 0;
    while (thread_index < created_thread_count)
    {
        join_result = pt_thread_timed_join(threads[thread_index], ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
        if (join_result == 0)
        {
            if (arguments[thread_index].result_code != FT_ERR_SUCCESS && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "arguments[thread_index].result_code == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
        }
        thread_index += 1;
    }
    if (test_failed != 0)
    {
        delete[] arguments;
        delete[] threads;
        delete scheduler_instance;
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    FT_ASSERT_EQ(static_cast<size_t>(expected_total), scheduler_instance->size());
    scheduler_instance->dump_events(events);
    FT_ASSERT_EQ(static_cast<size_t>(expected_total), events.size());
    delete[] arguments;
    delete[] threads;
    delete scheduler_instance;
    return (1);
}

struct scheduler_reschedule_args
{
    game_event_scheduler *scheduler_pointer;
    int event_identifier;
    int iteration_count;
};

static void *scheduler_reschedule_task(void *argument)
{
    scheduler_reschedule_args *arguments;

    arguments = static_cast<scheduler_reschedule_args *>(argument);
    if (arguments == ft_nullptr || arguments->scheduler_pointer == ft_nullptr)
        return (ft_nullptr);
    arguments->scheduler_pointer->reschedule_event(
        arguments->event_identifier,
        arguments->event_identifier + arguments->iteration_count);
    return (ft_nullptr);
}

FT_TEST(test_game_event_scheduler_concurrent_reschedule)
{
    game_event_scheduler *scheduler_instance;
    pthread_t *threads;
    scheduler_reschedule_args *arguments;
    int join_result;
    int preload_index;
    int create_result;
    int iteration_count;
    int created_thread_count;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    long join_timeout_ms;
    ft_vector<ft_sharedptr<game_event> > events;
    ft_size_t event_index;

    iteration_count = 64;
    created_thread_count = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    join_timeout_ms = 5000;
    scheduler_instance = new (std::nothrow) game_event_scheduler();
    FT_ASSERT(scheduler_instance != ft_nullptr);
    threads = new (std::nothrow) pthread_t[3];
    FT_ASSERT(threads != ft_nullptr);
    arguments = new (std::nothrow) scheduler_reschedule_args[3];
    FT_ASSERT(arguments != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    preload_index = 0;
    while (preload_index < 3)
    {
        ft_sharedptr<game_event> event_instance(new (std::nothrow) game_event());

        FT_ASSERT_EQ(1, static_cast<int>(static_cast<bool>(event_instance)));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event_instance->initialize());
        event_instance->set_id(preload_index);
        event_instance->set_duration(1);
        scheduler_instance->schedule_event(event_instance);
        preload_index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance->enable_thread_safety());
    preload_index = 0;
    while (preload_index < 3)
    {
        arguments[preload_index].scheduler_pointer = scheduler_instance;
        arguments[preload_index].event_identifier = preload_index;
        arguments[preload_index].iteration_count = iteration_count;
        create_result = pt_thread_create(&threads[preload_index], ft_nullptr,
                scheduler_reschedule_task, &arguments[preload_index]);
        if (create_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "create_result == 0";
            failure_line = __LINE__;
        }
        if (create_result == 0)
            created_thread_count += 1;
        preload_index += 1;
    }
    preload_index = 0;
    while (preload_index < created_thread_count)
    {
        join_result = pt_thread_timed_join(threads[preload_index], ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
        preload_index += 1;
    }
    if (test_failed != 0)
    {
        delete[] arguments;
        delete[] threads;
        delete scheduler_instance;
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    scheduler_instance->dump_events(events);
    FT_ASSERT_EQ(static_cast<size_t>(3), events.size());
    event_index = 0;
    while (event_index < events.size())
    {
        FT_ASSERT(events[event_index].get() != ft_nullptr);
        FT_ASSERT_EQ(static_cast<int32_t>(event_index + iteration_count),
            events[event_index]->get_duration());
        event_index += 1;
    }
    delete[] arguments;
    delete[] threads;
    delete scheduler_instance;
    return (1);
}

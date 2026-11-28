#include "../test_internal.hpp"
#include "../../Modules/Threading/task_scheduler.hpp"
#include "../../Modules/Threading/task_scheduler_tracing.hpp"
#include "../../Modules/Observability/observability_task_scheduler_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <cstdio>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread_internal.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/promise.hpp"
#include "../../Modules/Template/queue.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static std::mutex g_trace_mutex;
static std::vector<ft_task_trace_event> g_recorded_events;
static std::atomic<uint32_t> g_trace_sink_mask{0U};

template <uint32_t index>
static void task_scheduler_trace_mask_sink(const ft_task_trace_event &)
{
    g_trace_sink_mask.fetch_or(1U << index, std::memory_order_relaxed);
    return ;
}

FT_TEST(test_threading_trace_dispatches_all_sinks_past_inline_capacity)
{
    const task_scheduler_trace_sink sinks[] = {
        task_scheduler_trace_mask_sink<0>, task_scheduler_trace_mask_sink<1>,
        task_scheduler_trace_mask_sink<2>, task_scheduler_trace_mask_sink<3>,
        task_scheduler_trace_mask_sink<4>, task_scheduler_trace_mask_sink<5>,
        task_scheduler_trace_mask_sink<6>, task_scheduler_trace_mask_sink<7>,
        task_scheduler_trace_mask_sink<8>
    };
    FT_ASSERT_EQ(FT_ERR_SUCCESS, task_scheduler_trace_reset_for_tests());
    g_trace_sink_mask.store(0U);
    uint32_t index = 0U;
    while (index < 9U)
        FT_ASSERT_EQ(0, task_scheduler_register_trace_sink(sinks[index++]));
    const ft_task_trace_event event{};
    task_scheduler_trace_emit(event);
    const int32_t error = task_scheduler_trace_get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, task_scheduler_trace_reset_for_tests());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error);
    FT_ASSERT_EQ(0x1FFU, g_trace_sink_mask.load());
    return (1);
}

static void task_scheduler_trace_test_sink(const ft_task_trace_event &event)
{
    std::lock_guard<std::mutex> guard(g_trace_mutex);
    g_recorded_events.push_back(event);
    return ;
}

static void task_scheduler_trace_clear_events(void)
{
    std::lock_guard<std::mutex> guard(g_trace_mutex);
    std::vector<ft_task_trace_event>().swap(g_recorded_events);
    return ;
}

struct task_scheduler_trace_test_guard
{
    ft_bool _cleaned;

    task_scheduler_trace_test_guard() : _cleaned(FT_FALSE)
    {
        return ;
    }

    int32_t cleanup(void)
    {
        int32_t unregister_result;

        if (this->_cleaned == FT_TRUE)
            return (FT_ERR_SUCCESS);
        unregister_result = task_scheduler_unregister_trace_sink(
            &task_scheduler_trace_test_sink);
        task_scheduler_trace_clear_events();
        this->_cleaned = FT_TRUE;
        return (unregister_result);
    }

    ~task_scheduler_trace_test_guard()
    {
        if (this->_cleaned == FT_FALSE)
            (void)this->cleanup();
        return ;
    }
};

static std::vector<ft_task_trace_event> task_scheduler_trace_snapshot(void)
{
    std::vector<ft_task_trace_event> snapshot;

    {
        std::lock_guard<std::mutex> guard(g_trace_mutex);
        snapshot = g_recorded_events;
    }
    return (snapshot);
}

static ft_bool task_scheduler_trace_wait_for_phase(const char *label,
    unsigned long long expected_parent_id, e_ft_task_trace_phase expected_phase)
{
    size_t wait_iterations;

    wait_iterations = 0;
    while (wait_iterations < 10000)
    {
        std::vector<ft_task_trace_event> events;
        unsigned long long traced_identifier;
        size_t index;
        size_t count;

        events = task_scheduler_trace_snapshot();
        traced_identifier = 0;
        index = 0;
        count = events.size();
        while (index < count)
        {
            if (events[index].label == label
                && events[index].parent_id == expected_parent_id)
            {
                traced_identifier = events[index].trace_id;
                break ;
            }
            index += 1;
        }
        if (traced_identifier != 0)
        {
            index = 0;
            while (index < count)
            {
                if (events[index].trace_id == traced_identifier
                    && events[index].phase == expected_phase)
                    return (FT_TRUE);
                index += 1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        wait_iterations += 1;
    }
    return (FT_FALSE);
}

static void task_scheduler_trace_print_diagnostics(const char *label,
    ft_bool task_started, ft_bool task_completed, int32_t task_result)
{
    (void)label;
    (void)task_started;
    (void)task_completed;
    (void)task_result;
    return ;
}

static ft_bool task_scheduler_trace_wait_for_flag_with_diagnostics(
    const char *label, const std::atomic<ft_bool> &flag,
    ft_bool task_started, const std::atomic<int32_t> &task_result,
    int32_t wait_timeout_milliseconds)
{
    std::chrono::steady_clock::time_point deadline;
    std::chrono::steady_clock::time_point last_report;

    deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(wait_timeout_milliseconds);
    last_report = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (flag.load() == FT_TRUE)
            return (FT_TRUE);
        if (std::chrono::steady_clock::now() - last_report
            >= std::chrono::milliseconds(5000))
        {
            task_scheduler_trace_print_diagnostics(label, task_started,
                flag.load(), task_result.load());
            last_report = std::chrono::steady_clock::now();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    task_scheduler_trace_print_diagnostics(label, task_started, flag.load(),
        task_result.load());
    return (FT_FALSE);
}

static void task_scheduler_trace_reset_state(void)
{
    (void)observability_task_scheduler_bridge_shutdown();
    (void)task_scheduler_trace_reset_for_tests();
    task_scheduler_trace_clear_events();
    (void)cma_set_alloc_limit(0);
    return ;
}

FT_TEST(test_task_scheduler_tracing_submit)
{
    task_scheduler_trace_reset_state();
    FT_ASSERT_EQ(0, task_scheduler_register_trace_sink(&task_scheduler_trace_test_sink));
    task_scheduler_trace_test_guard cleanup_guard;
    ft_task_scheduler scheduler_instance(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.initialize());
    unsigned long long root_span;
    unsigned long long previous_span;

    root_span = task_scheduler_trace_generate_span_id();
    previous_span = task_scheduler_trace_push_span(root_span);
    ft_future<int> future_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        scheduler_instance.submit(future_value, []() { return (11); }));
    FT_ASSERT_EQ(11, future_value.get());
    FT_ASSERT(task_scheduler_trace_wait_for_phase(
        g_ft_task_trace_label_async, root_span,
        FT_TASK_TRACE_PHASE_FINISHED));
    task_scheduler_trace_pop_span(previous_span);
    std::vector<ft_task_trace_event> events = task_scheduler_trace_snapshot();
    unsigned long long traced_identifier;
    size_t index;
    size_t count;

    traced_identifier = 0;
    index = 0;
    count = events.size();
    while (index < count)
    {
        if (events[index].label == g_ft_task_trace_label_async)
        {
            traced_identifier = events[index].trace_id;
            break;
        }
        index += 1;
    }
    FT_ASSERT(traced_identifier != 0);
    std::vector<e_ft_task_trace_phase> phases;
    index = 0;
    while (index < count)
    {
        if (events[index].trace_id == traced_identifier)
        {
            phases.push_back(events[index].phase);
            FT_ASSERT_EQ(root_span, events[index].parent_id);
            FT_ASSERT(!events[index].timer_thread);
        }
        index += 1;
    }
    FT_ASSERT_EQ(static_cast<size_t>(5), phases.size());
    FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_SUBMITTED, phases[0]);
    FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_ENQUEUED, phases[1]);
    FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_DEQUEUED, phases[2]);
    FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_STARTED, phases[3]);
    FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_FINISHED, phases[4]);
    FT_ASSERT_EQ(0, cleanup_guard.cleanup());
    return (1);
}

FT_TEST(test_task_scheduler_tracing_schedule_after)
{
    task_scheduler_trace_reset_state();
    FT_ASSERT_EQ(0, task_scheduler_register_trace_sink(&task_scheduler_trace_test_sink));
    task_scheduler_trace_test_guard cleanup_guard;
    ft_task_scheduler scheduler_instance(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.initialize());
    {
        ft_future<int> bootstrap_future;

        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            scheduler_instance.submit(bootstrap_future, []()
        {
            return (1);
        }));
        FT_ASSERT_EQ(1, bootstrap_future.get());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, bootstrap_future.destroy());
    }
    task_scheduler_trace_clear_events();
    std::atomic<ft_bool> task_completed;
    std::atomic<ft_bool> task_started;
    std::atomic<int32_t> task_result;

    task_completed.store(FT_FALSE);
    task_started.store(FT_FALSE);
    task_result.store(0);
    {
        unsigned long long root_span;
        unsigned long long previous_span;

        root_span = task_scheduler_trace_generate_span_id();
        previous_span = task_scheduler_trace_push_span(root_span);
        auto schedule_result = scheduler_instance.schedule_after(std::chrono::milliseconds(20), [&task_started, &task_completed, &task_result]() {
            task_started.store(FT_TRUE);
            task_result.store(7);
            task_completed.store(FT_TRUE);
            return (7);
        });
        ft_future<int> future_value;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.initialize(ft_move(schedule_result.key)));
        ft_scheduled_task_handle handle_value = schedule_result.get_value();
        FT_ASSERT(handle_value.valid());
        if (task_scheduler_trace_wait_for_flag_with_diagnostics(
                "tracing_schedule_after", task_completed, task_started.load(),
                task_result, 600000) != FT_TRUE)
        {
            task_scheduler_trace_pop_span(previous_span);
            (void)future_value.destroy();
            (void)scheduler_instance.destroy();
            FT_ASSERT(FT_FALSE);
        }
        FT_ASSERT_EQ(7, future_value.get());
        FT_ASSERT_EQ(7, task_result.load());
        FT_ASSERT(future_value.valid());
        FT_ASSERT(task_scheduler_trace_wait_for_phase(
            g_ft_task_trace_label_schedule_once, root_span,
            FT_TASK_TRACE_PHASE_FINISHED));
        task_scheduler_trace_pop_span(previous_span);
        std::vector<ft_task_trace_event> events = task_scheduler_trace_snapshot();
        unsigned long long traced_identifier;
        size_t index;
        size_t count;

        traced_identifier = 0;
        index = 0;
        count = events.size();
        while (index < count)
        {
            if (events[index].label == g_ft_task_trace_label_schedule_once)
            {
                traced_identifier = events[index].trace_id;
                break ;
            }
            index += 1;
        }
        FT_ASSERT(traced_identifier != 0);
        std::vector<e_ft_task_trace_phase> phases;
        std::vector<bool> timer_flags;
        index = 0;
        while (index < count)
        {
            if (events[index].trace_id == traced_identifier)
            {
                phases.push_back(events[index].phase);
                timer_flags.push_back(events[index].timer_thread);
                if (events[index].phase == FT_TASK_TRACE_PHASE_TIMER_REGISTERED)
                    FT_ASSERT(!events[index].timer_thread);
                if (events[index].phase == FT_TASK_TRACE_PHASE_TIMER_TRIGGERED)
                    FT_ASSERT(events[index].timer_thread);
                FT_ASSERT_EQ(root_span, events[index].parent_id);
            }
            index += 1;
        }
        FT_ASSERT_EQ(static_cast<size_t>(6), phases.size());
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_TIMER_REGISTERED, phases[0]);
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_TIMER_TRIGGERED, phases[1]);
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_ENQUEUED, phases[2]);
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_DEQUEUED, phases[3]);
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_STARTED, phases[4]);
        FT_ASSERT_EQ(FT_TASK_TRACE_PHASE_FINISHED, phases[5]);
        index = 0;
        while (index < timer_flags.size())
        {
            if (phases[index] == FT_TASK_TRACE_PHASE_ENQUEUED)
                FT_ASSERT(timer_flags[index]);
            if (phases[index] == FT_TASK_TRACE_PHASE_DEQUEUED)
                FT_ASSERT(!timer_flags[index]);
            if (phases[index] == FT_TASK_TRACE_PHASE_STARTED)
                FT_ASSERT(!timer_flags[index]);
            if (phases[index] == FT_TASK_TRACE_PHASE_FINISHED)
                FT_ASSERT(!timer_flags[index]);
            index += 1;
        }
        handle_value = ft_scheduled_task_handle();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.destroy());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler_instance.destroy());
    }
    FT_ASSERT_EQ(0, cleanup_guard.cleanup());
    return (1);
}

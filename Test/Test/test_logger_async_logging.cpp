#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

struct logger_async_sink_context
{
    size_t handled_messages;
};

static int32_t logger_async_sink(const char *message, void *user_data)
{
    logger_async_sink_context *context;

    (void)message;
    context = static_cast<logger_async_sink_context *>(user_data);
    if (context == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    context->handled_messages = context->handled_messages + 1;
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_logger_async_logging_teardown)
{
    logger_async_sink_context context;
    size_t original_limit;
    size_t message_index;
    int sink_registered;

    context.handled_messages = 0;
    sink_registered = 0;
    ft_log_enable_async(false);
    original_limit = ft_log_get_async_queue_limit();
    ft_log_set_async_queue_limit(0);
    if (ft_log_add_sink(logger_async_sink, &context) != 0)
        return (0);
    sink_registered = 1;
    ft_log_reset_async_metrics();
    ft_log_enable_async(true);
    message_index = 0;
    while (message_index < 64)
    {
        ft_log_info("async regression message %u", static_cast<unsigned int>(message_index));
        message_index++;
    }
    ft_log_enable_async(false);
    if (sink_registered)
        ft_log_remove_sink(logger_async_sink, &context);
    ft_log_set_async_queue_limit(original_limit);
    FT_ASSERT(context.handled_messages >= 64);
    return (1);
}

static int32_t logger_worker_stop_sink(const char *, void *user_data)
{
    auto *count = static_cast<std::atomic<uint32_t> *>(user_data);
    if (count->fetch_add(1U, std::memory_order_relaxed) == 0U)
        ft_log_enable_async(FT_FALSE);
    return (FT_ERR_SUCCESS);
}

static int32_t logger_count_async_sink(const char *, void *user_data)
{
    auto *count = static_cast<std::atomic<uint32_t> *>(user_data);
    count->fetch_add(1U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_logger_async_worker_delivers_separate_bursts_before_shutdown)
{
    constexpr uint32_t MESSAGES_PER_BURST = 48U;
    std::atomic<uint32_t> delivered{0U};
    s_log_async_metrics metrics;
    ft_log_enable_async(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_count_async_sink, &delivered));
    const ft_size_t original_limit = ft_log_get_async_queue_limit();
    ft_log_set_async_queue_limit(0U);
    ft_log_enable_async(FT_TRUE);
    uint32_t burst = 0U;
    ft_bool consistent = FT_TRUE;
    while (burst < 2U && consistent != FT_FALSE)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        uint32_t index = 0U;
        while (index < MESSAGES_PER_BURST)
        {
            ft_log_info("async burst %u message %u", burst, index);
            index += 1U;
        }
        const uint32_t target = (burst + 1U) * MESSAGES_PER_BURST;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (delivered.load(std::memory_order_acquire) < target
            && std::chrono::steady_clock::now() < deadline)
        {
            if (ft_log_get_async_metrics(&metrics) != FT_ERR_SUCCESS
                || metrics.pending_messages > metrics.peak_pending_messages
                || metrics.dropped_messages != 0U)
                consistent = FT_FALSE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (delivered.load(std::memory_order_acquire) != target
            || ft_log_get_async_metrics(&metrics) != FT_ERR_SUCCESS
            || metrics.pending_messages != 0U || metrics.peak_pending_messages == 0U)
            consistent = FT_FALSE;
        burst += 1U;
    }
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_count_async_sink, &delivered);
    ft_log_set_async_queue_limit(original_limit);
    FT_ASSERT_EQ(FT_TRUE, consistent);
    FT_ASSERT_EQ(2U * MESSAGES_PER_BURST, delivered.load(std::memory_order_acquire));
    return (1);
}

FT_TEST(test_logger_worker_can_request_stop_and_be_restarted)
{
    std::atomic<uint32_t> delivered{0U};
    ft_log_enable_async(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_worker_stop_sink, &delivered));
    ft_log_enable_async(FT_TRUE);
    ft_log_info("worker stops from its callback");
    ft_log_enable_async(FT_FALSE);
    ft_log_enable_async(FT_TRUE);
    ft_log_info("replacement worker sends");
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_worker_stop_sink, &delivered);
    FT_ASSERT_EQ(2U, delivered.load(std::memory_order_relaxed));
    return (1);
}

static void logger_test_route_async(const char *format_string, ...)
{
    va_list args;
    va_start(args, format_string);
    logger_enqueue_running(LOG_LEVEL_INFO, format_string, args);
    va_end(args);
    return ;
}

FT_TEST(test_logger_routes_in_flight_producer_to_sink_after_shutdown)
{
    logger_async_sink_context context = {0U};
    ft_log_enable_async(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_async_sink, &context));
    logger_test_route_async("%s", "formatted after worker stopped");
    ft_log_remove_sink(logger_async_sink, &context);
    s_log_async_metrics metrics;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_get_async_metrics(&metrics));
    FT_ASSERT_EQ(0U, metrics.pending_messages);
    FT_ASSERT_EQ(1U, context.handled_messages);
    return (1);
}

FT_TEST(test_logger_concurrent_worker_toggles_deliver_all_messages)
{
    logger_async_sink_context context = {0U};
    ft_log_enable_async(FT_FALSE);
    const ft_size_t previous_limit = ft_log_get_async_queue_limit();
    ft_log_set_async_queue_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_async_sink, &context));
    const auto toggle = []()
    {
        uint32_t iteration = 0U;
        while (iteration < 16U)
        {
            ft_log_enable_async(FT_TRUE);
            ft_log_info("transition %u", iteration);
            ft_log_enable_async(FT_FALSE);
            iteration += 1U;
        }
        return ;
    };
    std::thread first(toggle);
    std::thread second(toggle);
    first.join();
    second.join();
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_async_sink, &context);
    ft_log_set_async_queue_limit(previous_limit);
    FT_ASSERT_EQ(32U, context.handled_messages);
    return (1);
}

static int32_t logger_unregister_from_callback(const char *, void *user_data)
{
    auto *context = static_cast<logger_async_sink_context *>(user_data);
    context->handled_messages += 1U;
    ft_log_remove_sink(logger_async_sink, context);
    ft_log_remove_sink(logger_unregister_from_callback, context);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_logger_callback_can_remove_itself_and_skip_a_later_sink)
{
    logger_async_sink_context context = {0U};
    ft_log_enable_async(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_unregister_from_callback, &context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_async_sink, &context));
    ft_log_enable_async(FT_TRUE);
    ft_log_info("remove registrations inside worker callback");
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_unregister_from_callback, &context);
    ft_log_remove_sink(logger_async_sink, &context);
    FT_ASSERT_EQ(1U, context.handled_messages);
    return (1);
}

struct logger_blocked_sink_context
{
    std::mutex mutex;
    std::condition_variable condition;
    ft_bool entered = FT_FALSE;
    ft_bool released = FT_FALSE;
    std::atomic<ft_bool> removed{FT_FALSE};
};

static int32_t logger_blocked_sink(const char *, void *user_data)
{
    auto *context = static_cast<logger_blocked_sink_context *>(user_data);
    std::unique_lock<std::mutex> lock(context->mutex);
    context->entered = FT_TRUE;
    context->condition.notify_all();
    context->condition.wait(lock, [context]()
    {
        return (context->released == FT_TRUE);
    });
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_logger_emission_reports_full_async_queue)
{
    ft_log_enable_async(FT_FALSE);
    const ft_size_t previous_limit = ft_log_get_async_queue_limit();
    ft_logger logger;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.initialize());
    logger_blocked_sink_context context;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_blocked_sink, &context));
    ft_log_set_async_queue_limit(1U);
    ft_log_enable_async(FT_TRUE);
    logger.info("in-flight async message");
    ft_bool entered;
    {
        std::unique_lock<std::mutex> lock(context.mutex);
        entered = context.condition.wait_for(lock, std::chrono::seconds(2), [&context]()
        {
            return (context.entered == FT_TRUE);
        });
    }
    logger.info("queued async message");
    const int32_t queued_error = logger.get_error();
    logger.structured_info("overflow", ft_nullptr, 0U);
    const int32_t overflow_error = logger.get_error();
    {
        std::unique_lock<std::mutex> lock(context.mutex);
        context.released = FT_TRUE;
        context.condition.notify_all();
    }
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_blocked_sink, &context);
    ft_log_set_async_queue_limit(previous_limit);
    FT_ASSERT_EQ(FT_TRUE, entered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queued_error);
    FT_ASSERT_EQ(FT_ERR_FULL, overflow_error);
    return (1);
}

FT_TEST(test_logger_shutdown_preserves_replacement_sinks)
{
    ft_log_close();
    ft_logger original;
    ft_logger replacement;
    logger_blocked_sink_context blocked;
    logger_async_sink_context received{0U};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.initialize());
    original.set_global();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_blocked_sink, &blocked));
    ft_log_enable_async(FT_TRUE);
    ft_log_info("hold the old worker during shutdown");
    ft_bool entered;
    {
        std::unique_lock<std::mutex> lock(blocked.mutex);
        entered = blocked.condition.wait_for(lock, std::chrono::seconds(2), [&blocked]()
        {
            return (blocked.entered == FT_TRUE);
        });
    }
    int32_t close_result = FT_ERR_INTERNAL;
    std::thread closer([&original, &close_result]()
    {
        close_result = original.destroy();
        return ;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline
        && (logger_runtime_closing() == FT_FALSE || g_async_running.load() != FT_FALSE))
        std::this_thread::yield();
    const ft_bool draining = logger_runtime_closing() != FT_FALSE && g_async_running.load() == FT_FALSE;
    replacement.set_global();
    const int32_t add_result = ft_log_add_sink(logger_async_sink, &received);
    std::atomic<ft_bool> start_returned{FT_FALSE};
    std::thread starter([&start_returned]()
    {
        ft_log_enable_async(FT_TRUE);
        start_returned.store(FT_TRUE, std::memory_order_release);
        return ;
    });
    const auto start_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (start_returned.load(std::memory_order_acquire) == FT_FALSE
        && std::chrono::steady_clock::now() < start_deadline)
        std::this_thread::yield();
    const ft_bool start_skipped = start_returned.load(std::memory_order_acquire);
    {
        std::unique_lock<std::mutex> lock(blocked.mutex);
        blocked.released = FT_TRUE;
        blocked.condition.notify_all();
    }
    closer.join();
    starter.join();
    const ft_bool running_after_close = g_async_running.load();
    ft_log_enable_async(FT_TRUE);
    const ft_bool restarted = g_async_running.load();
    ft_log_info("deliver to the replacement sink");
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_blocked_sink, &blocked);
    ft_log_remove_sink(logger_async_sink, &received);
    FT_ASSERT_EQ(FT_TRUE, entered);
    FT_ASSERT_EQ(FT_TRUE, draining);
    FT_ASSERT_EQ(FT_TRUE, start_skipped);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, close_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, add_result);
    FT_ASSERT_EQ(FT_FALSE, running_after_close);
    FT_ASSERT_EQ(FT_TRUE, restarted);
    FT_ASSERT_EQ(1U, received.handled_messages);
    return (1);
}

FT_TEST(test_logger_removal_waits_for_active_callback)
{
    logger_blocked_sink_context context;
    logger_blocked_sink_context next_context;
    ft_log_enable_async(FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_blocked_sink, &context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_blocked_sink, &next_context));
    ft_log_enable_async(FT_TRUE);
    ft_log_info("callback in flight");
    ft_bool entered;
    {
        std::unique_lock<std::mutex> lock(context.mutex);
        entered = context.condition.wait_for(lock, std::chrono::seconds(2), [&context]()
        {
            return (context.entered == FT_TRUE);
        });
    }
    std::thread remover([&context]()
    {
        ft_log_remove_sink(logger_blocked_sink, &context);
        context.removed.store(FT_TRUE, std::memory_order_release);
        return ;
    });
    ft_bool unlisted = FT_FALSE;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && unlisted == FT_FALSE)
    {
        if (logger_lock_sinks() == FT_ERR_SUCCESS)
        {
            unlisted = FT_TRUE;
            ft_size_t index = 0U;
            while (index < g_sinks.size())
            {
                if (g_sinks[index].user_data == &context)
                    unlisted = FT_FALSE;
                index += 1U;
            }
            (void)logger_unlock_sinks();
        }
        std::this_thread::yield();
    }
    const ft_bool removed_while_active = context.removed.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(context.mutex);
        context.released = FT_TRUE;
    }
    context.condition.notify_all();
    ft_bool next_entered;
    {
        std::unique_lock<std::mutex> lock(next_context.mutex);
        next_entered = next_context.condition.wait_for(lock, std::chrono::seconds(2),
            [&next_context]()
            {
                return (next_context.entered == FT_TRUE);
            });
    }
    const auto next_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (context.removed.load(std::memory_order_acquire) == FT_FALSE
        && std::chrono::steady_clock::now() < next_deadline)
        std::this_thread::yield();
    const ft_bool removed_before_next_finished = context.removed.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(next_context.mutex);
        next_context.released = FT_TRUE;
    }
    next_context.condition.notify_all();
    remover.join();
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(logger_blocked_sink, &next_context);
    FT_ASSERT_EQ(FT_TRUE, entered);
    FT_ASSERT_EQ(FT_TRUE, unlisted);
    FT_ASSERT_EQ(FT_FALSE, removed_while_active);
    FT_ASSERT_EQ(FT_TRUE, next_entered);
    FT_ASSERT_EQ(FT_TRUE, removed_before_next_finished);
    return (1);
}

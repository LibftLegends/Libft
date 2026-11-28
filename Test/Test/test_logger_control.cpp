#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include <cstdarg>
#include <atomic>
#include <chrono>
#include <thread>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void    logger_enqueue_message(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    ft_log_enqueue(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
    return ;
}

FT_TEST(test_logger_async_backpressure_metrics)
{
    s_log_async_metrics metrics;
    size_t original_limit;

    ft_log_enable_async(false);
    original_limit = ft_log_get_async_queue_limit();
    ft_log_set_async_queue_limit(2);
    ft_log_reset_async_metrics();
    logger_enqueue_message("backpressure-1");
    logger_enqueue_message("backpressure-2");
    logger_enqueue_message("backpressure-3");
    logger_enqueue_message("backpressure-4");
    FT_ASSERT_EQ(0, ft_log_get_async_metrics(&metrics));
    FT_ASSERT(metrics.pending_messages == 2);
    FT_ASSERT(metrics.dropped_messages >= 2);
    FT_ASSERT(metrics.peak_pending_messages >= metrics.pending_messages);
    ft_log_enable_async(true);
    ft_log_enable_async(false);
    ft_log_set_async_queue_limit(original_limit);
    ft_log_reset_async_metrics();
    return (1);
}

FT_TEST(test_ft_log_set_level_updates_global_threshold)
{
    t_log_level previous_level;

    previous_level = g_level;
    ft_log_set_level(LOG_LEVEL_ERROR);
    FT_ASSERT_EQ(LOG_LEVEL_ERROR, g_level.load(std::memory_order_relaxed));
    g_level = previous_level;
    return (1);
}

FT_TEST(test_ft_log_alloc_logging_without_global_logger)
{
    ft_log_set_alloc_logging(true);
    FT_ASSERT_EQ(false, ft_log_get_alloc_logging());
    return (1);
}

FT_TEST(test_ft_log_global_helpers_forward_to_logger)
{
    {
        ft_logger logger_instance;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.initialize());
        logger_instance.set_global();
        ft_log_set_alloc_logging(true);
        FT_ASSERT_EQ(true, logger_instance.get_alloc_logging());

        FT_ASSERT_EQ(true, ft_log_get_alloc_logging());

        ft_log_set_api_logging(true);
        FT_ASSERT_EQ(true, logger_instance.get_api_logging());

        FT_ASSERT_EQ(true, ft_log_get_api_logging());

        ft_log_set_alloc_logging(false);
        FT_ASSERT_EQ(false, logger_instance.get_alloc_logging());
        ft_log_set_api_logging(false);
        FT_ASSERT_EQ(false, logger_instance.get_api_logging());

        FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.destroy());
    }
    return (1);
}

FT_TEST(test_logger_global_registration_survives_move)
{
    ft_logger original;
    ft_logger moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.enable_thread_safety());
    original.set_global();
    ft_log_set_api_logging(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(original));
    FT_ASSERT_EQ(FT_TRUE, moved.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, ft_log_get_api_logging());
    ft_log_set_api_logging(FT_FALSE);
    FT_ASSERT_EQ(FT_FALSE, moved.get_api_logging());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.destroy());
    FT_ASSERT_EQ(FT_FALSE, ft_log_get_api_logging());
    return (1);
}

static int32_t logger_count_lifecycle_message(const char *, void *user_data)
{
    auto *count = static_cast<uint32_t *>(user_data);
    *count += 1U;
    return (FT_ERR_SUCCESS);
}

static int32_t logger_fail_emission(const char *, void *)
{
    return (FT_ERR_IO);
}

FT_TEST(test_logger_emission_reports_first_sink_failure_and_continues)
{
    ft_log_close();
    ft_logger logger;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.initialize());
    uint32_t delivered = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_fail_emission, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_count_lifecycle_message, &delivered));
    logger.info("format %u", 1U);
    const int32_t text_error = logger.get_error();
    logger.structured_info("structured message", ft_nullptr, 0U);
    const int32_t structured_error = logger.get_error();
    logger.structured_info("missing fields", ft_nullptr, 1U);
    const int32_t argument_error = logger.get_error();
    logger.debug("filtered message");
    const int32_t filtered_error = logger.get_error();
    ft_log_remove_sink(logger_fail_emission, ft_nullptr);
    ft_log_remove_sink(logger_count_lifecycle_message, &delivered);
    FT_ASSERT_EQ(FT_ERR_IO, text_error);
    FT_ASSERT_EQ(FT_ERR_IO, structured_error);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, argument_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filtered_error);
    FT_ASSERT_EQ(2U, delivered);
    return (1);
}

FT_TEST(test_logger_configuration_reports_operation_errors)
{
    ft_log_close();
    ft_logger logger;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.set_file(ft_nullptr, 512U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_error());
    FT_ASSERT_EQ(-1, ft_log_set_file(ft_nullptr, 512U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, logger.set_rotation(512U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, logger.get_error());
    FT_ASSERT_EQ(-1, ft_log_set_rotation(512U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_async_metrics(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_error());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_remote_health(ft_nullptr, 0U, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_error());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.push_context(ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_error());
    FT_ASSERT_EQ(ft_nullptr, logger.make_context_guard(ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, logger.get_error());
    FT_ASSERT_EQ(FT_ERR_INVALID_IP_FORMAT, logger.set_remote_sink("invalid", 80U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_IP_FORMAT, logger.get_error());
    FT_ASSERT_EQ(FT_ERR_INTERNAL, ft_log_set_remote_sink("invalid", 80U, FT_FALSE));
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE, logger.set_syslog("libft"));
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE, logger.get_error());
#endif
    s_log_async_metrics metrics;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.get_async_metrics(&metrics));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.get_error());
    return (1);
}

FT_TEST(test_logger_copy_and_move_preserve_shared_sinks)
{
    ft_log_enable_async(FT_FALSE);
    ft_log_set_level(LOG_LEVEL_INFO);
    ft_logger source;
    ft_logger copy;
    ft_logger moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize());
    source.set_global();
    uint32_t count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_count_lifecycle_message, &count));
    const int32_t copy_result = copy.initialize(source);
    const ft_bool copy_is_thread_safe = copy.is_thread_safe();
    const int32_t copy_destroy_result = copy.destroy();
    ft_log_info("after closing a copied Logger");
    const uint32_t after_copy = count;
    const uint32_t move_result = moved.move(source);
    ft_log_info("after moving the registered Logger");
    const int32_t moved_destroy_result = moved.destroy();
    ft_log_remove_sink(logger_count_lifecycle_message, &count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_result);
    FT_ASSERT_EQ(FT_TRUE, copy_is_thread_safe);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_destroy_result);
    FT_ASSERT_EQ(1U, after_copy);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_result);
    FT_ASSERT_EQ(2U, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_destroy_result);
    return (1);
}

FT_TEST(test_logger_failed_configuration_preserves_existing_runtime)
{
    ft_log_enable_async(FT_FALSE);
    ft_log_set_level(LOG_LEVEL_INFO);
    uint32_t count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(logger_count_lifecycle_message, &count));
    ft_logger logger;
    const int32_t initialize_error = logger.initialize("", 512U, LOG_LEVEL_ERROR);
    const int32_t reported_error = logger.get_error();
    ft_log_info("after a failed Logger configuration");
    ft_log_remove_sink(logger_count_lifecycle_message, &count);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, initialize_error);
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, reported_error);
    FT_ASSERT_EQ(1U, count);
    return (1);
}

FT_TEST(test_logger_global_logging_during_logger_replacement)
{
    std::atomic<ft_bool> stop{FT_FALSE};
    std::atomic<uint32_t> calls{0U};
    std::thread reader([&stop, &calls]()
    {
        while (stop.load(std::memory_order_acquire) == FT_FALSE)
        {
            ft_log_set_api_logging(FT_TRUE);
            (void)ft_log_get_api_logging();
            logger_report_sink_error(FT_ERR_IO);
            calls.fetch_add(1U, std::memory_order_release);
        }
        return ;
    });
    uint32_t iteration = 0U;
    ft_bool valid = FT_TRUE;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (iteration < 256U && std::chrono::steady_clock::now() < deadline)
    {
        ft_logger logger;
        if (logger.initialize() != FT_ERR_SUCCESS)
            valid = FT_FALSE;
        logger.set_global();
        const uint32_t previous_calls = calls.load(std::memory_order_acquire);
        while (calls.load(std::memory_order_acquire) == previous_calls
            && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        if (logger.destroy() != FT_ERR_SUCCESS)
            valid = FT_FALSE;
        iteration += 1U;
    }
    stop.store(FT_TRUE, std::memory_order_release);
    reader.join();
    FT_ASSERT_EQ(FT_TRUE, valid);
    FT_ASSERT_EQ(256U, iteration);
    FT_ASSERT(calls.load(std::memory_order_relaxed) >= iteration);
    FT_ASSERT_EQ(FT_FALSE, ft_log_get_api_logging());
    return (1);
}

FT_TEST(test_logger_log_field_prepare_thread_safety_initializes_mutex)
{
    s_log_field field;
    ft_bool     lock_acquired;

    FT_ASSERT_EQ(0, log_field_prepare_thread_safety(&field));
    FT_ASSERT(field.thread_safe_enabled == FT_TRUE);
    FT_ASSERT(field.mutex != ft_nullptr);
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, log_field_lock(&field, &lock_acquired));
    FT_ASSERT(lock_acquired == FT_TRUE);
    log_field_unlock(&field, lock_acquired);
    log_field_teardown_thread_safety(&field);
    return (1);
}

FT_TEST(test_logger_log_field_lock_blocks_until_released)
{
    s_log_field          field;
    ft_bool              main_lock_acquired;
    std::atomic<bool>    ready;
    std::atomic<bool>    worker_failed;
    std::atomic<long long> wait_duration_ms;
    std::thread          worker;

    field.key = "field";
    FT_ASSERT_EQ(0, log_field_prepare_thread_safety(&field));
    main_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, log_field_lock(&field, &main_lock_acquired));
    FT_ASSERT(main_lock_acquired == FT_TRUE);
    ready.store(false);
    worker_failed.store(false);
    wait_duration_ms.store(0);
    worker = std::thread([&field, &ready, &worker_failed, &wait_duration_ms]() {
        ft_bool worker_lock_acquired;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;

        worker_lock_acquired = FT_FALSE;
        ready.store(true);
        start_time = std::chrono::steady_clock::now();
        if (log_field_lock(&field, &worker_lock_acquired) != 0)
        {
            worker_failed.store(true);
            wait_duration_ms.store(-1);
            return ;
        }
        end_time = std::chrono::steady_clock::now();
        wait_duration_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count());
        if (!worker_lock_acquired)
            worker_failed.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        log_field_unlock(&field, worker_lock_acquired);
    });
    while (!ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    log_field_unlock(&field, main_lock_acquired);
    worker.join();
    FT_ASSERT(worker_failed.load() == false);
    FT_ASSERT(wait_duration_ms.load() >= 40);
    log_field_teardown_thread_safety(&field);
    return (1);
}

FT_TEST(test_ft_logger_constructor_enables_thread_safety)
{
    ft_logger logger_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.initialize());
    FT_ASSERT_EQ(false, logger_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.get_error());
    return (1);
}

FT_TEST(test_ft_logger_prepare_thread_safety_idempotent)
{
    ft_logger logger_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.initialize());
    FT_ASSERT_EQ(false, logger_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.enable_thread_safety());
    FT_ASSERT_EQ(true, logger_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.enable_thread_safety());
    FT_ASSERT_EQ(true, logger_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.disable_thread_safety());
    FT_ASSERT_EQ(false, logger_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_instance.enable_thread_safety());
    FT_ASSERT_EQ(true, logger_instance.is_thread_safe());
    return (1);
}

FT_TEST(test_logger_log_sink_prepare_thread_safety_initializes_mutex)
{
    s_log_sink sink;
    ft_bool    lock_acquired;

    FT_ASSERT_EQ(0, log_sink_prepare_thread_safety(&sink));
    FT_ASSERT(sink.thread_safe_enabled == FT_TRUE);
    FT_ASSERT(sink.mutex != ft_nullptr);
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, log_sink_lock(&sink, &lock_acquired));
    FT_ASSERT(lock_acquired == FT_TRUE);
    log_sink_unlock(&sink, lock_acquired);
    log_sink_teardown_thread_safety(&sink);
    return (1);
}

FT_TEST(test_logger_log_sink_lock_blocks_until_release)
{
    s_log_sink           sink;
    ft_bool              main_lock_acquired;
    std::atomic<bool>    ready;
    std::atomic<bool>    worker_failed;
    std::atomic<long long> wait_duration_ms;
    std::thread          worker;

    FT_ASSERT_EQ(0, log_sink_prepare_thread_safety(&sink));
    main_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, log_sink_lock(&sink, &main_lock_acquired));
    FT_ASSERT(main_lock_acquired == FT_TRUE);
    ready.store(false);
    worker_failed.store(false);
    wait_duration_ms.store(0);
    worker = std::thread([&sink, &ready, &worker_failed, &wait_duration_ms]() {
        ft_bool                                    worker_lock_acquired;
        std::chrono::steady_clock::time_point      start_time;
        std::chrono::steady_clock::time_point      end_time;

        worker_lock_acquired = FT_FALSE;
        ready.store(true);
        start_time = std::chrono::steady_clock::now();
        if (log_sink_lock(&sink, &worker_lock_acquired) != 0)
        {
            worker_failed.store(true);
            wait_duration_ms.store(-1);
            return ;
        }
        end_time = std::chrono::steady_clock::now();
        wait_duration_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count());
        if (!worker_lock_acquired)
            worker_failed.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        log_sink_unlock(&sink, worker_lock_acquired);
    });
    while (!ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    log_sink_unlock(&sink, main_lock_acquired);
    worker.join();
    FT_ASSERT(worker_failed.load() == false);
    FT_ASSERT(wait_duration_ms.load() >= 40);
    log_sink_teardown_thread_safety(&sink);
    return (1);
}

FT_TEST(test_logger_async_metrics_lock_blocks_until_release)
{
    s_log_async_metrics   metrics;
    ft_bool               main_lock_acquired;
    std::atomic<bool>     ready;
    std::atomic<int>      worker_result;
    std::atomic<long long> wait_duration_ms;
    std::thread           worker;

    FT_ASSERT_EQ(0, log_async_metrics_prepare_thread_safety(&metrics));
    main_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, log_async_metrics_lock(&metrics, &main_lock_acquired));
    FT_ASSERT(main_lock_acquired == FT_TRUE);
    ready.store(false);
    worker_result.store(-2);
    wait_duration_ms.store(0);
    worker = std::thread([&metrics, &ready, &worker_result, &wait_duration_ms]() {
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        int result;

        ready.store(true);
        start_time = std::chrono::steady_clock::now();
        result = ft_log_get_async_metrics(&metrics);
        end_time = std::chrono::steady_clock::now();
        worker_result.store(result);
        wait_duration_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count());
    });
    while (!ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    log_async_metrics_unlock(&metrics, main_lock_acquired);
    worker.join();
    FT_ASSERT_EQ(0, worker_result.load());
    FT_ASSERT(wait_duration_ms.load() >= 40);
    log_async_metrics_teardown_thread_safety(&metrics);
    return (1);
}

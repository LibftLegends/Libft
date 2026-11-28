#include "../test_internal.hpp"
#include "../../Modules/Threading/thread_pool.hpp"
#include "../../Modules/Threading/cancellation.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/queue.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_thread_pool_resets_error_status)
{
    ft_thread_pool pool_instance(1, 0);
    std::atomic<int> execution_count;

    execution_count.store(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.initialize());
    cma_set_alloc_limit(1);
    pool_instance.submit([&execution_count]()
    {
        execution_count.store(-1);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, pool_instance.get_error());
    cma_set_alloc_limit(0);
    pool_instance.submit([&execution_count]()
    {
        execution_count.store(1);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.get_error());
    pool_instance.wait();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.get_error());
    int final_count;

    final_count = execution_count.load();
    FT_ASSERT_EQ(1, final_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.get_error());
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_thread_pool_cancellation_skips_tasks)
{
    ft_thread_pool pool_instance(1, 0);
    ft_cancellation_source cancellation_source;
    std::atomic<int> execution_count;

    execution_count.store(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.initialize());
    cancellation_source.request_cancel();
    {
        const ft_cancellation_token cancellation_token(cancellation_source.get_token());

        pool_instance.submit([&execution_count, cancellation_token]()
        {
            if (!cancellation_token.is_cancellation_requested())
                execution_count.fetch_add(1);
            else
                execution_count.fetch_sub(1);
            return ;
        }, cancellation_token);
    }
    pool_instance.wait();
    FT_ASSERT_EQ(0, execution_count.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.destroy());
    return (1);
}

FT_TEST(test_thread_pool_cancellation_allows_execution)
{
    ft_thread_pool pool_instance(1, 0);
    ft_cancellation_source cancellation_source;
    std::atomic<int> execution_count;

    execution_count.store(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.initialize());
    {
        const ft_cancellation_token cancellation_token(cancellation_source.get_token());

        FT_ASSERT_EQ(FT_FALSE, cancellation_token.is_cancellation_requested());
        pool_instance.submit([&execution_count, cancellation_token]()
        {
            if (!cancellation_token.is_cancellation_requested())
                execution_count.fetch_add(1);
            return ;
        }, cancellation_token);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.get_error());
    }
    pool_instance.wait();
    FT_ASSERT_EQ(1, execution_count.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.destroy());
    return (1);
}

FT_TEST(test_cancellation_token_callbacks_trigger)
{
    ft_cancellation_source cancellation_source;
    std::atomic<int> callback_count;
    int registration_status;

    callback_count.store(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.initialize());
    {
        const ft_cancellation_token cancellation_token(cancellation_source.get_token());

        registration_status = cancellation_token.register_callback([&callback_count]()
        {
            callback_count.fetch_add(1);
            return ;
        });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, registration_status);
        cancellation_source.request_cancel();
        FT_ASSERT_EQ(1, callback_count.load());
        registration_status = cancellation_token.register_callback([&callback_count]()
        {
            callback_count.fetch_add(1);
            return ;
        });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, registration_status);
        FT_ASSERT_EQ(2, callback_count.load());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancellation_source.destroy());
    return (1);
}

FT_TEST(test_thread_pool_move_constructor_preserves_execution)
{
    ft_thread_pool source_pool(1, 0);
    std::atomic<int> execution_count;

    execution_count.store(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_pool.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_pool.enable_thread_safety());

    ft_thread_pool moved_pool(0, 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_pool.move(source_pool));

    moved_pool.submit([&execution_count]()
    {
        execution_count.fetch_add(1);
        return ;
    });
    moved_pool.wait();
    FT_ASSERT_EQ(1, execution_count.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_pool.destroy());
    return (1);
}

FT_TEST(test_thread_pool_idle_wait_allows_workers_to_submit_more_work)
{
    ft_thread_pool pool(2U, 0U);
    std::atomic<uint32_t> release{0U};
    std::atomic<uint32_t> executions{0U};
    std::atomic<int32_t> wait_result{FT_ERR_INVALID_STATE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.enable_thread_safety());
    pool.submit([&]()
    {
        while (release.load() == 0U)
            pt_thread_wait_uint32(&release, 0U);
        pool.submit([&]()
        {
            executions.fetch_add(1U);
            return ;
        });
        executions.fetch_add(1U);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    std::thread waiter([&]()
    {
        pool.wait();
        wait_result.store(pool.get_error());
        return ;
    });
    pt_thread_sleep(15U);
    release.store(1U);
    pt_thread_wake_one_uint32(&release);
    waiter.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wait_result.load());
    FT_ASSERT_EQ(2U, executions.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.destroy());
    return (1);
}

FT_TEST(test_threading_pool_can_retry_after_worker_storage_allocation_fails)
{
    ft_thread_pool pool(32U, 0U);
    (void)cma_set_alloc_limit(1U);
    const int32_t initial_error = pool.initialize();
    (void)cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, initial_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize(1U, 0U));
    std::atomic<ft_bool> executed{FT_FALSE};
    pool.submit([&]()
    {
        executed.store(FT_TRUE);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    pool.wait();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    FT_ASSERT_EQ(FT_TRUE, executed.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.destroy());
    return (1);
}

FT_TEST(test_threading_pool_wait_includes_callable_cleanup)
{
    ft_thread_pool pool(1U, 0U);
    std::atomic<uint32_t> cleanup_started{0U};
    std::atomic<uint32_t> release_cleanup{0U};
    std::atomic<ft_bool> ran{FT_FALSE};
    std::atomic<ft_bool> wait_returned{FT_FALSE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize());
    auto capture = std::shared_ptr<int32_t>(new int32_t(0), [&](int32_t *pointer)
    {
        delete pointer;
        if (ran.load() == FT_TRUE)
        {
            cleanup_started.store(1U);
            while (release_cleanup.load() == 0U)
                (void)pt_thread_wait_uint32(&release_cleanup, 0U);
        }
        return ;
    });
    pool.submit([capture = ft_move(capture), &ran]()
    {
        (void)capture;
        ran.store(FT_TRUE);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(2);
    while (cleanup_started.load() == 0U && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    std::thread waiter([&]()
    {
        pool.wait();
        wait_returned.store(FT_TRUE);
        return ;
    });
    pt_thread_sleep(30U);
    const ft_bool returned_early = wait_returned.load();
    release_cleanup.store(1U);
    (void)pt_thread_wake_one_uint32(&release_cleanup);
    waiter.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.destroy());
    FT_ASSERT_EQ(1U, cleanup_started.load());
    FT_ASSERT_EQ(FT_FALSE, returned_early);
    FT_ASSERT_EQ(FT_TRUE, wait_returned.load());
    return (1);
}

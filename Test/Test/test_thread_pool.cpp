#include "../test_internal.hpp"
#include "../../Modules/Threading/thread_pool.hpp"
#include "../../Modules/Threading/cancellation.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <atomic>

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

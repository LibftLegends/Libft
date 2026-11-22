#include "../test_internal.hpp"
#include "../../Modules/Template/queue.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

typedef ft_queue<int> queue_type;

static int queue_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static int queue_expect_sigabrt_uninitialised(void (*operation)(queue_type &))
{
    return (test_expect_sigabrt_signal_uninitialised<queue_type>(operation));
}

static void queue_call_destroy_uninitialised(queue_type &queue_value)
{
    (void)queue_value.destroy();
    return ;
}

static void queue_call_enable_thread_safety_uninitialised(queue_type &queue_value)
{
    (void)queue_value.enable_thread_safety();
    return ;
}

static void queue_call_disable_thread_safety_uninitialised(queue_type &queue_value)
{
    (void)queue_value.disable_thread_safety();
    return ;
}

static void queue_call_is_thread_safe_uninitialised(queue_type &queue_value)
{
    (void)queue_value.is_thread_safe();
    return ;
}

static void queue_call_lock_uninitialised(queue_type &queue_value)
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    (void)queue_value.lock(&lock_acquired);
    return ;
}

static void queue_call_unlock_uninitialised(queue_type &queue_value)
{
    queue_value.unlock(FT_FALSE);
    return ;
}

static void queue_call_enqueue_lvalue_uninitialised(queue_type &queue_value)
{
    int value;

    value = 42;
    queue_value.enqueue(value);
    return ;
}

static void queue_call_enqueue_rvalue_uninitialised(queue_type &queue_value)
{
    queue_value.enqueue(42);
    return ;
}

static void queue_call_dequeue_uninitialised(queue_type &queue_value)
{
    (void)queue_value.dequeue();
    return ;
}

static void queue_call_front_uninitialised(queue_type &queue_value)
{
    (void)queue_value.front();
    return ;
}

static void queue_call_front_const_uninitialised(queue_type &queue_value)
{
    const queue_type &const_queue = queue_value;

    (void)const_queue.front();
    return ;
}

static void queue_call_front_proxy_uninitialised(queue_type &queue_value)
{
    (void)queue_value.front_proxy();
    return ;
}

static void queue_call_size_uninitialised(queue_type &queue_value)
{
    (void)queue_value.size();
    return ;
}

static void queue_call_empty_uninitialised(queue_type &queue_value)
{
    (void)queue_value.empty();
    return ;
}

static void queue_call_clear_uninitialised(queue_type &queue_value)
{
    queue_value.clear();
    return ;
}

static void queue_call_initialize_twice_aborts(void)
{
    queue_type queue_value;

    (void)queue_value.initialize();
    (void)queue_value.initialize();
    return ;
}

static void queue_call_destroy_twice_aborts(void)
{
    queue_type queue_value;

    (void)queue_value.initialize();
    (void)queue_value.destroy();
    (void)queue_value.destroy();
    return ;
}


FT_TEST(test_ft_queue_uninitialised_destructor_is_noop)
{
    queue_type queue_value;

    return (1);
}

FT_TEST(test_ft_queue_uninitialised_destroy_is_noop)
{
    queue_type queue_value;

    queue_call_destroy_uninitialised(queue_value);
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_enable_thread_safety_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(
        queue_call_enable_thread_safety_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_disable_thread_safety_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(
        queue_call_disable_thread_safety_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_is_thread_safe_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_is_thread_safe_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_lock_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_lock_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_unlock_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_unlock_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_enqueue_lvalue_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_enqueue_lvalue_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_enqueue_rvalue_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_enqueue_rvalue_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_dequeue_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_dequeue_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_front_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_front_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_front_const_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_front_const_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_front_proxy_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_front_proxy_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_size_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_size_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_empty_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_empty_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_uninitialised_clear_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt_uninitialised(queue_call_clear_uninitialised));
    return (1);
}

FT_TEST(test_ft_queue_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, queue_expect_sigabrt(queue_call_initialize_twice_aborts));
    return (1);
}

FT_TEST(test_ft_queue_destroy_twice_is_noop)
{
    queue_call_destroy_twice_aborts();
    return (1);
}

FT_TEST(test_ft_queue_initialize_move_from_uninitialised_source_aborts)
{
    /* The move constructor/operator= are deleted, so no runtime move exists. */
    return (1);
}

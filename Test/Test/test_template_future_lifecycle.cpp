#include "../test_internal.hpp"
#include "../../Modules/Template/future.hpp"
#include "../../Modules/Template/promise.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_future_publication_wakes_all_waiters_and_delivers_value)
{
    ft_promise<uint32_t> promise;
    ft_future<uint32_t> future(promise);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, promise.enable_thread_safety());
    std::atomic<uint32_t> waiting(0U);
    uint32_t first_value = 0U;
    uint32_t second_value = 0U;
    std::thread first([&]()
    {
        waiting.fetch_add(1U);
        first_value = future.get();
        return ;
    });
    std::thread second([&]()
    {
        waiting.fetch_add(1U);
        future.wait();
        second_value = future.get();
        return ;
    });
    while (waiting.load() != 2U)
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const uint32_t expected = 27U;
    promise.set_value(expected);
    first.join();
    second.join();
    FT_ASSERT_EQ(expected, first_value);
    FT_ASSERT_EQ(expected, second_value);
    FT_ASSERT_EQ(expected, future.get());
    promise.set_value(31U);
    FT_ASSERT_EQ(31U, future.get());
    return (1);
}

FT_TEST(test_ft_future_void_completes_when_the_promise_is_published)
{
    ft_promise<void> promise;
    ft_future<void> future(promise);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future.initialize());
    std::atomic<ft_bool> waiting(FT_FALSE);
    int32_t worker_error = FT_ERR_INVALID_STATE;
    std::thread worker([&]()
    {
        waiting.store(FT_TRUE);
        future.get();
        worker_error = future.get_error();
        return ;
    });
    while (waiting.load() == FT_FALSE)
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    promise.set_value();
    worker.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, worker_error);
    future.wait();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future.get_error());
    return (1);
}

FT_TEST(test_ft_future_wait_timeout_leaves_readiness_available_for_publication)
{
    std::atomic<uint32_t> readiness(ft_promise_detail::PENDING);
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, ft_promise_detail::wait_ready(
        readiness, std::chrono::milliseconds(2)));
    ft_promise_detail::publish_ready(readiness);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_promise_detail::wait_ready(
        readiness, std::chrono::milliseconds(0)));
    return (1);
}

FT_TEST(test_ft_future_default_is_invalid)
{
    ft_future<int> future_value;

    FT_ASSERT_EQ(false, future_value.valid());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.destroy());
    return (1);
}

FT_TEST(test_ft_future_initialize_copy_lifecycle)
{
    ft_promise<int> promise_value;
    ft_future<int> source_future(promise_value);
    ft_future<int> destination_future;

    promise_value.set_value(101);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    FT_ASSERT_EQ(0, destination_future.initialize(source_future));
    FT_ASSERT_EQ(true, destination_future.valid());
    FT_ASSERT_EQ(101, destination_future.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

FT_TEST(test_ft_future_initialize_move_lifecycle)
{
    ft_promise<int> promise_value;
    ft_future<int> source_future(promise_value);
    ft_future<int> destination_future;

    promise_value.set_value(202);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    FT_ASSERT_EQ(0, destination_future.initialize(ft_move(source_future)));
    FT_ASSERT_EQ(true, destination_future.valid());
    FT_ASSERT_EQ(false, source_future.valid());
    FT_ASSERT_EQ(202, destination_future.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

FT_TEST(test_ft_future_move_method_lifecycle)
{
    ft_promise<int> promise_value;
    ft_future<int> source_future(promise_value);
    ft_future<int> destination_future;

    promise_value.set_value(303);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    FT_ASSERT_EQ(0, destination_future.move(source_future));
    FT_ASSERT_EQ(true, destination_future.valid());
    FT_ASSERT_EQ(false, source_future.valid());
    FT_ASSERT_EQ(303, destination_future.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

FT_TEST(test_ft_future_move_self_is_noop)
{
    ft_promise<int> promise_value;
    ft_future<int> future_value(promise_value);

    promise_value.set_value(404);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.initialize());
    FT_ASSERT_EQ(0, future_value.move(future_value));
    FT_ASSERT_EQ(true, future_value.valid());
    FT_ASSERT_EQ(404, future_value.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.destroy());
    return (1);
}

FT_TEST(test_ft_future_wait_and_error_on_invalid)
{
    ft_future<int> future_value;
    int result_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.initialize());
    future_value.wait();
    result_value = future_value.get();
    FT_ASSERT_EQ(0, result_value);
    FT_ASSERT_EQ(false, future_value.valid());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.destroy());
    return (1);
}

FT_TEST(test_ft_future_thread_safety_member_lifecycle)
{
    ft_promise<int> promise_value;
    ft_future<int> future_value(promise_value);
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.initialize());
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(false, future_value.is_thread_safe());
    FT_ASSERT_EQ(0, future_value.enable_thread_safety());
    FT_ASSERT_EQ(true, future_value.is_thread_safe());
    FT_ASSERT_EQ(0, future_value.lock(&lock_acquired));
    FT_ASSERT_EQ(true, lock_acquired);
    future_value.unlock(lock_acquired);
    future_value.disable_thread_safety();
    FT_ASSERT_EQ(false, future_value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, future_value.destroy());
    return (1);
}

FT_TEST(test_ft_future_void_initialize_and_get)
{
    ft_promise<void> promise_value;
    ft_future<void> source_future(promise_value);
    ft_future<void> copy_future;
    ft_future<void> moved_future;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    FT_ASSERT_EQ(0, copy_future.initialize(source_future));
    FT_ASSERT_EQ(0, moved_future.initialize(ft_move(source_future)));
    FT_ASSERT_EQ(true, copy_future.valid());
    FT_ASSERT_EQ(true, moved_future.valid());
    promise_value.set_value();
    copy_future.get();
    moved_future.get();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

FT_TEST(test_ft_future_void_move_method_and_thread_safety)
{
    ft_promise<void> promise_value;
    ft_future<void> source_future(promise_value);
    ft_future<void> destination_future;
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, destination_future.move(source_future));
    FT_ASSERT_EQ(true, destination_future.valid());
    FT_ASSERT_EQ(false, source_future.valid());
    FT_ASSERT_EQ(0, destination_future.enable_thread_safety());
    FT_ASSERT_EQ(true, destination_future.is_thread_safe());
    FT_ASSERT_EQ(0, destination_future.lock(&lock_acquired));
    FT_ASSERT_EQ(true, lock_acquired);
    destination_future.unlock(lock_acquired);
    destination_future.disable_thread_safety();
    FT_ASSERT_EQ(false, destination_future.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

FT_TEST(test_ft_future_void_move_constructor_and_thread_safety)
{
    ft_promise<void> promise_value;
    ft_future<void> source_future(promise_value);
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.initialize());
    FT_ASSERT_EQ(0, source_future.enable_thread_safety());

    ft_future<void> moved_future;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_future.move(source_future));

    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(true, moved_future.valid());
    FT_ASSERT_EQ(false, source_future.valid());
    FT_ASSERT_EQ(true, moved_future.is_thread_safe());
    FT_ASSERT_EQ(0, moved_future.lock(&lock_acquired));
    FT_ASSERT_EQ(true, lock_acquired);
    moved_future.unlock(lock_acquired);
    promise_value.set_value();
    moved_future.get();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_future.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, moved_future.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_future.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_future.destroy());
    return (1);
}

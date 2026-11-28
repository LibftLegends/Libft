#include "../test_internal.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <chrono>
#include <thread>

FT_TEST(test_pthread_wait_uint32_fast_paths_and_zero_timeout)
{
    std::atomic<uint32_t> value{7U};
#if defined(_WIN32)
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, pt_thread_wait_uint32(ft_nullptr, 7U));
#endif
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_thread_wait_uint32(&value, 6U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_thread_wait_uint32_timed(&value, 6U, 0U));
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, pt_thread_wait_uint32_timed(&value, 7U, 0U));
    return (1);
}

FT_TEST(test_pthread_wake_one_targets_the_requested_address)
{
    std::atomic<uint32_t> first{0U};
    std::atomic<uint32_t> second{0U};
    std::atomic<uint32_t> started{0U};
    std::atomic<int32_t> first_result{FT_ERR_INVALID_STATE};
    std::atomic<int32_t> second_result{FT_ERR_INVALID_STATE};
    std::thread first_waiter([&]()
    {
        started.fetch_add(1U, std::memory_order_release);
        first_result.store(pt_thread_wait_uint32_timed(&first, 0U, 1500U),
            std::memory_order_release);
        return ;
    });
    std::thread second_waiter([&]()
    {
        started.fetch_add(1U, std::memory_order_release);
        second_result.store(pt_thread_wait_uint32_timed(&second, 0U, 1500U),
            std::memory_order_release);
        return ;
    });
    while (started.load(std::memory_order_acquire) != 2U)
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    second.store(1U, std::memory_order_release);
    const int32_t second_wake = pt_thread_wake_one_uint32(&second);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (second_result.load(std::memory_order_acquire) == FT_ERR_INVALID_STATE
        && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const int32_t observed_second = second_result.load(std::memory_order_acquire);
    const int32_t observed_first = first_result.load(std::memory_order_acquire);
    first.store(1U, std::memory_order_release);
    const int32_t first_wake = pt_thread_wake_all_uint32(&first);
    first_waiter.join();
    second_waiter.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_wake);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_wake);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observed_second);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, observed_first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_result.load());
    return (1);
}

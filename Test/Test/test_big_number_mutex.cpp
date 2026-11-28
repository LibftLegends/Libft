#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_big_number_mutex_enable_disable_cycle)
{
    ft_big_number number;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    FT_ASSERT_EQ(FT_FALSE, number.is_thread_safe());
    FT_ASSERT_EQ(ft_nullptr, number._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, number.is_thread_safe());
    FT_ASSERT(number._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, number.is_thread_safe());
    FT_ASSERT_EQ(ft_nullptr, number._mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.destroy());
    return (1);
}

FT_TEST(test_big_number_mutex_enable_is_retryable)
{
    ft_big_number number;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, number.is_thread_safe());
    FT_ASSERT(number._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, number._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, number._mutex);
    return (1);
}

FT_TEST(test_big_number_mutex_destroy_disables_thread_safety)
{
    ft_big_number number;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, number.is_thread_safe());
    FT_ASSERT(number._mutex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, number._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, number._mutex);
    return (1);
}

FT_TEST(test_big_number_move_constructor_preserves_thread_safety_and_value)
{
    ft_big_number source_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize());
    source_value.assign("123456");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.enable_thread_safety());

    ft_big_number moved_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_value.initialize(static_cast<ft_big_number &&>(source_value)));

    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_value._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, source_value._mutex);
    FT_ASSERT_EQ(FT_TRUE, moved_value.is_thread_safe());
    FT_ASSERT(moved_value._mutex != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(moved_value.c_str(), "123456"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_value.destroy());
    return (1);
}

FT_TEST(test_big_number_move_method_preserves_thread_safety_and_value)
{
    ft_big_number source_value;
    ft_big_number destination_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize());
    source_value.assign("789012");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.move(source_value));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_value._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, source_value._mutex);
    FT_ASSERT_EQ(FT_TRUE, destination_value.is_thread_safe());
    FT_ASSERT(destination_value._mutex != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(destination_value.c_str(), "789012"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.destroy());
    return (1);
}

FT_TEST(test_big_number_move_replaces_destination_mutex_and_releases_source_lock)
{
    ft_big_number source;
    ft_big_number destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.assign("-789012");
    destination.assign("41");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source._mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(source_mutex, destination._mutex);
    FT_ASSERT_EQ(ft_nullptr, source._mutex);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    int32_t lock_error = FT_ERR_INVALID_STATE;
    std::thread worker([&]()
    {
        lock_error = source_mutex->try_lock(pt_thread_self());
        if (lock_error == FT_ERR_SUCCESS)
            (void)source_mutex->unlock();
        return ;
    });
    worker.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lock_error);
    FT_ASSERT_EQ(FT_TRUE, destination.is_negative());
    FT_ASSERT_EQ(0, ft_strcmp(destination.c_str(), "789012"));
    return (1);
}

FT_TEST(test_big_number_failed_move_leaves_source_available_for_retry)
{
    ft_big_number source;
    ft_big_number destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.assign("789012");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t move_error = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, move_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(0, ft_strcmp(source.c_str(), "789012"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(0, ft_strcmp(destination.c_str(), "789012"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, destination._mutex);
    return (1);
}

FT_TEST(test_big_number_relations_report_lock_failure_and_retry)
{
    ft_big_number left;
    ft_big_number right;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left.enable_thread_safety());
    left.assign("-42");
    right.assign("99");

    ft_bool results[6];
    int32_t errors[6];
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    results[0] = left == right;
    errors[0] = ft_big_number::get_error();
    results[1] = left != right;
    errors[1] = ft_big_number::get_error();
    results[2] = left < right;
    errors[2] = ft_big_number::get_error();
    results[3] = left <= right;
    errors[3] = ft_big_number::get_error();
    results[4] = left > right;
    errors[4] = ft_big_number::get_error();
    results[5] = left >= right;
    errors[5] = ft_big_number::get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    const ft_bool retry = left < right;
    const int32_t retry_error = ft_big_number::get_error();

    uint32_t index = 0U;
    while (index < 6U)
    {
        FT_ASSERT_EQ(FT_FALSE, results[index]);
        FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, errors[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, retry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retry_error);
    return (1);
}

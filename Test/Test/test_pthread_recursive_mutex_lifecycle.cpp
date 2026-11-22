#include "../test_internal.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <cstring>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static pt_recursive_mutex g_pt_recursive_mutex_abort_object;

static int pt_recursive_mutex_expect_sigabrt(void (*operation)())
{
    int result;

    result = test_expect_sigabrt_signal(operation);
    (void)g_pt_recursive_mutex_abort_object.destroy();
    return (result);
}

static void pt_recursive_mutex_initialize_twice_aborts_operation()
{
    (void)g_pt_recursive_mutex_abort_object.destroy();
    (void)g_pt_recursive_mutex_abort_object.initialize();
    (void)g_pt_recursive_mutex_abort_object.initialize();
    return ;
}

static void pt_recursive_mutex_destroy_uninitialised_aborts_operation()
{
    pt_recursive_mutex mutex_object;

    (void)mutex_object.destroy();
    return ;
}

static void pt_recursive_mutex_destroy_twice_aborts_operation()
{
    pt_recursive_mutex mutex_object;

    (void)mutex_object.initialize();
    (void)mutex_object.destroy();
    (void)mutex_object.destroy();
    return ;
}

static void pt_recursive_mutex_lock_uninitialised_aborts_operation()
{
    pt_recursive_mutex mutex_object;

    (void)mutex_object.lock();
    return ;
}

static void pt_recursive_mutex_uninitialised_destructor_aborts_operation()
{
    alignas(pt_recursive_mutex) unsigned char storage[sizeof(pt_recursive_mutex)];
    pt_recursive_mutex *mutex_pointer;

    std::memset(storage, 0, sizeof(storage));
    mutex_pointer = reinterpret_cast<pt_recursive_mutex *>(storage);
    mutex_pointer->~pt_recursive_mutex();
    return ;
}

FT_TEST(test_pt_recursive_mutex_initialize_destroy_reinitialize_success)
{
    pt_recursive_mutex mutex_object;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_recursive_mutex_recursive_lock_then_destroy_busy)
{
    pt_recursive_mutex mutex_object;
    pt_thread_id_type current_thread_id;

    current_thread_id = pt_thread_self();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.try_lock(current_thread_id));
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, mutex_object.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, mutex_object.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, mutex_object.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pthread_recursive_mutex_try_lock_requires_the_calling_thread)
{
    pt_recursive_mutex mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.lock());
    const pt_thread_id_type owner = pt_thread_self();
    int32_t result = FT_ERR_SUCCESS;
    std::thread worker([&]()
    {
        result = mutex.try_lock(owner);
        return ;
    });
    worker.join();
    const int32_t unlock = mutex.unlock();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unlock);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex.destroy());
    return (1);
}

FT_TEST(test_pt_recursive_mutex_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, pt_recursive_mutex_expect_sigabrt(
        pt_recursive_mutex_initialize_twice_aborts_operation));
    return (1);
}

FT_TEST(test_pt_recursive_mutex_destroy_uninitialised_succeeds)
{
    FT_ASSERT_EQ(0, pt_recursive_mutex_expect_sigabrt(
        pt_recursive_mutex_destroy_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_pt_recursive_mutex_destroy_twice_succeeds)
{
    FT_ASSERT_EQ(0, pt_recursive_mutex_expect_sigabrt(
        pt_recursive_mutex_destroy_twice_aborts_operation));
    return (1);
}

FT_TEST(test_pt_recursive_mutex_lock_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, pt_recursive_mutex_expect_sigabrt(
        pt_recursive_mutex_lock_uninitialised_aborts_operation));
    return (1);
}

FT_TEST(test_pt_recursive_mutex_uninitialised_destructor_succeeds)
{
    FT_ASSERT_EQ(0, pt_recursive_mutex_expect_sigabrt(
        pt_recursive_mutex_uninitialised_destructor_aborts_operation));
    return (1);
}

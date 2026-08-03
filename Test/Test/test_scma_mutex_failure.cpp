#include "../test_internal.hpp"
#include "test_scma_shared.hpp"
#include "../../Modules/SCMA/SCMA.hpp"
#include "../../Modules/SCMA/scma_internal.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void scma_mutex_failure_prepare(void)
{
    if (scma_is_initialised() != 0)
        scma_shutdown();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    return ;
}

static void scma_mutex_failure_cleanup(void)
{
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    if (scma_is_initialised() != 0)
        scma_shutdown();
    (void)scma_disable_thread_safety();
    return ;
}

FT_TEST(test_scma_mutex_lock_failure_direct)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, scma_mutex_lock());
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_initialize_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, scma_initialize(64));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_allocate_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    scma_handle result = scma_allocate(16);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    scma_handle invalid = scma_invalid_handle();
    FT_ASSERT_EQ(invalid.index, result.index);
    FT_ASSERT_EQ(invalid.generation, result.generation);
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_free_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, scma_free(scma_invalid_handle()));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_resize_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, scma_resize(scma_invalid_handle(), 32));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_shutdown_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    scma_shutdown();
    FT_ASSERT_EQ(0, scma_is_initialised());
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_is_initialised_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(0, scma_is_initialised());
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_get_size_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    scma_handle handle = scma_allocate(8);
    if (handle.index == scma_invalid_handle().index
        && handle.generation == scma_invalid_handle().generation)
    {
        scma_mutex_failure_cleanup();
        return (0);
    }
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), scma_get_size(handle));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_read_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    scma_handle handle = scma_allocate(8);
    if (handle.index == scma_invalid_handle().index
        && handle.generation == scma_invalid_handle().generation)
    {
        scma_mutex_failure_cleanup();
        return (0);
    }
    char buffer[8] = {0};
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        scma_read(handle, 0, buffer, sizeof(buffer)));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_write_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    scma_handle handle = scma_allocate(8);
    if (handle.index == scma_invalid_handle().index
        && handle.generation == scma_invalid_handle().generation)
    {
        scma_mutex_failure_cleanup();
        return (0);
    }
    const char source[8] = "scma";
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        scma_write(handle, 0, source, sizeof(source)));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_get_stats_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    scma_stats stats = {0, 0, 0};
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, scma_get_stats(&stats));
    scma_mutex_failure_cleanup();
    return (1);
}

FT_TEST(test_scma_handle_is_valid_mutex_lock_failure)
{
    scma_mutex_failure_prepare();
    FT_ASSERT_EQ(0, scma_initialize(64));
    scma_handle handle = scma_allocate(8);
    if (handle.index == scma_invalid_handle().index
        && handle.generation == scma_invalid_handle().generation)
    {
        scma_mutex_failure_cleanup();
        return (0);
    }
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    FT_ASSERT_EQ(0, scma_handle_is_valid(handle));
    scma_mutex_failure_cleanup();
    return (1);
}

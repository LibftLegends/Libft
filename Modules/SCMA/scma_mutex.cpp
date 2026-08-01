#include <mutex>
#include <new>
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "SCMA.hpp"
#include "../PThread/mutex.hpp"

static std::mutex g_scma_control_mutex;
static pt_recursive_mutex *g_scma_mutex = ft_nullptr;
static ft_bool g_scma_thread_safety_enabled = FT_FALSE;
static ft_size_t g_scma_active_operations = 0;
static thread_local ft_size_t g_scma_lock_depth = 0;

static int32_t scma_prepare_mutex_locked(void)
{
    pt_recursive_mutex *created_mutex;
    int32_t initialize_result;

    if (g_scma_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    created_mutex = new (std::nothrow) pt_recursive_mutex();
    if (created_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_result = created_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete created_mutex;
        return (initialize_result);
    }
    g_scma_mutex = created_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t scma_enable_thread_safety(void)
{
    std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);
    int32_t initialize_result;

    if (g_scma_thread_safety_enabled == FT_TRUE)
        return (scma_prepare_mutex_locked());
    if (g_scma_active_operations != 0)
        return (FT_ERR_THREAD_BUSY);
    initialize_result = scma_prepare_mutex_locked();
    if (initialize_result != FT_ERR_SUCCESS)
        return (initialize_result);
    g_scma_thread_safety_enabled = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t scma_disable_thread_safety(void)
{
    std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

    if (g_scma_active_operations != 0 || g_scma_lock_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    g_scma_thread_safety_enabled = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

ft_bool scma_is_thread_safe_enabled(void)
{
    std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

    return (g_scma_thread_safety_enabled);
}

pt_recursive_mutex *scma_runtime_mutex(void)
{
    std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

    if (g_scma_thread_safety_enabled == FT_FALSE)
        return (ft_nullptr);
    return (g_scma_mutex);
}

static ft_size_t &scma_runtime_lock_depth(void)
{
    return (g_scma_lock_depth);
}

static void scma_finish_operation(void)
{
    std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

    if (g_scma_active_operations > 0)
        g_scma_active_operations--;
    return ;
}

int32_t scma_mutex_lock(void)
{
    ft_size_t &lock_depth = scma_runtime_lock_depth();
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    int32_t mutex_error;

    if (lock_depth == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (lock_depth != 0)
    {
        lock_depth++;
        return (FT_ERR_SUCCESS);
    }
    {
        std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

        if (g_scma_active_operations == static_cast<ft_size_t>(
                FT_SYSTEM_SIZE_MAX))
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
        thread_safety_enabled = g_scma_thread_safety_enabled;
        mutex_pointer = g_scma_mutex;
        if (thread_safety_enabled == FT_TRUE && mutex_pointer == ft_nullptr)
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
        g_scma_active_operations++;
    }
    if (thread_safety_enabled == FT_TRUE)
    {
        mutex_error = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
        if (mutex_error != FT_ERR_SUCCESS)
        {
            scma_finish_operation();
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
        }
    }
    lock_depth = 1;
    return (FT_ERR_SUCCESS);
}

int32_t scma_mutex_unlock(void)
{
    ft_size_t &lock_depth = scma_runtime_lock_depth();
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    int32_t mutex_error;

    if (lock_depth == 0)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    if (lock_depth > 1)
    {
        lock_depth--;
        return (FT_ERR_SUCCESS);
    }
    {
        std::lock_guard<std::mutex> control_lock(g_scma_control_mutex);

        thread_safety_enabled = g_scma_thread_safety_enabled;
        mutex_pointer = g_scma_mutex;
    }
    mutex_error = FT_ERR_SUCCESS;
    if (thread_safety_enabled == FT_TRUE)
        mutex_error = pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    lock_depth = 0;
    scma_finish_operation();
    if (mutex_error != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t scma_mutex_close(void)
{
    ft_size_t &lock_depth = scma_runtime_lock_depth();

    if (lock_depth == 0)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    while (lock_depth > 0)
    {
        if (scma_mutex_unlock() != FT_ERR_SUCCESS)
            return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    }
    return (FT_ERR_SUCCESS);
}

ft_size_t scma_mutex_lock_count(void)
{
    return (scma_runtime_lock_depth());
}

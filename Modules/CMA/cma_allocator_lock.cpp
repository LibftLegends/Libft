#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "cma_internal.hpp"
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <new>
#include "../PThread/mutex.hpp"

static std::mutex g_cma_control_mutex;
static pt_recursive_mutex *g_cma_allocator_mutex = nullptr;
static ft_bool g_cma_thread_safety_enabled = FT_TRUE;
static ft_size_t g_cma_active_operations = 0;
static ft_bool g_cma_transition_in_progress = FT_FALSE;
static std::condition_variable g_cma_control_condition;
static thread_local ft_size_t g_cma_operation_depth = 0;

static int32_t cma_destroy_allocator_mutex_locked(void)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t result;

    mutex_pointer = g_cma_allocator_mutex;
    if (mutex_pointer == nullptr)
        return (FT_ERR_SUCCESS);
    result = mutex_pointer->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    g_cma_allocator_mutex = nullptr;
    mutex_pointer->~pt_recursive_mutex();
    std::free(static_cast<void *>(mutex_pointer));
    return (FT_ERR_SUCCESS);
}

static int32_t cma_prepare_allocator_mutex_locked(void)
{
    void *memory;
    pt_recursive_mutex *created_mutex;
    int32_t result;

    if (g_cma_allocator_mutex != nullptr)
        return (FT_ERR_SUCCESS);
    memory = std::malloc(sizeof(pt_recursive_mutex));
    if (memory == nullptr)
        return (FT_ERR_NO_MEMORY);
    created_mutex = new (memory) pt_recursive_mutex();
    result = created_mutex->initialize();
    if (result != FT_ERR_SUCCESS)
    {
        created_mutex->~pt_recursive_mutex();
        std::free(memory);
        return (result);
    }
    g_cma_allocator_mutex = created_mutex;
    return (FT_ERR_SUCCESS);
}

static void cma_finish_operation(void)
{
    std::lock_guard<std::mutex> control_lock(g_cma_control_mutex);

    if (g_cma_active_operations > 0)
        g_cma_active_operations--;
    if (g_cma_active_operations == 0)
        g_cma_control_condition.notify_all();
    return ;
}

int32_t cma_enable_thread_safety(void)
{
    std::unique_lock<std::mutex> control_lock(g_cma_control_mutex);
    int32_t result;

    if (g_cma_thread_safety_enabled == FT_TRUE
        && g_cma_transition_in_progress == FT_FALSE)
        return (cma_prepare_allocator_mutex_locked());
    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    g_cma_transition_in_progress = FT_TRUE;
    while (g_cma_active_operations != 0)
        g_cma_control_condition.wait(control_lock);
    result = cma_prepare_allocator_mutex_locked();
    if (result != FT_ERR_SUCCESS)
    {
        g_cma_transition_in_progress = FT_FALSE;
        g_cma_control_condition.notify_all();
        return (result);
    }
    g_cma_thread_safety_enabled = FT_TRUE;
    g_cma_transition_in_progress = FT_FALSE;
    g_cma_control_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

int32_t cma_disable_thread_safety(void)
{
    std::unique_lock<std::mutex> control_lock(g_cma_control_mutex);
    int32_t result;

    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    g_cma_transition_in_progress = FT_TRUE;
    while (g_cma_active_operations != 0)
        g_cma_control_condition.wait(control_lock);
    result = cma_destroy_allocator_mutex_locked();
    if (result != FT_ERR_SUCCESS)
    {
        g_cma_transition_in_progress = FT_FALSE;
        g_cma_control_condition.notify_all();
        return (result);
    }
    g_cma_thread_safety_enabled = FT_FALSE;
    g_cma_transition_in_progress = FT_FALSE;
    g_cma_control_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

ft_bool cma_is_thread_safe_enabled(void)
{
    std::lock_guard<std::mutex> control_lock(g_cma_control_mutex);

    return (g_cma_thread_safety_enabled);
}

int32_t cma_lock_allocator(ft_bool *lock_acquired)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    int32_t prepare_result;
    int32_t mutex_error;

    if (lock_acquired == nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    {
        std::unique_lock<std::mutex> control_lock(g_cma_control_mutex);

        while (g_cma_transition_in_progress == FT_TRUE)
            g_cma_control_condition.wait(control_lock);
        thread_safety_enabled = g_cma_thread_safety_enabled;
        mutex_pointer = nullptr;
        if (thread_safety_enabled == FT_TRUE)
        {
            prepare_result = cma_prepare_allocator_mutex_locked();
            if (prepare_result != FT_ERR_SUCCESS)
                return (prepare_result);
            mutex_pointer = g_cma_allocator_mutex;
        }
        g_cma_active_operations++;
    }
    if (thread_safety_enabled == FT_TRUE)
    {
        mutex_error = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
        if (mutex_error != FT_ERR_SUCCESS)
        {
            cma_finish_operation();
            return (FT_ERR_INVALID_STATE);
        }
    }
    if (cma_metadata_make_writable() != FT_ERR_SUCCESS)
    {
        if (thread_safety_enabled == FT_TRUE)
            (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
        cma_finish_operation();
        return (FT_ERR_INVALID_STATE);
    }
    if (cma_metadata_guard_increment() == FT_FALSE)
    {
        if (thread_safety_enabled == FT_TRUE)
            (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
        cma_finish_operation();
        return (FT_ERR_INVALID_STATE);
    }
    *lock_acquired = FT_TRUE;
    g_cma_operation_depth++;
    return (FT_ERR_SUCCESS);
}

int32_t cma_unlock_allocator(ft_bool lock_acquired)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    ft_bool guard_decremented;
    int32_t mutex_error;

    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    {
        std::lock_guard<std::mutex> control_lock(g_cma_control_mutex);

        thread_safety_enabled = g_cma_thread_safety_enabled;
        mutex_pointer = g_cma_allocator_mutex;
    }
    guard_decremented = cma_metadata_guard_decrement();
    mutex_error = FT_ERR_SUCCESS;
    if (thread_safety_enabled == FT_TRUE)
        mutex_error = pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    cma_finish_operation();
    if (g_cma_operation_depth > 0)
        g_cma_operation_depth--;
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    if (guard_decremented == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

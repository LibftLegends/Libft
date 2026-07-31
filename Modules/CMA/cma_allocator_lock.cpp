#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "cma_internal.hpp"
#include <cstdlib>
#include <new>
#include "../PThread/mutex.hpp"

static pt_recursive_mutex *g_cma_allocator_mutex = nullptr;
static ft_bool g_cma_thread_safety_enabled = FT_TRUE;

int32_t cma_enable_thread_safety(void)
{
    if (g_cma_allocator_mutex != nullptr)
    {
        g_cma_thread_safety_enabled = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    void *memory = std::malloc(sizeof(pt_recursive_mutex));
    if (memory == nullptr)
        return (FT_ERR_NO_MEMORY);
    pt_recursive_mutex *created_mutex = new (memory) pt_recursive_mutex();
    int32_t result = created_mutex->initialize();
    if (result != FT_ERR_SUCCESS)
    {
        created_mutex->~pt_recursive_mutex();
        std::free(memory);
        g_cma_allocator_mutex = nullptr;
        return (result);
    }
    g_cma_allocator_mutex = created_mutex;
    g_cma_thread_safety_enabled = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t cma_disable_thread_safety(void)
{
    if (g_cma_allocator_mutex == nullptr)
    {
        g_cma_thread_safety_enabled = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    int32_t result = g_cma_allocator_mutex->destroy();
    g_cma_allocator_mutex->~pt_recursive_mutex();
    std::free(static_cast<void *>(g_cma_allocator_mutex));
    g_cma_allocator_mutex = nullptr;
    g_cma_thread_safety_enabled = FT_FALSE;
    return (result);
}

ft_bool cma_is_thread_safe_enabled(void)
{
    return (g_cma_thread_safety_enabled);
}

static pt_recursive_mutex *cma_allocator_mutex(void)
{
    if (g_cma_allocator_mutex == nullptr)
    {
        if (cma_enable_thread_safety() != FT_ERR_SUCCESS)
            return (nullptr);
    }
    return (g_cma_allocator_mutex);
}

int32_t cma_lock_allocator(ft_bool *lock_acquired)
{
    if (!lock_acquired)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    if (g_cma_thread_safety_enabled == FT_FALSE)
    {
        if (cma_metadata_make_writable() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        if (cma_metadata_guard_increment() == FT_FALSE)
            return (FT_ERR_INVALID_STATE);
        *lock_acquired = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    pt_recursive_mutex *mutex_pointer = cma_allocator_mutex();
    if (mutex_pointer == nullptr)
        return (FT_ERR_INITIALIZATION_FAILED);
    int32_t mutex_error = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (mutex_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (cma_metadata_make_writable() != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_INVALID_STATE);
    }
    ft_bool guard_incremented = cma_metadata_guard_increment();
    if (!guard_incremented)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_INVALID_STATE);
    }
    *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t cma_unlock_allocator(ft_bool lock_acquired)
{
    if (!lock_acquired)
        return (FT_ERR_SUCCESS);
    ft_bool guard_decremented = cma_metadata_guard_decrement();
    if (g_cma_thread_safety_enabled == FT_FALSE)
    {
        if (guard_decremented == FT_FALSE)
            return (FT_ERR_INVALID_STATE);
        return (FT_ERR_SUCCESS);
    }
    pt_recursive_mutex *mutex_pointer = cma_allocator_mutex();
    if (mutex_pointer == nullptr)
        return (FT_ERR_INITIALIZATION_FAILED);
    int32_t mutex_error = pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    if (!guard_decremented)
    {
        if (mutex_error != FT_ERR_SUCCESS)
            return (mutex_error);
        return (FT_ERR_INVALID_STATE);
    }
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    return (FT_ERR_SUCCESS);
}

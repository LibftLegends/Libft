#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "cma_internal.hpp"
#include <atomic>
#include <cstdlib>
#include <new>

#include "../PThread/mutex.hpp"

static const uint64_t CMA_OPERATION_STATE_TRANSITION =
    static_cast<uint64_t>(1U);
static const uint64_t CMA_OPERATION_STATE_ENABLED =
    static_cast<uint64_t>(2U);
static const uint64_t CMA_OPERATION_STATE_ACTIVE_INCREMENT =
    static_cast<uint64_t>(4U);

static std::atomic<pt_recursive_mutex *> g_cma_allocator_mutex(nullptr);
static std::atomic<uint64_t> g_cma_operation_state(CMA_OPERATION_STATE_ENABLED);
static thread_local ft_size_t g_cma_operation_depth = 0;

static uint64_t cma_active_operation_count(uint64_t state)
{
    return (state / CMA_OPERATION_STATE_ACTIVE_INCREMENT);
}

static int32_t cma_create_allocator_mutex(pt_recursive_mutex **mutex_out)
{
    void *memory;
    pt_recursive_mutex *created_mutex;
    int32_t result;

    if (mutex_out == nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *mutex_out = nullptr;
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
    *mutex_out = created_mutex;
    return (FT_ERR_SUCCESS);
}

static int32_t cma_prepare_allocator_mutex(void)
{
    pt_recursive_mutex *current_mutex;
    pt_recursive_mutex *created_mutex;
    int32_t result;

    current_mutex = g_cma_allocator_mutex.load(std::memory_order_acquire);
    if (current_mutex != nullptr)
        return (FT_ERR_SUCCESS);
    result = cma_create_allocator_mutex(&created_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    current_mutex = nullptr;
    if (!g_cma_allocator_mutex.compare_exchange_strong(current_mutex,
            created_mutex, std::memory_order_release,
            std::memory_order_acquire))
    {
        (void)created_mutex->destroy();
        created_mutex->~pt_recursive_mutex();
        std::free(static_cast<void *>(created_mutex));
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cma_destroy_allocator_mutex(void)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t result;

    mutex_pointer = g_cma_allocator_mutex.load(std::memory_order_acquire);
    if (mutex_pointer == nullptr)
        return (FT_ERR_SUCCESS);
    result = mutex_pointer->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    g_cma_allocator_mutex.store(nullptr, std::memory_order_release);
    mutex_pointer->~pt_recursive_mutex();
    std::free(static_cast<void *>(mutex_pointer));
    return (FT_ERR_SUCCESS);
}

static void cma_finish_operation(void)
{
    g_cma_operation_state.fetch_sub(CMA_OPERATION_STATE_ACTIVE_INCREMENT,
        std::memory_order_release);
    return ;
}

static int32_t cma_begin_transition(ft_bool enable)
{
    uint64_t state;
    uint64_t transitioned_state;
    int32_t result;

    state = g_cma_operation_state.load(std::memory_order_acquire);
    while (true)
    {
        if ((state & CMA_OPERATION_STATE_TRANSITION) != 0)
            return (FT_ERR_THREAD_BUSY);
        transitioned_state = state | CMA_OPERATION_STATE_TRANSITION;
        if (g_cma_operation_state.compare_exchange_weak(state,
                transitioned_state, std::memory_order_acq_rel,
                std::memory_order_acquire))
            break ;
    }
    while (cma_active_operation_count(transitioned_state) != 0)
    {
        pt_thread_yield();
        transitioned_state = g_cma_operation_state.load(
                std::memory_order_acquire);
    }
    if (enable == FT_TRUE)
    {
        result = cma_prepare_allocator_mutex();
        if (result == FT_ERR_SUCCESS)
            g_cma_operation_state.store(CMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        else
            g_cma_operation_state.store(
                transitioned_state & CMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        return (result);
    }
    result = cma_destroy_allocator_mutex();
    if (result == FT_ERR_SUCCESS)
        g_cma_operation_state.store(0, std::memory_order_release);
    else
        g_cma_operation_state.store(
            transitioned_state & CMA_OPERATION_STATE_ENABLED,
            std::memory_order_release);
    return (result);
}

int32_t cma_enable_thread_safety(void)
{
    uint64_t state;

    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    state = g_cma_operation_state.load(std::memory_order_acquire);
    if ((state & CMA_OPERATION_STATE_ENABLED) != 0
        && (state & CMA_OPERATION_STATE_TRANSITION) == 0)
        return (cma_prepare_allocator_mutex());
    return (cma_begin_transition(FT_TRUE));
}

int32_t cma_disable_thread_safety(void)
{
    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    return (cma_begin_transition(FT_FALSE));
}

ft_bool cma_is_thread_safe_enabled(void)
{
    uint64_t state;

    state = g_cma_operation_state.load(std::memory_order_acquire);
    if ((state & CMA_OPERATION_STATE_ENABLED) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t cma_lock_allocator(ft_bool *lock_acquired)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    uint64_t state;
    uint64_t next_state;
    int32_t mutex_error;
    int32_t prepare_result;

    if (lock_acquired == nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    if (g_cma_operation_depth != 0)
    {
        if (cma_metadata_make_writable() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        if (cma_metadata_guard_increment() == FT_FALSE)
            return (FT_ERR_INVALID_STATE);
        g_cma_operation_depth++;
        *lock_acquired = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    while (true)
    {
        state = g_cma_operation_state.load(std::memory_order_acquire);
        if ((state & CMA_OPERATION_STATE_TRANSITION) != 0)
        {
            pt_thread_yield();
            continue ;
        }
        if (cma_active_operation_count(state)
                == static_cast<uint64_t>(FT_SYSTEM_SIZE_MAX))
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
        next_state = state + CMA_OPERATION_STATE_ACTIVE_INCREMENT;
        if (g_cma_operation_state.compare_exchange_weak(state, next_state,
                std::memory_order_acquire, std::memory_order_relaxed))
            break ;
    }
    thread_safety_enabled = FT_FALSE;
    mutex_pointer = nullptr;
    if ((next_state & CMA_OPERATION_STATE_ENABLED) != 0)
    {
        thread_safety_enabled = FT_TRUE;
        prepare_result = cma_prepare_allocator_mutex();
        if (prepare_result != FT_ERR_SUCCESS)
        {
            cma_finish_operation();
            return (prepare_result);
        }
        mutex_pointer = g_cma_allocator_mutex.load(std::memory_order_acquire);
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
    g_cma_operation_depth = 1;
    return (FT_ERR_SUCCESS);
}

int32_t cma_unlock_allocator(ft_bool lock_acquired)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    ft_bool guard_decremented;
    uint64_t state;
    int32_t mutex_error;

    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (g_cma_operation_depth > 1)
    {
        guard_decremented = cma_metadata_guard_decrement();
        g_cma_operation_depth--;
        if (guard_decremented == FT_FALSE)
            return (FT_ERR_INVALID_STATE);
        return (FT_ERR_SUCCESS);
    }
    state = g_cma_operation_state.load(std::memory_order_acquire);
    thread_safety_enabled = FT_FALSE;
    if ((state & CMA_OPERATION_STATE_ENABLED) != 0)
        thread_safety_enabled = FT_TRUE;
    mutex_pointer = g_cma_allocator_mutex.load(std::memory_order_acquire);
    guard_decremented = cma_metadata_guard_decrement();
    mutex_error = FT_ERR_SUCCESS;
    if (thread_safety_enabled == FT_TRUE)
        mutex_error = pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    cma_finish_operation();
    g_cma_operation_depth = 0;
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    if (guard_decremented == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

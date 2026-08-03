#include <atomic>
#include <cstdlib>
#include <new>
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "SCMA.hpp"
#include "../PThread/mutex.hpp"

static const uint64_t SCMA_OPERATION_STATE_TRANSITION =
    static_cast<uint64_t>(1U);
static const uint64_t SCMA_OPERATION_STATE_ENABLED =
    static_cast<uint64_t>(2U);
static const uint64_t SCMA_OPERATION_STATE_ACTIVE_INCREMENT =
    static_cast<uint64_t>(4U);

static std::atomic<pt_recursive_mutex *> g_scma_mutex(nullptr);
static std::atomic<uint64_t> g_scma_operation_state(0);
static thread_local ft_size_t g_scma_lock_depth = 0;

static uint64_t scma_active_operation_count(uint64_t state)
{
    return (state / SCMA_OPERATION_STATE_ACTIVE_INCREMENT);
}

static int32_t scma_create_mutex(pt_recursive_mutex **mutex_out)
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

static int32_t scma_prepare_mutex(void)
{
    pt_recursive_mutex *current_mutex;
    pt_recursive_mutex *created_mutex;
    int32_t result;

    current_mutex = g_scma_mutex.load(std::memory_order_acquire);
    if (current_mutex != nullptr)
        return (FT_ERR_SUCCESS);
    result = scma_create_mutex(&created_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    current_mutex = nullptr;
    if (!g_scma_mutex.compare_exchange_strong(current_mutex, created_mutex,
            std::memory_order_release, std::memory_order_acquire))
    {
        (void)created_mutex->destroy();
        created_mutex->~pt_recursive_mutex();
        std::free(static_cast<void *>(created_mutex));
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scma_destroy_mutex(void)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t result;

    mutex_pointer = g_scma_mutex.load(std::memory_order_acquire);
    if (mutex_pointer == nullptr)
        return (FT_ERR_SUCCESS);
    result = mutex_pointer->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    g_scma_mutex.store(nullptr, std::memory_order_release);
    mutex_pointer->~pt_recursive_mutex();
    std::free(static_cast<void *>(mutex_pointer));
    return (FT_ERR_SUCCESS);
}

static void scma_finish_operation(void)
{
    g_scma_operation_state.fetch_sub(SCMA_OPERATION_STATE_ACTIVE_INCREMENT,
        std::memory_order_release);
    return ;
}

static int32_t scma_begin_transition(ft_bool enable)
{
    uint64_t state;
    uint64_t transitioned_state;
    int32_t result;

    state = g_scma_operation_state.load(std::memory_order_acquire);
    while (true)
    {
        if ((state & SCMA_OPERATION_STATE_TRANSITION) != 0)
            return (FT_ERR_THREAD_BUSY);
        transitioned_state = state | SCMA_OPERATION_STATE_TRANSITION;
        if (g_scma_operation_state.compare_exchange_weak(state,
                transitioned_state, std::memory_order_acq_rel,
                std::memory_order_acquire))
            break ;
    }
    while (scma_active_operation_count(transitioned_state) != 0)
    {
        pt_thread_yield();
        transitioned_state = g_scma_operation_state.load(
                std::memory_order_acquire);
    }
    if (enable == FT_TRUE)
    {
        result = scma_prepare_mutex();
        if (result == FT_ERR_SUCCESS)
            g_scma_operation_state.store(SCMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        else
            g_scma_operation_state.store(
                transitioned_state & SCMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        return (result);
    }
    result = scma_destroy_mutex();
    if (result == FT_ERR_SUCCESS)
        g_scma_operation_state.store(0, std::memory_order_release);
    else
        g_scma_operation_state.store(
            transitioned_state & SCMA_OPERATION_STATE_ENABLED,
            std::memory_order_release);
    return (result);
}

int32_t scma_enable_thread_safety(void)
{
    uint64_t state;

    if (g_scma_lock_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    state = g_scma_operation_state.load(std::memory_order_acquire);
    if ((state & SCMA_OPERATION_STATE_ENABLED) != 0
        && (state & SCMA_OPERATION_STATE_TRANSITION) == 0)
        return (scma_prepare_mutex());
    return (scma_begin_transition(FT_TRUE));
}

int32_t scma_disable_thread_safety(void)
{
    if (g_scma_lock_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    return (scma_begin_transition(FT_FALSE));
}

ft_bool scma_is_thread_safe_enabled(void)
{
    uint64_t state;

    state = g_scma_operation_state.load(std::memory_order_acquire);
    if ((state & SCMA_OPERATION_STATE_ENABLED) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

pt_recursive_mutex *scma_runtime_mutex(void)
{
    uint64_t state;

    state = g_scma_operation_state.load(std::memory_order_acquire);
    if ((state & SCMA_OPERATION_STATE_ENABLED) == 0)
        return (ft_nullptr);
    return (g_scma_mutex.load(std::memory_order_acquire));
}

int32_t scma_mutex_lock(void)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    uint64_t state;
    uint64_t next_state;
    int32_t mutex_error;
    int32_t prepare_result;

    if (g_scma_lock_depth == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (g_scma_lock_depth != 0)
    {
        g_scma_lock_depth++;
        return (FT_ERR_SUCCESS);
    }
    while (true)
    {
        state = g_scma_operation_state.load(std::memory_order_acquire);
        if ((state & SCMA_OPERATION_STATE_TRANSITION) != 0)
        {
            pt_thread_yield();
            continue ;
        }
        if (scma_active_operation_count(state)
                == static_cast<uint64_t>(FT_SYSTEM_SIZE_MAX))
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
        next_state = state + SCMA_OPERATION_STATE_ACTIVE_INCREMENT;
        if (g_scma_operation_state.compare_exchange_weak(state, next_state,
                std::memory_order_acquire, std::memory_order_relaxed))
            break ;
    }
    thread_safety_enabled = FT_FALSE;
    mutex_pointer = nullptr;
    if ((next_state & SCMA_OPERATION_STATE_ENABLED) != 0)
    {
        thread_safety_enabled = FT_TRUE;
        prepare_result = scma_prepare_mutex();
        if (prepare_result != FT_ERR_SUCCESS)
        {
            scma_finish_operation();
            return (prepare_result);
        }
        mutex_pointer = g_scma_mutex.load(std::memory_order_acquire);
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
    g_scma_lock_depth = 1;
    return (FT_ERR_SUCCESS);
}

int32_t scma_mutex_unlock(void)
{
    pt_recursive_mutex *mutex_pointer;
    ft_bool thread_safety_enabled;
    uint64_t state;
    int32_t mutex_error;

    if (g_scma_lock_depth == 0)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    if (g_scma_lock_depth > 1)
    {
        g_scma_lock_depth--;
        return (FT_ERR_SUCCESS);
    }
    state = g_scma_operation_state.load(std::memory_order_acquire);
    thread_safety_enabled = FT_FALSE;
    if ((state & SCMA_OPERATION_STATE_ENABLED) != 0)
        thread_safety_enabled = FT_TRUE;
    mutex_pointer = g_scma_mutex.load(std::memory_order_acquire);
    mutex_error = FT_ERR_SUCCESS;
    if (thread_safety_enabled == FT_TRUE)
        mutex_error = pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    g_scma_lock_depth = 0;
    scma_finish_operation();
    if (mutex_error != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t scma_mutex_close(void)
{
    if (g_scma_lock_depth == 0)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    while (g_scma_lock_depth > 0)
    {
        if (scma_mutex_unlock() != FT_ERR_SUCCESS)
            return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    }
    return (FT_ERR_SUCCESS);
}

ft_size_t scma_mutex_lock_count(void)
{
    return (g_scma_lock_depth);
}

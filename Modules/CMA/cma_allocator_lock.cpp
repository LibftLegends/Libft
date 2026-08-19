#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "cma_internal.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <pthread.h>
#include <cstdlib>
#include <new>

#include "../PThread/mutex.hpp"

static const uint64_t CMA_OPERATION_STATE_TRANSITION =
    static_cast<uint64_t>(1U);
static const uint64_t CMA_OPERATION_STATE_ENABLED =
    static_cast<uint64_t>(2U);
static const uint64_t CMA_OPERATION_STATE_ACTIVE_INCREMENT =
    static_cast<uint64_t>(4U);
static const uint64_t CMA_DEFAULT_TRANSITION_TIMEOUT_MS =
    static_cast<uint64_t>(1000U);

static std::atomic<pt_recursive_mutex *> g_cma_allocator_mutex(nullptr);
static std::atomic<uint64_t> g_cma_operation_state(CMA_OPERATION_STATE_ENABLED);
static std::atomic<uint32_t> g_cma_operation_epoch(0U);
static thread_local ft_size_t g_cma_operation_depth = 0;
static thread_local ft_bool g_cma_operation_exit_cleanup_armed = FT_FALSE;
static thread_local pt_recursive_mutex *g_cma_operation_mutex = nullptr;
static thread_local ft_bool g_cma_operation_mutex_locked = FT_FALSE;
static pthread_key_t g_cma_operation_exit_key;
static std::atomic<ft_bool> g_cma_operation_exit_key_initialised(FT_FALSE);
static pthread_once_t g_cma_operation_exit_mutex_once = PTHREAD_ONCE_INIT;
static std::mutex *g_cma_operation_exit_mutex = nullptr;

static void cma_initialize_operation_exit_mutex(void)
{
    void *memory_pointer;

    memory_pointer = std::malloc(sizeof(std::mutex));
    if (memory_pointer == nullptr)
        return ;
    g_cma_operation_exit_mutex = new (memory_pointer) std::mutex();
    return ;
}

static std::mutex *cma_get_operation_exit_key_mutex(void)
{
    if (pthread_once(&g_cma_operation_exit_mutex_once,
            cma_initialize_operation_exit_mutex) != 0)
        return (nullptr);
    return (g_cma_operation_exit_mutex);
}

static void cma_operation_exit_key_cleanup(void *argument)
{
    if (argument == nullptr)
        return ;
    (void)pthread_setspecific(g_cma_operation_exit_key, nullptr);
    while (g_cma_operation_depth != 0)
        (void)cma_unlock_allocator(FT_TRUE);
    return ;
}

static void cma_initialise_operation_exit_key(void)
{
    std::mutex *exit_key_mutex;

    if (g_cma_operation_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        return ;
    exit_key_mutex = cma_get_operation_exit_key_mutex();
    if (exit_key_mutex == nullptr)
        return ;
    exit_key_mutex->lock();
    if (g_cma_operation_exit_key_initialised.load(std::memory_order_relaxed)
        == FT_FALSE
        && pthread_key_create(&g_cma_operation_exit_key,
            cma_operation_exit_key_cleanup) == 0)
        g_cma_operation_exit_key_initialised.store(FT_TRUE,
            std::memory_order_release);
    exit_key_mutex->unlock();
    return ;
}

struct s_cma_operation_exit_guard
{
    ~s_cma_operation_exit_guard()
    {
        while (g_cma_operation_depth != 0)
            (void)cma_unlock_allocator(FT_TRUE);
        return ;
    }
};

static thread_local s_cma_operation_exit_guard g_cma_operation_exit_cleanup;

static void cma_arm_operation_exit_cleanup(void)
{
    if (g_cma_operation_exit_cleanup_armed == FT_TRUE)
    {
        (void)g_cma_operation_exit_cleanup;
        return ;
    }
    cma_initialise_operation_exit_key();
    if (g_cma_operation_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        (void)pthread_setspecific(g_cma_operation_exit_key,
            reinterpret_cast<void *>(static_cast<uintptr_t>(1U)));
    g_cma_operation_exit_cleanup_armed = FT_TRUE;
    (void)g_cma_operation_exit_cleanup;
    return ;
}

static uint64_t cma_active_operation_count(uint64_t state)
{
    return (state / CMA_OPERATION_STATE_ACTIVE_INCREMENT);
}

static ft_bool cma_transition_is_set(uint64_t state)
{
    if ((state & CMA_OPERATION_STATE_TRANSITION) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool cma_thread_safety_is_enabled(uint64_t state)
{
    if ((state & CMA_OPERATION_STATE_ENABLED) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static uint64_t cma_state_without_transition(uint64_t state)
{
    if (cma_thread_safety_is_enabled(state) == FT_TRUE)
        return (CMA_OPERATION_STATE_ENABLED);
    return (static_cast<uint64_t>(0U));
}

static void cma_cancel_transition(void)
{
    (void)g_cma_operation_state.fetch_and(
        ~CMA_OPERATION_STATE_TRANSITION, std::memory_order_release);
    g_cma_operation_epoch.fetch_add(1U, std::memory_order_release);
    (void)pt_thread_wake_all_uint32(&g_cma_operation_epoch);
    return ;
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
    g_cma_operation_epoch.fetch_add(1U, std::memory_order_release);
    (void)pt_thread_wake_one_uint32(&g_cma_operation_epoch);
    return ;
}

static int32_t cma_begin_transition(ft_bool enable, uint64_t timeout_ms)
{
    uint64_t state;
    uint64_t transitioned_state;
    int32_t result;
    int32_t wait_result;
    uint64_t remaining_timeout_ms;
    std::chrono::steady_clock::time_point transition_deadline;
    std::chrono::milliseconds remaining_duration;

    state = g_cma_operation_state.load(std::memory_order_acquire);
    while (true)
    {
        if (cma_transition_is_set(state) == FT_TRUE)
            return (FT_ERR_THREAD_BUSY);
        transitioned_state = state | CMA_OPERATION_STATE_TRANSITION;
        if (g_cma_operation_state.compare_exchange_weak(state,
                transitioned_state, std::memory_order_acq_rel,
                std::memory_order_acquire))
            break ;
    }
    transition_deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeout_ms);
    while (cma_active_operation_count(transitioned_state) != 0)
    {
        remaining_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            transition_deadline - std::chrono::steady_clock::now());
        if (remaining_duration.count() <= 0)
        {
            cma_cancel_transition();
            return (FT_ERR_TIMEOUT);
        }
        remaining_timeout_ms = static_cast<uint64_t>(remaining_duration.count());
        wait_result = pt_thread_wait_uint32_timed(&g_cma_operation_epoch,
            g_cma_operation_epoch.load(std::memory_order_acquire),
            remaining_timeout_ms);
        if (wait_result == FT_ERR_TIMEOUT)
        {
            cma_cancel_transition();
            return (FT_ERR_TIMEOUT);
        }
        if (wait_result != FT_ERR_SUCCESS)
        {
            cma_cancel_transition();
            return (wait_result);
        }
        transitioned_state = g_cma_operation_state.load(
                std::memory_order_acquire);
    }
    if (cma_active_operation_count(transitioned_state) != 0)
    {
        cma_cancel_transition();
        return (FT_ERR_INVALID_STATE);
    }
    if (enable == FT_TRUE)
    {
        result = cma_prepare_allocator_mutex();
        if (result == FT_ERR_SUCCESS)
            g_cma_operation_state.store(CMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        else
            g_cma_operation_state.store(
                cma_state_without_transition(transitioned_state),
                std::memory_order_release);
        g_cma_operation_epoch.fetch_add(1U, std::memory_order_release);
        (void)pt_thread_wake_all_uint32(&g_cma_operation_epoch);
        return (result);
    }
    result = cma_destroy_allocator_mutex();
    if (result == FT_ERR_SUCCESS)
        g_cma_operation_state.store(0, std::memory_order_release);
    else
        g_cma_operation_state.store(
            cma_state_without_transition(transitioned_state),
            std::memory_order_release);
    g_cma_operation_epoch.fetch_add(1U, std::memory_order_release);
    (void)pt_thread_wake_all_uint32(&g_cma_operation_epoch);
    return (result);
}

int32_t cma_enable_thread_safety(void)
{
    return (cma_enable_thread_safety_timed(
        CMA_DEFAULT_TRANSITION_TIMEOUT_MS));
}

int32_t cma_enable_thread_safety_timed(uint64_t timeout_ms)
{
    uint64_t state;

    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    state = g_cma_operation_state.load(std::memory_order_acquire);
    if (cma_thread_safety_is_enabled(state) == FT_TRUE
        && cma_transition_is_set(state) == FT_FALSE)
        return (cma_prepare_allocator_mutex());
    return (cma_begin_transition(FT_TRUE, timeout_ms));
}

int32_t cma_disable_thread_safety(void)
{
    return (cma_disable_thread_safety_timed(
        CMA_DEFAULT_TRANSITION_TIMEOUT_MS));
}

int32_t cma_disable_thread_safety_timed(uint64_t timeout_ms)
{
    if (g_cma_operation_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    return (cma_begin_transition(FT_FALSE, timeout_ms));
}

int32_t cma_set_thread_safety_timed(ft_bool enable, uint64_t timeout_ms)
{
    if (enable == FT_TRUE)
        return (cma_enable_thread_safety_timed(timeout_ms));
    return (cma_disable_thread_safety_timed(timeout_ms));
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
    cma_arm_operation_exit_cleanup();
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
            uint32_t operation_epoch;

            operation_epoch = g_cma_operation_epoch.load(
                std::memory_order_acquire);
            if (g_cma_operation_state.load(std::memory_order_acquire)
                    & CMA_OPERATION_STATE_TRANSITION)
                (void)pt_thread_wait_uint32(&g_cma_operation_epoch,
                    operation_epoch);
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
    g_cma_operation_mutex = mutex_pointer;
    g_cma_operation_mutex_locked = thread_safety_enabled;
    *lock_acquired = FT_TRUE;
    g_cma_operation_depth = 1;
    return (FT_ERR_SUCCESS);
}

int32_t cma_unlock_allocator(ft_bool lock_acquired)
{
    ft_bool guard_decremented;
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
    guard_decremented = cma_metadata_guard_decrement();
    mutex_error = FT_ERR_SUCCESS;
    if (g_cma_operation_mutex_locked == FT_TRUE)
        mutex_error = pt_recursive_mutex_unlock_if_not_null(
            g_cma_operation_mutex);
    g_cma_operation_mutex = nullptr;
    g_cma_operation_mutex_locked = FT_FALSE;
    cma_finish_operation();
    g_cma_operation_depth = 0;
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    if (guard_decremented == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

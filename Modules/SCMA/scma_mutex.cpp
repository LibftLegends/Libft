#include <atomic>
#include <cstdlib>
#include <mutex>
#include <new>
#include <pthread.h>
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
static const uint64_t SCMA_DEFAULT_TRANSITION_TIMEOUT_MS =
    static_cast<uint64_t>(1000U);

static std::atomic<pt_recursive_mutex *> g_scma_mutex(nullptr);
static std::atomic<uint64_t> g_scma_operation_state(0);
static thread_local ft_size_t g_scma_lock_depth = 0;
static pthread_key_t g_scma_operation_exit_key;
static std::atomic<ft_bool> g_scma_operation_exit_key_initialised(FT_FALSE);
static pthread_once_t g_scma_operation_exit_mutex_once = PTHREAD_ONCE_INIT;
static std::mutex *g_scma_operation_exit_mutex = nullptr;

static void scma_initialize_operation_exit_mutex(void)
{
    void *memory_pointer;

    memory_pointer = std::malloc(sizeof(std::mutex));
    if (memory_pointer == nullptr)
        return ;
    g_scma_operation_exit_mutex = new (memory_pointer) std::mutex();
    return ;
}

static std::mutex *scma_get_operation_exit_key_mutex(void)
{
    if (pthread_once(&g_scma_operation_exit_mutex_once,
            scma_initialize_operation_exit_mutex) != 0)
        return (nullptr);
    return (g_scma_operation_exit_mutex);
}

static void scma_operation_exit_key_cleanup(void *argument)
{
    if (argument == nullptr)
        return ;
    (void)pthread_setspecific(g_scma_operation_exit_key, nullptr);
    while (g_scma_lock_depth != 0)
        (void)scma_mutex_unlock();
    return ;
}

static void scma_initialise_operation_exit_key(void)
{
    std::mutex *exit_key_mutex;

    if (g_scma_operation_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        return ;
    exit_key_mutex = scma_get_operation_exit_key_mutex();
    if (exit_key_mutex == nullptr)
        return ;
    exit_key_mutex->lock();
    if (g_scma_operation_exit_key_initialised.load(std::memory_order_relaxed)
        == FT_FALSE
        && pthread_key_create(&g_scma_operation_exit_key,
            scma_operation_exit_key_cleanup) == 0)
        g_scma_operation_exit_key_initialised.store(FT_TRUE,
            std::memory_order_release);
    exit_key_mutex->unlock();
    return ;
}

struct s_scma_operation_exit_guard
{
    ~s_scma_operation_exit_guard()
    {
        while (g_scma_lock_depth != 0)
            (void)scma_mutex_unlock();
        return ;
    }
};

static thread_local s_scma_operation_exit_guard g_scma_operation_exit_cleanup;

static void scma_arm_operation_exit_cleanup(void)
{
    scma_initialise_operation_exit_key();
    if (g_scma_operation_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        (void)pthread_setspecific(g_scma_operation_exit_key,
            reinterpret_cast<void *>(static_cast<uintptr_t>(1U)));
    (void)g_scma_operation_exit_cleanup;
    return ;
}

static uint64_t scma_active_operation_count(uint64_t state)
{
    return (state / SCMA_OPERATION_STATE_ACTIVE_INCREMENT);
}

static ft_bool scma_transition_is_set(uint64_t state)
{
    if ((state & SCMA_OPERATION_STATE_TRANSITION) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool scma_thread_safety_is_enabled(uint64_t state)
{
    if ((state & SCMA_OPERATION_STATE_ENABLED) != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static uint64_t scma_state_without_transition(uint64_t state)
{
    if (scma_thread_safety_is_enabled(state) == FT_TRUE)
        return (SCMA_OPERATION_STATE_ENABLED);
    return (static_cast<uint64_t>(0U));
}

static void scma_cancel_transition(void)
{
    (void)g_scma_operation_state.fetch_and(
        ~SCMA_OPERATION_STATE_TRANSITION, std::memory_order_release);
    return ;
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

static int32_t scma_begin_transition(ft_bool enable, uint64_t timeout_ms)
{
    uint64_t state;
    uint64_t transitioned_state;
    int32_t result;
    uint64_t elapsed_ms;

    state = g_scma_operation_state.load(std::memory_order_acquire);
    while (true)
    {
        if (scma_transition_is_set(state) == FT_TRUE)
            return (FT_ERR_THREAD_BUSY);
        transitioned_state = state | SCMA_OPERATION_STATE_TRANSITION;
        if (g_scma_operation_state.compare_exchange_weak(state,
                transitioned_state, std::memory_order_acq_rel,
                std::memory_order_acquire))
            break ;
    }
    elapsed_ms = 0;
    while (scma_active_operation_count(transitioned_state) != 0)
    {
        if (elapsed_ms >= timeout_ms)
        {
            scma_cancel_transition();
            return (FT_ERR_TIMEOUT);
        }
        (void)pt_thread_sleep(1);
        elapsed_ms++;
        transitioned_state = g_scma_operation_state.load(
                std::memory_order_acquire);
    }
    if (scma_active_operation_count(transitioned_state) != 0)
    {
        scma_cancel_transition();
        return (FT_ERR_INVALID_STATE);
    }
    if (enable == FT_TRUE)
    {
        result = scma_prepare_mutex();
        if (result == FT_ERR_SUCCESS)
            g_scma_operation_state.store(SCMA_OPERATION_STATE_ENABLED,
                std::memory_order_release);
        else
            g_scma_operation_state.store(
                scma_state_without_transition(transitioned_state),
                std::memory_order_release);
        return (result);
    }
    result = scma_destroy_mutex();
    if (result == FT_ERR_SUCCESS)
        g_scma_operation_state.store(0, std::memory_order_release);
    else
        g_scma_operation_state.store(
            scma_state_without_transition(transitioned_state),
            std::memory_order_release);
    return (result);
}

int32_t scma_enable_thread_safety(void)
{
    return (scma_enable_thread_safety_timed(
        SCMA_DEFAULT_TRANSITION_TIMEOUT_MS));
}

int32_t scma_enable_thread_safety_timed(uint64_t timeout_ms)
{
    uint64_t state;

    if (g_scma_lock_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    state = g_scma_operation_state.load(std::memory_order_acquire);
    if (scma_thread_safety_is_enabled(state) == FT_TRUE
        && scma_transition_is_set(state) == FT_FALSE)
        return (scma_prepare_mutex());
    return (scma_begin_transition(FT_TRUE, timeout_ms));
}

int32_t scma_disable_thread_safety(void)
{
    return (scma_disable_thread_safety_timed(
        SCMA_DEFAULT_TRANSITION_TIMEOUT_MS));
}

int32_t scma_disable_thread_safety_timed(uint64_t timeout_ms)
{
    if (g_scma_lock_depth != 0)
        return (FT_ERR_THREAD_BUSY);
    return (scma_begin_transition(FT_FALSE, timeout_ms));
}

int32_t scma_try_set_thread_safety(ft_bool enable)
{
    if (enable == FT_TRUE)
        return (scma_enable_thread_safety_timed(0));
    return (scma_disable_thread_safety_timed(0));
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
    uint64_t state;
    uint64_t next_state;
    int32_t mutex_error;
    int32_t prepare_result;

    scma_arm_operation_exit_cleanup();
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
    mutex_pointer = nullptr;
    if ((next_state & SCMA_OPERATION_STATE_ENABLED) != 0)
    {
        prepare_result = scma_prepare_mutex();
        if (prepare_result != FT_ERR_SUCCESS)
        {
            scma_finish_operation();
            return (prepare_result);
        }
        mutex_pointer = g_scma_mutex.load(std::memory_order_acquire);
    }
    mutex_error = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (mutex_error != FT_ERR_SUCCESS)
    {
        scma_finish_operation();
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    g_scma_lock_depth = 1;
    return (FT_ERR_SUCCESS);
}

int32_t scma_mutex_unlock(void)
{
    pt_recursive_mutex *mutex_pointer;

    if (g_scma_lock_depth == 0)
        return (FT_ERR_SYS_MUTEX_UNLOCK_FAILED);
    if (g_scma_lock_depth > 1)
    {
        g_scma_lock_depth--;
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = g_scma_mutex.load(std::memory_order_acquire);
    (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    g_scma_lock_depth = 0;
    scma_finish_operation();
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

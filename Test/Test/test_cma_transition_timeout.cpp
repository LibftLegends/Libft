#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CMA/cma_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <atomic>

struct s_cma_transition_state
{
    std::atomic<int32_t> ready;
    std::atomic<int32_t> release;
    std::atomic<int32_t> lock_result;
    std::atomic<int32_t> unlock_result;
};

static void *cma_transition_worker(void *argument)
{
    s_cma_transition_state *state;
    ft_bool lock_acquired;

    state = static_cast<s_cma_transition_state *>(argument);
    lock_acquired = FT_FALSE;
    state->lock_result.store(cma_lock_allocator(&lock_acquired));
    if (state->lock_result.load() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    state->ready.store(1);
    while (state->release.load() == 0)
        (void)pt_thread_sleep(1);
    state->unlock_result.store(cma_unlock_allocator(lock_acquired));
    return (ft_nullptr);
}

FT_TEST(test_cma_timed_transition_returns_timeout_for_active_operation)
{
    s_cma_transition_state state;
    pthread_t worker_thread;
    int32_t wait_count;
    int32_t create_result;
    int32_t join_result;
    int32_t test_failed;
    const char *failure_expression;
    int32_t failure_line;
    ft_bool worker_created;

    state.ready.store(0);
    state.release.store(0);
    state.lock_result.store(FT_ERR_INVALID_STATE);
    state.unlock_result.store(FT_ERR_INVALID_STATE);
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    worker_created = FT_FALSE;
    if (cma_enable_thread_safety_timed(100) != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "cma_enable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
        goto cleanup;
    }
    create_result = pt_thread_create(&worker_thread, ft_nullptr,
        cma_transition_worker, &state);
    if (create_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_result == 0";
        failure_line = __LINE__;
        goto cleanup;
    }
    worker_created = FT_TRUE;
    wait_count = 0;
    while (state.ready.load() == 0 && wait_count < 1000)
    {
        (void)pt_thread_sleep(1);
        wait_count++;
    }
    if (state.ready.load() != 1)
    {
        test_failed = 1;
        failure_expression = "state.ready.load() == 1";
        failure_line = __LINE__;
        goto cleanup;
    }
    if (state.lock_result.load() != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "state.lock_result.load() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
        goto cleanup;
    }
    if (cma_disable_thread_safety_timed(5) != FT_ERR_TIMEOUT)
    {
        test_failed = 1;
        failure_expression = "cma_disable_thread_safety_timed(5) == FT_ERR_TIMEOUT";
        failure_line = __LINE__;
        goto cleanup;
    }
    if (cma_is_thread_safe_enabled() != FT_TRUE)
    {
        test_failed = 1;
        failure_expression = "cma_is_thread_safe_enabled() == FT_TRUE";
        failure_line = __LINE__;
        goto cleanup;
    }
cleanup:
    state.release.store(1);
    if (worker_created == FT_TRUE)
    {
        join_result = pt_thread_join(worker_thread, ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
        if (join_result == 0 && state.unlock_result.load() != FT_ERR_SUCCESS
            && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "state.unlock_result.load() == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
    }
    if (cma_disable_thread_safety_timed(100) != FT_ERR_SUCCESS
        && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "cma_disable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (cma_is_thread_safe_enabled() != FT_FALSE && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "cma_is_thread_safe_enabled() == FT_FALSE";
        failure_line = __LINE__;
    }
    if (cma_enable_thread_safety_timed(100) != FT_ERR_SUCCESS
        && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "cma_enable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    return (1);
}

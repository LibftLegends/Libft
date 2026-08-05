#include "../test_internal.hpp"
#include "test_scma_shared.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <atomic>

struct s_scma_transition_state
{
    std::atomic<int32_t> ready;
    std::atomic<int32_t> release;
    std::atomic<int32_t> lock_result;
    std::atomic<int32_t> unlock_result;
};

static void *scma_transition_worker(void *argument)
{
    s_scma_transition_state *state;

    state = static_cast<s_scma_transition_state *>(argument);
    state->lock_result.store(scma_mutex_lock());
    if (state->lock_result.load() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    state->ready.store(1);
    while (state->release.load() == 0)
        (void)pt_thread_sleep(1);
    state->unlock_result.store(scma_mutex_unlock());
    return (ft_nullptr);
}

FT_TEST(test_scma_timed_transition_returns_timeout_for_active_operation)
{
    s_scma_transition_state state;
    pthread_t worker_thread;
    int32_t wait_count;
    int32_t create_result;
    int32_t join_result;
    int32_t test_failed;
    const char *failure_expression;
    int32_t failure_line;
    ft_bool worker_created;

    scma_test_reset();
    state.ready.store(0);
    state.release.store(0);
    state.lock_result.store(FT_ERR_INVALID_STATE);
    state.unlock_result.store(FT_ERR_INVALID_STATE);
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    worker_created = FT_FALSE;
    if (scma_enable_thread_safety_timed(100) != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "scma_enable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
        goto cleanup;
    }
    create_result = pt_thread_create(&worker_thread, ft_nullptr,
        scma_transition_worker, &state);
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
    if (scma_disable_thread_safety_timed(5) != FT_ERR_TIMEOUT)
    {
        test_failed = 1;
        failure_expression = "scma_disable_thread_safety_timed(5) == FT_ERR_TIMEOUT";
        failure_line = __LINE__;
        goto cleanup;
    }
    if (scma_is_thread_safe_enabled() != FT_TRUE)
    {
        test_failed = 1;
        failure_expression = "scma_is_thread_safe_enabled() == FT_TRUE";
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
    if (scma_disable_thread_safety_timed(100) != FT_ERR_SUCCESS
        && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "scma_disable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (scma_is_thread_safe_enabled() != FT_FALSE && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "scma_is_thread_safe_enabled() == FT_FALSE";
        failure_line = __LINE__;
    }
    if (scma_enable_thread_safety_timed(100) != FT_ERR_SUCCESS
        && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "scma_enable_thread_safety_timed(100) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    return (1);
}

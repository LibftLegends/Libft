#include "../test_internal.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif
#define PT_LOCK_TRACKING_TESTING
#include <atomic>
#include <chrono>
#include <csignal>
#include <csetjmp>
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static sigjmp_buf g_sigabrt_jump_buffer;
static std::atomic<int> g_sigabrt_received;

static void handle_sigabrt(int signal_number)
{
    if (signal_number == SIGABRT)
    {
        g_sigabrt_received.store(1);
        siglongjmp(g_sigabrt_jump_buffer, 1);
    }
    return ;
}

struct s_lock_cycle_shared
{
    pt_mutex *first_mutex;
    pt_mutex *second_mutex;
    std::atomic<int> stage;
    std::atomic<int> first_lock_result;
    std::atomic<int> first_lock_error;
    std::atomic<int> second_lock_result;
    std::atomic<int> second_lock_error;
    std::atomic<pt_thread_id_type> worker_thread_identifier;
};

struct s_unlock_shared_state
{
    pt_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> unlock_result;
    std::atomic<int> unlock_error;
};

struct s_try_lock_shared_state
{
    pt_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> lock_result;
    std::atomic<int> lock_error;
    std::atomic<int> unlock_result;
    std::atomic<int> unlock_error;
};

struct s_foreign_owned_shared_state
{
    pt_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> lock_result;
    std::atomic<int> lock_error;
    std::atomic<int> unlock_result;
    std::atomic<int> unlock_error;
    pt_thread_id_type worker_thread_identifier;
};

struct s_thread_id_capture_state
{
    std::atomic<int> stage;
    std::atomic<pt_thread_id_type> thread_identifier;
};

struct s_detached_tracking_state
{
    pt_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> lock_result;
    std::atomic<int> unlock_result;
    std::atomic<pt_thread_id_type> thread_identifier;
};

struct s_recursive_holder_shared_state
{
    pt_recursive_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> lock_result;
    std::atomic<int> unlock_result;
    std::atomic<pt_thread_id_type> thread_identifier;
};

struct s_recursive_contender_shared_state
{
    pt_recursive_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> allow_lock;
    std::atomic<int> lock_result;
    std::atomic<pt_thread_id_type> thread_identifier;
};

struct s_timed_try_lock_shared_state
{
    pt_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> try_lock_result;
    std::atomic<int64_t> elapsed_ms;
};

struct s_timed_recursive_try_lock_shared_state
{
    pt_recursive_mutex *mutex_pointer;
    std::atomic<int> stage;
    std::atomic<int> try_lock_result;
    std::atomic<int64_t> elapsed_ms;
    std::atomic<pt_thread_id_type> thread_identifier;
};

static void initialize_shared_state(s_lock_cycle_shared *shared, pt_mutex *first_mutex, pt_mutex *second_mutex)
{
    shared->first_mutex = first_mutex;
    shared->second_mutex = second_mutex;
    shared->stage.store(0);
    shared->first_lock_result.store(0);
    shared->first_lock_error.store(0);
    shared->second_lock_result.store(0);
    shared->second_lock_error.store(0);
    shared->worker_thread_identifier.store(0);
    return ;
}

static bool wait_for_stage(std::atomic<int> *stage, int expected_stage)
{
    int attempts;

    attempts = 0;
    while (stage->load() < expected_stage && attempts < 5000)
    {
        pt_thread_sleep(1);
        attempts += 1;
    }
    if (stage->load() < expected_stage)
        return (false);
    return (true);
}

static bool vector_contains_mutex(const pt_mutex_vector &owned_mutexes, pt_mutex *mutex_pointer)
{
    ft_size_t index;

    index = 0;
    while (index < owned_mutexes.size)
    {
        if (owned_mutexes.data[index] == mutex_pointer)
            return (true);
        index += 1;
    }
    return (false);
}

static ft_size_t find_wait_snapshot_index(
    const pt_lock_wait_snapshot_vector &snapshot,
    const void *mutex_pointer,
    pt_thread_id_type waiting_thread)
{
    ft_size_t index;

    index = 0;
    while (index < snapshot.size)
    {
        if (snapshot.data[index].mutex_pointer == mutex_pointer
            && snapshot.data[index].waiting_thread == waiting_thread)
            return (index);
        index += 1;
    }
    return (snapshot.size);
}

static bool wait_for_thread_state(pt_thread_id_type thread_identifier, pt_mutex *owned_mutex_pointer,
        pt_mutex *waiting_mutex_pointer, ft_size_t expected_owned_count, long timeout_ms)
{
    s_pt_lock_tracking_thread_state state;
    long attempts;
    ft_size_t owned_mutex_count;

    pt_buffer_init(state.owned_mutexes);
    state.thread_identifier = 0;
    state.waiting_mutex = ft_nullptr;
    state.wait_started_ms = 0;
    attempts = 0;
    while (attempts <= timeout_ms)
    {
        if (pt_lock_tracking::get_thread_state(thread_identifier, state)
                == FT_ERR_SUCCESS)
        {
            owned_mutex_count = state.owned_mutexes.size;
            if (owned_mutex_count == expected_owned_count
                && vector_contains_mutex(state.owned_mutexes, owned_mutex_pointer)
                && state.waiting_mutex == waiting_mutex_pointer)
            {
                pt_buffer_destroy(state.owned_mutexes);
                return (true);
            }
        }
        pt_thread_sleep(1);
        attempts += 1;
    }
    pt_buffer_destroy(state.owned_mutexes);
    return (false);
}

static void *deadlock_worker(void *argument)
{
    s_lock_cycle_shared *shared;
    int second_mutex_locked;
    int first_mutex_locked;

    shared = static_cast<s_lock_cycle_shared *>(argument);
    second_mutex_locked = 0;
    first_mutex_locked = 0;
    shared->worker_thread_identifier.store(THREAD_ID);
    shared->stage.store(1);
    shared->first_lock_result.store(shared->second_mutex->lock());
    if (shared->first_lock_result.load() == FT_ERR_SUCCESS)
        second_mutex_locked = 1;
    shared->first_lock_error.store(FT_ERR_SUCCESS);
    shared->stage.store(2);
    shared->second_lock_result.store(shared->first_mutex->lock());
    if (shared->second_lock_result.load() == FT_ERR_SUCCESS)
        first_mutex_locked = 1;
    shared->second_lock_error.store(FT_ERR_SUCCESS);
    shared->stage.store(3);
    if (first_mutex_locked == 1)
        shared->first_mutex->unlock();
    shared->stage.store(4);
    if (second_mutex_locked == 1)
        shared->second_mutex->unlock();
    shared->stage.store(5);
    return (ft_nullptr);
}

static void *detached_tracking_worker(void *argument)
{
    s_detached_tracking_state *state;

    state = static_cast<s_detached_tracking_state *>(argument);
    state->thread_identifier.store(THREAD_ID);
    state->lock_result.store(state->mutex_pointer->lock());
    state->stage.store(1);
    if (state->lock_result.load() == FT_ERR_SUCCESS)
        state->unlock_result.store(state->mutex_pointer->unlock());
    else
        state->unlock_result.store(FT_ERR_INVALID_STATE);
    state->stage.store(2);
    return (ft_nullptr);
}

static void initialize_unlock_shared_state(s_unlock_shared_state *shared_state, pt_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->unlock_result.store(0);
    shared_state->unlock_error.store(0);
    return ;
}

static void *unlock_worker(void *argument)
{
    s_unlock_shared_state *shared_state;

    shared_state = static_cast<s_unlock_shared_state *>(argument);
    g_sigabrt_received.store(0);
    shared_state->stage.store(1);
    if (sigsetjmp(g_sigabrt_jump_buffer, 1) != 0)
    {
        shared_state->unlock_result.store(-1);
        shared_state->unlock_error.store(FT_ERR_INVALID_ARGUMENT);
        shared_state->stage.store(2);
        return (ft_nullptr);
    }
    shared_state->unlock_result.store(shared_state->mutex_pointer->unlock());
    shared_state->unlock_error.store(FT_ERR_SUCCESS);
    shared_state->stage.store(2);
    return (ft_nullptr);
}

static void initialize_try_lock_shared_state(s_try_lock_shared_state *shared_state, pt_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->lock_result.store(0);
    shared_state->lock_error.store(0);
    shared_state->unlock_result.store(0);
    shared_state->unlock_error.store(0);
    return ;
}

static void initialize_foreign_owned_shared_state(s_foreign_owned_shared_state *shared_state, pt_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->lock_result.store(0);
    shared_state->lock_error.store(0);
    shared_state->unlock_result.store(0);
    shared_state->unlock_error.store(0);
    shared_state->worker_thread_identifier = 0;
    return ;
}

static void initialize_thread_id_capture_state(
    s_thread_id_capture_state *shared_state)
{
    shared_state->stage.store(0);
    shared_state->thread_identifier.store(0);
    return ;
}

static void initialize_recursive_holder_shared_state(
    s_recursive_holder_shared_state *shared_state,
    pt_recursive_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->lock_result.store(0);
    shared_state->unlock_result.store(0);
    shared_state->thread_identifier.store(0);
    return ;
}

static void initialize_recursive_contender_shared_state(
    s_recursive_contender_shared_state *shared_state,
    pt_recursive_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->allow_lock.store(0);
    shared_state->lock_result.store(0);
    shared_state->thread_identifier.store(0);
    return ;
}

static void initialize_timed_try_lock_shared_state(
    s_timed_try_lock_shared_state *shared_state,
    pt_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->try_lock_result.store(FT_ERR_SUCCESS);
    shared_state->elapsed_ms.store(0);
    return ;
}

static void initialize_timed_recursive_try_lock_shared_state(
    s_timed_recursive_try_lock_shared_state *shared_state,
    pt_recursive_mutex *mutex_pointer)
{
    shared_state->mutex_pointer = mutex_pointer;
    shared_state->stage.store(0);
    shared_state->try_lock_result.store(FT_ERR_SUCCESS);
    shared_state->elapsed_ms.store(0);
    shared_state->thread_identifier.store(0);
    return ;
}

static void *try_lock_worker(void *argument)
{
    s_try_lock_shared_state *shared_state;

    shared_state = static_cast<s_try_lock_shared_state *>(argument);
    shared_state->stage.store(1);
    shared_state->lock_result.store(shared_state->mutex_pointer->lock());
    shared_state->lock_error.store(FT_ERR_SUCCESS);
    shared_state->stage.store(2);
    while (shared_state->stage.load() < 3)
    {
        pt_thread_sleep(1);
    }
    shared_state->unlock_result.store(shared_state->mutex_pointer->unlock());
    shared_state->unlock_error.store(FT_ERR_SUCCESS);
    shared_state->stage.store(4);
    return (ft_nullptr);
}

static void *timed_try_lock_worker(void *argument)
{
    s_timed_try_lock_shared_state *shared_state;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;

    shared_state = static_cast<s_timed_try_lock_shared_state *>(argument);
    shared_state->stage.store(1);
    start_time = std::chrono::steady_clock::now();
    shared_state->try_lock_result.store(shared_state->mutex_pointer->try_lock());
    end_time = std::chrono::steady_clock::now();
    shared_state->elapsed_ms.store(std::chrono::duration_cast<
            std::chrono::milliseconds>(end_time - start_time).count());
    shared_state->stage.store(2);
    return (ft_nullptr);
}

static void *foreign_owned_mutex_worker(void *argument)
{
    s_foreign_owned_shared_state *shared_state;
    pt_thread_id_type thread_identifier;

    shared_state = static_cast<s_foreign_owned_shared_state *>(argument);
    thread_identifier = THREAD_ID;
    shared_state->worker_thread_identifier = thread_identifier;
    shared_state->stage.store(1);
    shared_state->lock_result.store(shared_state->mutex_pointer->lock());
    shared_state->lock_error.store(FT_ERR_SUCCESS);
    shared_state->stage.store(2);
    while (shared_state->stage.load() < 3)
    {
        pt_thread_sleep(1);
    }
    shared_state->unlock_result.store(shared_state->mutex_pointer->unlock());
    shared_state->unlock_error.store(FT_ERR_SUCCESS);
    shared_state->stage.store(4);
    return (ft_nullptr);
}

static void *thread_id_capture_worker(void *argument)
{
    s_thread_id_capture_state *shared_state;

    shared_state = static_cast<s_thread_id_capture_state *>(argument);
    shared_state->thread_identifier.store(THREAD_ID);
    shared_state->stage.store(1);
    return (ft_nullptr);
}

static void *recursive_holder_worker(void *argument)
{
    s_recursive_holder_shared_state *shared_state;

    shared_state = static_cast<s_recursive_holder_shared_state *>(argument);
    shared_state->thread_identifier.store(THREAD_ID);
    shared_state->stage.store(1);
    shared_state->lock_result.store(shared_state->mutex_pointer->lock());
    shared_state->stage.store(2);
    while (shared_state->stage.load() < 3)
    {
        pt_thread_sleep(1);
    }
    shared_state->unlock_result.store(shared_state->mutex_pointer->unlock());
    shared_state->stage.store(4);
    return (ft_nullptr);
}

static void *timed_recursive_try_lock_worker(void *argument)
{
    s_timed_recursive_try_lock_shared_state *shared_state;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;

    shared_state = static_cast<s_timed_recursive_try_lock_shared_state *>(argument);
    shared_state->thread_identifier.store(THREAD_ID);
    shared_state->stage.store(1);
    start_time = std::chrono::steady_clock::now();
    shared_state->try_lock_result.store(
            shared_state->mutex_pointer->try_lock(THREAD_ID));
    end_time = std::chrono::steady_clock::now();
    shared_state->elapsed_ms.store(std::chrono::duration_cast<
            std::chrono::milliseconds>(end_time - start_time).count());
    shared_state->stage.store(2);
    return (ft_nullptr);
}

static void *recursive_contender_worker(void *argument)
{
    s_recursive_contender_shared_state *shared_state;

    shared_state = static_cast<s_recursive_contender_shared_state *>(argument);
    shared_state->thread_identifier.store(THREAD_ID);
    shared_state->stage.store(1);
    while (shared_state->allow_lock.load() == 0)
        pt_thread_sleep(1);
    shared_state->lock_result.store(shared_state->mutex_pointer->lock());
    shared_state->stage.store(2);
    return (ft_nullptr);
}

FT_TEST(test_pt_lock_tracking_detects_cycle)
{
    pt_mutex first_mutex;
    pt_mutex second_mutex;
    s_lock_cycle_shared shared;
    pthread_t worker_thread;
    int thread_created;
    int test_failed;
    int first_mutex_locked;
    int second_mutex_locked;
    const long join_timeout_ms = 200;
    const char *failure_expression;
    int failure_line;
#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    test_failed = 0;
    first_mutex_locked = 0;
    second_mutex_locked = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    RECORD_ASSERT(pt_lock_tracking::notify_thread_exit(THREAD_ID) == FT_ERR_SUCCESS);
    RECORD_ASSERT(first_mutex.initialize() == FT_ERR_SUCCESS);
    RECORD_ASSERT(second_mutex.initialize() == FT_ERR_SUCCESS);
    initialize_shared_state(&shared, &first_mutex, &second_mutex);
    RECORD_ASSERT(first_mutex.lock() == FT_ERR_SUCCESS);
    first_mutex_locked = 1;
    if (pt_thread_create(&worker_thread, ft_nullptr, deadlock_worker, &shared) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&shared.stage, 2));
    RECORD_ASSERT(shared.worker_thread_identifier.load() != 0);
    RECORD_ASSERT(shared.first_lock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared.first_lock_error.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(wait_for_thread_state(THREAD_ID, &first_mutex, ft_nullptr, 1, 20));
    RECORD_ASSERT(wait_for_thread_state(shared.worker_thread_identifier.load(), &second_mutex, &first_mutex, 1, 100));
    RECORD_ASSERT(second_mutex.lock() == FT_ERR_MUTEX_ALREADY_LOCKED);
    RECORD_ASSERT(second_mutex.lockState() == true);
    RECORD_ASSERT(first_mutex.unlock() == FT_ERR_SUCCESS);
    first_mutex_locked = 0;
    RECORD_ASSERT(wait_for_stage(&shared.stage, 5));
    RECORD_ASSERT(shared.second_lock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared.second_lock_error.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(second_mutex.lockState() == false);
    goto cleanup;

cleanup:
    if (first_mutex_locked == 1)
        first_mutex.unlock();
    if (second_mutex_locked == 1)
        second_mutex.unlock();
    if (thread_created == 1)
    {
        int join_result;

        join_result = pt_thread_timed_join(worker_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.destroy());
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_lock_tracking_reports_owned_mutexes)
{
    pt_mutex first_mutex;
    pt_mutex second_mutex;
    pt_mutex_vector owned_mutexes;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.lock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.lock());
    int owned_error = 0;
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(THREAD_ID, &owned_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, owned_error);
    FT_ASSERT_EQ(2, static_cast<int>(owned_mutexes.size));
    pt_buffer_destroy(owned_mutexes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.destroy());
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(THREAD_ID, &owned_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, owned_error);
    FT_ASSERT_EQ(0, static_cast<int>(owned_mutexes.size));
    pt_buffer_destroy(owned_mutexes);
    return (1);
}

FT_TEST(test_pt_mutex_unlock_requires_ownership)
{
    pt_mutex mutex_object;
    s_unlock_shared_state shared_state;
    pthread_t worker_thread;
    int thread_created;
    int test_failed;
    struct sigaction sigabrt_action;
    struct sigaction previous_sigabrt_action;
    int handler_installed;
    const char *failure_expression;
    int failure_line;

#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    test_failed = 0;
    handler_installed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    sigabrt_action.sa_handler = handle_sigabrt;
    RECORD_ASSERT(sigemptyset(&sigabrt_action.sa_mask) == 0);
    sigabrt_action.sa_flags = 0;
    RECORD_ASSERT(sigaction(SIGABRT, &sigabrt_action, &previous_sigabrt_action) == 0);
    handler_installed = 1;
    RECORD_ASSERT(mutex_object.initialize() == FT_ERR_SUCCESS);
    initialize_unlock_shared_state(&shared_state, &mutex_object);
    RECORD_ASSERT(mutex_object.lock() == FT_ERR_SUCCESS);
    if (pt_thread_create(&worker_thread, ft_nullptr, unlock_worker, &shared_state) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&shared_state.stage, 2));
    RECORD_ASSERT(shared_state.unlock_result.load() == FT_ERR_INVALID_ARGUMENT);
    RECORD_ASSERT(shared_state.unlock_error.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(g_sigabrt_received.load() == 0);
    RECORD_ASSERT(mutex_object.unlock() == FT_ERR_SUCCESS);
    goto cleanup;

cleanup:
    if (thread_created == 1)
    {
        int join_result;

        join_result = pt_thread_timed_join(worker_thread, ft_nullptr, 5000);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "pt_thread_timed_join(worker_thread, ft_nullptr, 5000) == 0";
            failure_line = __LINE__;
        }
    }
    if (handler_installed == 1)
    {
        sigaction(SIGABRT, &previous_sigabrt_action, ft_nullptr);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_mutex_unlock_without_locking)
{
    pt_mutex mutex_object;
    struct sigaction sigabrt_action;
    struct sigaction previous_sigabrt_action;
    int handler_installed;

    handler_installed = 0;
    sigabrt_action.sa_handler = handle_sigabrt;
    FT_ASSERT_EQ(0, sigemptyset(&sigabrt_action.sa_mask));
    sigabrt_action.sa_flags = 0;
    FT_ASSERT_EQ(0, sigaction(SIGABRT, &sigabrt_action, &previous_sigabrt_action));
    handler_installed = 1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, mutex_object.unlock());
    if (handler_installed == 1)
    {
        sigaction(SIGABRT, &previous_sigabrt_action, ft_nullptr);
    }
    FT_ASSERT_EQ(false, mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_mutex_try_lock_reports_already_locked)
{
    pt_mutex mutex_object;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED, mutex_object.try_lock());
    FT_ASSERT(mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.try_lock());
    FT_ASSERT(mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_mutex_try_lock_owned_by_other_thread)
{
    pt_mutex mutex_object;
    s_try_lock_shared_state shared_state;
    pthread_t worker_thread;
    int thread_created;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    int mutex_initialized;
#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    mutex_initialized = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    if (mutex_object.initialize() == FT_ERR_SUCCESS)
        mutex_initialized = 1;
    else
        RECORD_ASSERT(0);
    initialize_try_lock_shared_state(&shared_state, &mutex_object);
    if (pt_thread_create(&worker_thread, ft_nullptr, try_lock_worker,
            &shared_state) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&shared_state.stage, 2));
    RECORD_ASSERT(shared_state.lock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared_state.lock_error.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(mutex_object.try_lock() == FT_ERR_MUTEX_ALREADY_LOCKED);
    RECORD_ASSERT(mutex_object.lockState());
    shared_state.stage.store(3);
    RECORD_ASSERT(wait_for_stage(&shared_state.stage, 4));
    RECORD_ASSERT(shared_state.unlock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared_state.unlock_error.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(mutex_object.lock() == FT_ERR_SUCCESS);
    RECORD_ASSERT(mutex_object.lockState());
    RECORD_ASSERT(mutex_object.unlock() == FT_ERR_SUCCESS);
    goto cleanup;

cleanup:
    if (thread_created == 1)
    {
        int join_result;

        join_result = pt_thread_join(worker_thread, ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    if (mutex_initialized == 1)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_mutex_try_lock_returns_quickly_when_locked)
{
    pt_mutex mutex_object;
    s_timed_try_lock_shared_state shared_state;
    pthread_t worker_thread;
    int thread_created;
    int test_failed;
    int mutex_locked;
    int wait_attempts;
    const char *failure_expression;
    int failure_line;
#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    test_failed = 0;
    mutex_locked = 0;
    wait_attempts = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    RECORD_ASSERT(mutex_object.initialize() == FT_ERR_SUCCESS);
    RECORD_ASSERT(mutex_object.lock() == FT_ERR_SUCCESS);
    mutex_locked = 1;
    initialize_timed_try_lock_shared_state(&shared_state, &mutex_object);
    if (pt_thread_create(&worker_thread, ft_nullptr, timed_try_lock_worker,
            &shared_state) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    while (shared_state.stage.load() < 2 && wait_attempts < 200)
    {
        pt_thread_sleep(1);
        wait_attempts++;
    }
    RECORD_ASSERT(shared_state.stage.load() == 2);

cleanup:
    if (mutex_locked == 1)
    {
        if (mutex_object.unlock() == FT_ERR_SUCCESS)
            mutex_locked = 0;
        else if (test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "mutex_object.unlock() == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
    }
    if (thread_created == 1)
    {
        if (pt_thread_timed_join(worker_thread, ft_nullptr, 500) != 0
            && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "pt_thread_timed_join(worker_thread, ft_nullptr, 500) == 0";
            failure_line = __LINE__;
        }
    }
    if (test_failed == 0)
    {
        if (shared_state.try_lock_result.load() != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            test_failed = 1;
            failure_expression = "shared_state.try_lock_result.load() == FT_ERR_MUTEX_ALREADY_LOCKED";
            failure_line = __LINE__;
        }
        else if (shared_state.elapsed_ms.load() >= 50)
        {
            test_failed = 1;
            failure_expression = "shared_state.elapsed_ms.load() < 50";
            failure_line = __LINE__;
        }
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_recursive_mutex_try_lock_returns_quickly_when_locked)
{
    pt_recursive_mutex mutex_object;
    s_timed_recursive_try_lock_shared_state shared_state;
    pthread_t worker_thread;
    int thread_created;
    int test_failed;
    int mutex_locked;
    int wait_attempts;
    const char *failure_expression;
    int failure_line;
#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    test_failed = 0;
    mutex_locked = 0;
    wait_attempts = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    RECORD_ASSERT(mutex_object.initialize() == FT_ERR_SUCCESS);
    RECORD_ASSERT(mutex_object.lock() == FT_ERR_SUCCESS);
    mutex_locked = 1;
    initialize_timed_recursive_try_lock_shared_state(&shared_state,
            &mutex_object);
    if (pt_thread_create(&worker_thread, ft_nullptr,
            timed_recursive_try_lock_worker, &shared_state) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    while (shared_state.stage.load() < 2 && wait_attempts < 200)
    {
        pt_thread_sleep(1);
        wait_attempts++;
    }
    RECORD_ASSERT(shared_state.stage.load() == 2);

cleanup:
    if (mutex_locked == 1)
    {
        if (mutex_object.unlock() == FT_ERR_SUCCESS)
            mutex_locked = 0;
        else if (test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "mutex_object.unlock() == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
    }
    if (thread_created == 1)
    {
        if (pt_thread_timed_join(worker_thread, ft_nullptr, 500) != 0
            && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "pt_thread_timed_join(worker_thread, ft_nullptr, 500) == 0";
            failure_line = __LINE__;
        }
    }
    if (test_failed == 0)
    {
        if (shared_state.try_lock_result.load() != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            test_failed = 1;
            failure_expression = "shared_state.try_lock_result.load() == FT_ERR_MUTEX_ALREADY_LOCKED";
            failure_line = __LINE__;
        }
        else if (shared_state.elapsed_ms.load() >= 50)
        {
            test_failed = 1;
            failure_expression = "shared_state.elapsed_ms.load() < 50";
            failure_line = __LINE__;
        }
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_mutex_lock_reports_reentrant_lock)
{
    pt_mutex mutex_object;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED, mutex_object.lock());
    FT_ASSERT(mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_mutex_unlock_twice_reports_invalid_argument)
{
    pt_mutex mutex_object;
    int second_unlock_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    second_unlock_result = mutex_object.unlock();
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, second_unlock_result);
    FT_ASSERT_EQ(false, mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_mutex_recovers_after_already_locked_error)
{
    pt_mutex mutex_object;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.lock());
    FT_ASSERT(mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED, mutex_object.lock());
    FT_ASSERT(mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.unlock());
    FT_ASSERT_EQ(false, mutex_object.lockState());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_lock_tracking_reports_other_thread_mutexes)
{
    pt_mutex mutex_object;
    s_foreign_owned_shared_state shared_state;
    pthread_t worker_thread;
    pt_mutex_vector owned_mutexes;
    int thread_created;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    int owned_error;
    int mutex_initialized;

#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    thread_created = 0;
    mutex_initialized = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    if (mutex_object.initialize() == FT_ERR_SUCCESS)
        mutex_initialized = 1;
    else
        RECORD_ASSERT(0);
    initialize_foreign_owned_shared_state(&shared_state, &mutex_object);
    if (pt_thread_create(&worker_thread, ft_nullptr, foreign_owned_mutex_worker, &shared_state) != 0)
    {
        RECORD_ASSERT(0);
    }
    else
        thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&shared_state.stage, 2));
    RECORD_ASSERT(shared_state.lock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared_state.lock_error.load() == FT_ERR_SUCCESS);
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(shared_state.worker_thread_identifier, &owned_error);
    RECORD_ASSERT(owned_error == FT_ERR_SUCCESS);
    RECORD_ASSERT(static_cast<int>(owned_mutexes.size) == 1);
    pt_buffer_destroy(owned_mutexes);
    shared_state.stage.store(3);
    RECORD_ASSERT(wait_for_stage(&shared_state.stage, 4));
    RECORD_ASSERT(shared_state.unlock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(shared_state.unlock_error.load() == FT_ERR_SUCCESS);
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(shared_state.worker_thread_identifier, &owned_error);
    RECORD_ASSERT(owned_error == FT_ERR_SUCCESS);
    RECORD_ASSERT(static_cast<int>(owned_mutexes.size) == 0);
    pt_buffer_destroy(owned_mutexes);
    goto cleanup;

cleanup:
    if (thread_created == 1)
    {
        int join_result;

        join_result = pt_thread_join(worker_thread, ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (mutex_initialized == 1)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    #undef RECORD_ASSERT
    return (1);
}

FT_TEST(test_pt_lock_tracking_query_apis_do_not_mutate_registry_on_miss)
{
    s_thread_id_capture_state capture_state;
    pthread_t worker_thread;
    pt_mutex_vector owned_mutexes;
    s_pt_lock_tracking_thread_state state;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    ft_size_t size_before;
    ft_size_t size_after;
    int error_code;

    initialize_thread_id_capture_state(&capture_state);
    pt_buffer_init(state.owned_mutexes);
    FT_ASSERT_EQ(0, pt_thread_create(&worker_thread, ft_nullptr,
        thread_id_capture_worker, &capture_state));
    FT_ASSERT(wait_for_stage(&capture_state.stage, 1));
    FT_ASSERT(capture_state.thread_identifier.load() != pt_thread_id_type());
    FT_ASSERT_EQ(0, pt_thread_join(worker_thread, ft_nullptr));
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_NEQ(ft_nullptr, thread_infos);
    size_before = thread_infos->size;
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(
        capture_state.thread_identifier.load(), &error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(0U, owned_mutexes.size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::get_thread_state(
        capture_state.thread_identifier.load(), state));
    FT_ASSERT_EQ(capture_state.thread_identifier.load(), state.thread_identifier);
    FT_ASSERT_EQ(0U, state.owned_mutexes.size);
    FT_ASSERT_EQ(ft_nullptr, state.waiting_mutex);
    FT_ASSERT_EQ(0L, state.wait_started_ms);
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_NEQ(ft_nullptr, thread_infos);
    size_after = thread_infos->size;
    FT_ASSERT_EQ(size_before, size_after);
    pt_buffer_destroy(owned_mutexes);
    pt_buffer_destroy(state.owned_mutexes);
    return (1);
}

FT_TEST(test_pt_lock_tracking_detached_thread_cleans_up_automatically)
{
    s_detached_tracking_state state;
    pt_mutex mutex_object;
    pt_lock_wait_snapshot_vector snapshot;
    pthread_t worker_thread;
    ft_size_t baseline_size;
    int attempts;

    state.mutex_pointer = &mutex_object;
    state.stage.store(0);
    state.lock_result.store(FT_ERR_INVALID_STATE);
    state.unlock_result.store(FT_ERR_INVALID_STATE);
    state.thread_identifier.store(0);
    pt_buffer_init(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::snapshot_waiters(snapshot));
    baseline_size = snapshot.size;
    FT_ASSERT_EQ(0, pt_thread_create(&worker_thread, ft_nullptr,
        detached_tracking_worker, &state));
    FT_ASSERT_EQ(0, pt_thread_detach(worker_thread));
    FT_ASSERT(wait_for_stage(&state.stage, 2));
    attempts = 0;
    while (attempts < 5000)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::snapshot_waiters(snapshot));
        if (snapshot.size == baseline_size)
            break ;
        pt_thread_sleep(1);
        attempts += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.lock_result.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.unlock_result.load());
    FT_ASSERT_EQ(baseline_size, snapshot.size);
    pt_buffer_destroy(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_lock_tracking_owner_index_tracks_acquire_release)
{
    s_thread_id_capture_state capture_state;
    pthread_t worker_thread;
    pt_mutex mutex_object;
    pt_lock_wait_snapshot_vector snapshot;
    int snapshot_error;
    ft_size_t baseline_size;
    ft_size_t snapshot_index;

    initialize_thread_id_capture_state(&capture_state);
    pt_buffer_init(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    snapshot_error = pt_lock_tracking::snapshot_waiters(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_error);
    baseline_size = snapshot.size;
    FT_ASSERT_EQ(0, pt_thread_create(&worker_thread, ft_nullptr,
        thread_id_capture_worker, &capture_state));
    FT_ASSERT(wait_for_stage(&capture_state.stage, 1));
    FT_ASSERT(capture_state.thread_identifier.load() != pt_thread_id_type());
    FT_ASSERT_EQ(0, pt_thread_join(worker_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_acquired(
        capture_state.thread_identifier.load(),
        static_cast<const void *>(&mutex_object)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_wait(THREAD_ID,
        static_cast<const void *>(&mutex_object)));
    snapshot_error = pt_lock_tracking::snapshot_waiters(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_error);
    FT_ASSERT_EQ(baseline_size + 1U, snapshot.size);
    snapshot_index = find_wait_snapshot_index(snapshot,
        static_cast<const void *>(&mutex_object), THREAD_ID);
    FT_ASSERT_NEQ(snapshot.size, snapshot_index);
    FT_ASSERT_EQ(static_cast<const void *>(&mutex_object),
        snapshot.data[snapshot_index].mutex_pointer);
    FT_ASSERT_EQ(capture_state.thread_identifier.load(),
        snapshot.data[snapshot_index].owner_thread);
    FT_ASSERT_EQ(THREAD_ID, snapshot.data[snapshot_index].waiting_thread);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_released(
        capture_state.thread_identifier.load(),
        static_cast<const void *>(&mutex_object)));
    snapshot_error = pt_lock_tracking::snapshot_waiters(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_error);
    FT_ASSERT_EQ(baseline_size + 1U, snapshot.size);
    snapshot_index = find_wait_snapshot_index(snapshot,
        static_cast<const void *>(&mutex_object), THREAD_ID);
    FT_ASSERT_NEQ(snapshot.size, snapshot_index);
    FT_ASSERT_EQ(pt_thread_id_type(), snapshot.data[snapshot_index].owner_thread);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    pt_buffer_destroy(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_lock_tracking_notify_thread_exit_clears_stale_state)
{
    s_thread_id_capture_state capture_state;
    pthread_t worker_thread;
    pt_mutex first_mutex;
    pt_mutex second_mutex;
    s_pt_lock_tracking_thread_state state;
    pt_lock_wait_snapshot_vector snapshot;
    ft_size_t baseline_size;

    initialize_thread_id_capture_state(&capture_state);
    pt_buffer_init(state.owned_mutexes);
    pt_buffer_init(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::snapshot_waiters(snapshot));
    baseline_size = snapshot.size;
    FT_ASSERT_EQ(0, pt_thread_create(&worker_thread, ft_nullptr,
        thread_id_capture_worker, &capture_state));
    FT_ASSERT(wait_for_stage(&capture_state.stage, 1));
    FT_ASSERT_EQ(0, pt_thread_join(worker_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_acquired(
        capture_state.thread_identifier.load(),
        static_cast<const void *>(&first_mutex)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_wait(
        capture_state.thread_identifier.load(),
        static_cast<const void *>(&second_mutex)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(
        capture_state.thread_identifier.load()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::get_thread_state(
        capture_state.thread_identifier.load(), state));
    FT_ASSERT_EQ(0U, state.owned_mutexes.size);
    FT_ASSERT_EQ(ft_nullptr, state.waiting_mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::snapshot_waiters(snapshot));
    FT_ASSERT_EQ(baseline_size, snapshot.size);
    pt_buffer_destroy(snapshot);
    pt_buffer_destroy(state.owned_mutexes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.destroy());
    return (1);
}

FT_TEST(test_pt_lock_tracking_detect_cycle_reports_allocation_failure)
{
    s_thread_id_capture_state first_capture_state;
    s_thread_id_capture_state second_capture_state;
    pthread_t first_thread;
    pthread_t second_thread;
    pt_mutex first_mutex;
    pt_mutex second_mutex;
    pt_mutex third_mutex;
    s_pt_thread_lock_info origin;
    pt_mutex_vector visited_mutexes;
    pt_thread_vector visited_threads;
    bool cycle_detected;
    int detect_error;

    initialize_thread_id_capture_state(&first_capture_state);
    initialize_thread_id_capture_state(&second_capture_state);
    pt_buffer_init(origin.owned_mutexes);
    pt_buffer_init(visited_mutexes);
    pt_buffer_init(visited_threads);
    origin.thread_identifier = THREAD_ID;
    origin.waiting_mutex = ft_nullptr;
    origin.wait_started_ms = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third_mutex.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_buffer_push(origin.owned_mutexes,
        static_cast<const void *>(&first_mutex)));
    FT_ASSERT_EQ(0, pt_thread_create(&first_thread, ft_nullptr,
        thread_id_capture_worker, &first_capture_state));
    FT_ASSERT_EQ(0, pt_thread_create(&second_thread, ft_nullptr,
        thread_id_capture_worker, &second_capture_state));
    FT_ASSERT(wait_for_stage(&first_capture_state.stage, 1));
    FT_ASSERT(wait_for_stage(&second_capture_state.stage, 1));
    FT_ASSERT_EQ(0, pt_thread_join(first_thread, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(second_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_acquired(
        first_capture_state.thread_identifier.load(),
        static_cast<const void *>(&second_mutex)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_wait(
        first_capture_state.thread_identifier.load(),
        static_cast<const void *>(&third_mutex)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_acquired(
        second_capture_state.thread_identifier.load(),
        static_cast<const void *>(&third_mutex)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_wait(
        second_capture_state.thread_identifier.load(),
        static_cast<const void *>(&first_mutex)));
    pt_lock_tracking_detect_cycle_override_error_code.store(FT_ERR_NO_MEMORY);
    cycle_detected = false;
    detect_error = pt_lock_tracking::detect_cycle(&origin,
        static_cast<const void *>(&second_mutex), &visited_mutexes,
        &visited_threads, &cycle_detected);
    pt_lock_tracking_detect_cycle_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, detect_error);
    FT_ASSERT_EQ(false, cycle_detected);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(
        first_capture_state.thread_identifier.load()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(
        second_capture_state.thread_identifier.load()));
    pt_buffer_destroy(visited_threads);
    pt_buffer_destroy(visited_mutexes);
    pt_buffer_destroy(origin.owned_mutexes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third_mutex.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_mutex.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_mutex.destroy());
    return (1);
}

FT_TEST(test_pt_recursive_mutex_lock_notify_acquired_failure_clears_wait_state)
{
    pt_recursive_mutex mutex_object;
    s_recursive_holder_shared_state holder_state;
    s_recursive_contender_shared_state contender_state;
    pthread_t holder_thread;
    pthread_t contender_thread;
    s_pt_lock_tracking_thread_state thread_state;
    int holder_thread_created;
    int contender_thread_created;
    int contender_thread_joined;
    int mutex_initialized;
    int test_failed;
    const char *failure_expression;
    int failure_line;

#define RECORD_ASSERT(expression) \
    if (!(expression) && test_failed == 0) \
    { \
        test_failed = 1; \
        failure_expression = #expression; \
        failure_line = __LINE__; \
        goto cleanup; \
    }

    initialize_recursive_holder_shared_state(&holder_state, &mutex_object);
    initialize_recursive_contender_shared_state(&contender_state, &mutex_object);
    pt_buffer_init(thread_state.owned_mutexes);
    holder_thread_created = 0;
    contender_thread_created = 0;
    contender_thread_joined = 0;
    mutex_initialized = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    if (mutex_object.initialize() != FT_ERR_SUCCESS)
        RECORD_ASSERT(0);
    mutex_initialized = 1;
    if (pt_thread_create(&holder_thread, ft_nullptr, recursive_holder_worker,
            &holder_state) != 0)
        RECORD_ASSERT(0);
    holder_thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&holder_state.stage, 2));
    RECORD_ASSERT(holder_state.lock_result.load() == FT_ERR_SUCCESS);
    if (pt_thread_create(&contender_thread, ft_nullptr,
            recursive_contender_worker, &contender_state) != 0)
        RECORD_ASSERT(0);
    contender_thread_created = 1;
    RECORD_ASSERT(wait_for_stage(&contender_state.stage, 1));
    pt_lock_tracking_notify_acquired_override_error_code.store(FT_ERR_NO_MEMORY);
    contender_state.allow_lock.store(1);
    holder_state.stage.store(3);
    RECORD_ASSERT(wait_for_stage(&holder_state.stage, 4));
    RECORD_ASSERT(holder_state.unlock_result.load() == FT_ERR_SUCCESS);
    RECORD_ASSERT(wait_for_stage(&contender_state.stage, 2));
    RECORD_ASSERT(pt_thread_join(contender_thread, ft_nullptr) == 0);
    contender_thread_joined = 1;
    pt_lock_tracking_notify_acquired_override_error_code.store(FT_ERR_SUCCESS);
    RECORD_ASSERT(contender_state.lock_result.load() == FT_ERR_NO_MEMORY);
    RECORD_ASSERT(pt_lock_tracking::get_thread_state(
        contender_state.thread_identifier.load(), thread_state)
        == FT_ERR_SUCCESS);
    RECORD_ASSERT(thread_state.owned_mutexes.size == 0U);
    RECORD_ASSERT(thread_state.waiting_mutex == ft_nullptr);
    RECORD_ASSERT(thread_state.wait_started_ms == 0L);

cleanup:
    pt_lock_tracking_notify_acquired_override_error_code.store(FT_ERR_SUCCESS);
    if (holder_thread_created == 1 && holder_state.stage.load() < 3)
        holder_state.stage.store(3);
    if (contender_thread_created == 1 && contender_thread_joined == 0)
        (void)pt_thread_join(contender_thread, ft_nullptr);
    if (holder_thread_created == 1)
        (void)pt_thread_join(holder_thread, ft_nullptr);
    pt_buffer_destroy(thread_state.owned_mutexes);
    if (mutex_initialized == 1)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        #undef RECORD_ASSERT
        return (0);
    }
    #undef RECORD_ASSERT
    return (1);
}

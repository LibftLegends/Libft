#include "../test_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>

struct s_rwlock_reader_state
{
    t_pt_rwlock *lock;
    std::atomic<int> *started;
    std::atomic<int> *active;
    std::atomic<int> *release;
    std::atomic<int> *sequence;
    std::atomic<int> *acquired_sequence;
    std::atomic<int> *ready;
};

struct s_rwlock_writer_state
{
    t_pt_rwlock *lock;
    std::atomic<int> ready;
    std::atomic<int> go;
    std::atomic<int> queued;
    std::atomic<int> result;
    std::atomic<int> acquired_sequence;
    int identifier;
    std::atomic<int> *sequence;
    std::atomic<int> *order;
};

struct s_rwlock_unlock_state
{
    t_pt_rwlock *lock;
    std::atomic<int> result;
};

static ft_bool rwlock_wait_for_value(const std::atomic<int> &value, int expected)
{
    int attempts;

    attempts = 0;
    while (value.load() != expected && attempts < 5000)
    {
        pt_thread_sleep(1);
        attempts += 1;
    }
    if (value.load() == expected)
        return (FT_TRUE);
    return (FT_FALSE);
}

static void *rwlock_reader_routine(void *argument)
{
    s_rwlock_reader_state *state;
    int lock_result;
    int sequence_value;

    state = static_cast<s_rwlock_reader_state *>(argument);
    if (state->ready != ft_nullptr)
        state->ready->fetch_add(1);
    lock_result = pt_rwlock_strategy_rdlock(state->lock);
    if (lock_result != FT_ERR_SUCCESS)
        return (ft_nullptr);
    sequence_value = state->sequence->fetch_add(1) + 1;
    state->acquired_sequence->store(sequence_value);
    state->active->fetch_add(1);
    state->started->fetch_add(1);
    while (state->release->load() == 0)
        pt_thread_sleep(1);
    state->active->fetch_sub(1);
    (void)pt_rwlock_strategy_rdunlock(state->lock);
    return (ft_nullptr);
}

static void *rwlock_writer_routine(void *argument)
{
    s_rwlock_writer_state *state;
    int lock_result;
    int sequence_value;

    state = static_cast<s_rwlock_writer_state *>(argument);
    state->ready.store(1);
    while (state->go.load() == 0)
        pt_thread_sleep(1);
    state->queued.store(1);
    lock_result = pt_rwlock_strategy_wrlock(state->lock);
    state->result.store(lock_result);
    if (lock_result != FT_ERR_SUCCESS)
        return (ft_nullptr);
    sequence_value = state->sequence->fetch_add(1) + 1;
    state->acquired_sequence.store(sequence_value);
    if (state->order != ft_nullptr)
        state->order[sequence_value - 1].store(state->identifier);
    (void)pt_rwlock_strategy_wrunlock(state->lock);
    return (ft_nullptr);
}

static void *rwlock_foreign_unlock_routine(void *argument)
{
    s_rwlock_unlock_state *state;
    int unlock_result;

    state = static_cast<s_rwlock_unlock_state *>(argument);
    unlock_result = pt_rwlock_strategy_unlock(state->lock);
    state->result.store(unlock_result);
    return (ft_nullptr);
}

static int rwlock_initialize(t_pt_rwlock *lock)
{
    return (pt_rwlock_strategy_init(lock, PT_RWLOCK_STRATEGY_WRITER_PRIORITY));
}

static void rwlock_initialize_writer_state(s_rwlock_writer_state *state,
    t_pt_rwlock *lock, int identifier, std::atomic<int> *sequence,
    std::atomic<int> *order)
{
    state->lock = lock;
    state->ready.store(0);
    state->go.store(0);
    state->queued.store(0);
    state->result.store(FT_ERR_INTERNAL);
    state->acquired_sequence.store(0);
    state->identifier = identifier;
    state->sequence = sequence;
    state->order = order;
    return ;
}

FT_TEST(test_pt_rwlock_strategy_readers_enter_together)
{
    t_pt_rwlock lock;
    std::atomic<int> started(0);
    std::atomic<int> active(0);
    std::atomic<int> release(0);
    std::atomic<int> sequence(0);
    std::atomic<int> sequence_one(0);
    std::atomic<int> sequence_two(0);
    s_rwlock_reader_state state_one;
    s_rwlock_reader_state state_two;
    pthread_t thread_one;
    pthread_t thread_two;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    state_one = {&lock, &started, &active, &release, &sequence, &sequence_one,
        ft_nullptr};
    state_two = {&lock, &started, &active, &release, &sequence, &sequence_two,
        ft_nullptr};
    FT_ASSERT_EQ(0, pt_thread_create(&thread_one, ft_nullptr,
            rwlock_reader_routine, &state_one));
    FT_ASSERT_EQ(0, pt_thread_create(&thread_two, ft_nullptr,
            rwlock_reader_routine, &state_two));
    FT_ASSERT(rwlock_wait_for_value(started, 2));
    FT_ASSERT_EQ(2, active.load());
    FT_ASSERT(sequence_one.load() != sequence_two.load());
    release.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(thread_one, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(thread_two, ft_nullptr));
    FT_ASSERT_EQ(0, active.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_uncontended_readers_use_fast_path)
{
    t_pt_rwlock lock;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(1U, lock.fast_active_readers.load());
    FT_ASSERT_EQ(0U, lock.active_readers);
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_try_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_unlock(&lock));
    FT_ASSERT_EQ(0U, lock.fast_active_readers.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_writer_closes_reader_gate)
{
    t_pt_rwlock lock;
    std::atomic<int> started(0);
    std::atomic<int> active(0);
    std::atomic<int> release(0);
    std::atomic<int> sequence(0);
    std::atomic<int> blocked_reader_sequence(0);
    s_rwlock_reader_state reader_state;
    s_rwlock_writer_state writer_state;
    pthread_t reader_thread;
    pthread_t writer_thread;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    rwlock_initialize_writer_state(&writer_state, &lock, 1, &sequence,
        ft_nullptr);
    reader_state = {&lock, &started, &active, &release, &sequence,
        &blocked_reader_sequence, ft_nullptr};
    FT_ASSERT_EQ(0, pt_thread_create(&writer_thread, ft_nullptr,
            rwlock_writer_routine, &writer_state));
    FT_ASSERT(rwlock_wait_for_value(writer_state.ready, 1));
    writer_state.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_state.queued, 1));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_thread, ft_nullptr,
            rwlock_reader_routine, &reader_state));
    pt_thread_sleep(20);
    FT_ASSERT_EQ(0, started.load());
    FT_ASSERT_EQ(0, writer_state.acquired_sequence.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(0, pt_thread_join(writer_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer_state.result.load());
    release.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(reader_thread, ft_nullptr));
    FT_ASSERT(writer_state.acquired_sequence.load() <
        blocked_reader_sequence.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_writers_are_ticket_ordered)
{
    t_pt_rwlock lock;
    std::atomic<int> sequence(0);
    std::atomic<int> order[3];
    s_rwlock_writer_state states[3];
    pthread_t threads[3];
    int index;

    index = 0;
    while (index < 3)
    {
        order[index].store(0);
        index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    index = 0;
    while (index < 3)
    {
        rwlock_initialize_writer_state(&states[index], &lock, index + 1,
            &sequence, order);
        FT_ASSERT_EQ(0, pt_thread_create(&threads[index], ft_nullptr,
                rwlock_writer_routine, &states[index]));
        FT_ASSERT(rwlock_wait_for_value(states[index].ready, 1));
        states[index].go.store(1);
        FT_ASSERT(rwlock_wait_for_value(states[index].queued, 1));
        pt_thread_sleep(10);
        index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    index = 0;
    while (index < 3)
    {
        FT_ASSERT_EQ(0, pt_thread_join(threads[index], ft_nullptr));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, states[index].result.load());
        index += 1;
    }
    FT_ASSERT_EQ(1, order[0].load());
    FT_ASSERT_EQ(2, order[1].load());
    FT_ASSERT_EQ(3, order[2].load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_reader_priority_preserves_legacy_admission)
{
    t_pt_rwlock lock;
    std::atomic<int> reader_started(0);
    std::atomic<int> reader_active(0);
    std::atomic<int> reader_release(0);
    std::atomic<int> sequence(0);
    std::atomic<int> reader_sequence(0);
    s_rwlock_reader_state reader_state;
    s_rwlock_writer_state writer_state;
    pthread_t reader_thread;
    pthread_t writer_thread;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_init(&lock,
            PT_RWLOCK_STRATEGY_READER_PRIORITY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    reader_state = {&lock, &reader_started, &reader_active,
        &reader_release, &sequence, &reader_sequence, ft_nullptr};
    rwlock_initialize_writer_state(&writer_state, &lock, 1, &sequence,
        ft_nullptr);
    FT_ASSERT_EQ(0, pt_thread_create(&writer_thread, ft_nullptr,
            rwlock_writer_routine, &writer_state));
    FT_ASSERT(rwlock_wait_for_value(writer_state.ready, 1));
    writer_state.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_state.queued, 1));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_thread, ft_nullptr,
            rwlock_reader_routine, &reader_state));
    FT_ASSERT(rwlock_wait_for_value(reader_started, 1));
    FT_ASSERT_EQ(0, writer_state.acquired_sequence.load());
    reader_release.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(reader_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(0, pt_thread_join(writer_thread, ft_nullptr));
    FT_ASSERT(reader_sequence.load() < writer_state.acquired_sequence.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer_state.result.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_rejects_recursive_and_wrong_mode_locks)
{
    t_pt_rwlock lock;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_NOT_OWNER,
        pt_rwlock_strategy_wrunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_NOT_OWNER,
        pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_destroy_busy_and_try_lock)
{
    t_pt_rwlock lock;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_try_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED,
        pt_rwlock_strategy_try_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, pt_rwlock_strategy_destroy(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_try_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, pt_rwlock_strategy_try_rdlock(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_uninitialised_lifecycle)
{
    t_pt_rwlock lock;

    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_init(&lock,
        PT_RWLOCK_STRATEGY_WRITER_PRIORITY));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, pt_rwlock_strategy_init(&lock,
        PT_RWLOCK_STRATEGY_WRITER_PRIORITY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_batches_readers_at_phase_cutoff)
{
    t_pt_rwlock lock;
    std::atomic<int> sequence(0);
    std::atomic<int> started(0);
    std::atomic<int> active(0);
    std::atomic<int> release(0);
    std::atomic<int> reader_one_sequence(0);
    std::atomic<int> reader_two_sequence(0);
    std::atomic<int> reader_three_sequence(0);
    std::atomic<int> reader_ready(0);
    std::atomic<int> reader_three_ready(0);
    s_rwlock_reader_state reader_one;
    s_rwlock_reader_state reader_two;
    s_rwlock_reader_state reader_three;
    s_rwlock_writer_state writer_one;
    s_rwlock_writer_state writer_two;
    pthread_t reader_one_thread;
    pthread_t reader_two_thread;
    pthread_t reader_three_thread;
    pthread_t writer_one_thread;
    pthread_t writer_two_thread;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    reader_one = {&lock, &started, &active, &release, &sequence,
        &reader_one_sequence, &reader_ready};
    reader_two = {&lock, &started, &active, &release, &sequence,
        &reader_two_sequence, &reader_ready};
    reader_three = {&lock, &started, &active, &release, &sequence,
        &reader_three_sequence, &reader_three_ready};
    rwlock_initialize_writer_state(&writer_one, &lock, 1, &sequence,
        ft_nullptr);
    rwlock_initialize_writer_state(&writer_two, &lock, 2, &sequence,
        ft_nullptr);
    FT_ASSERT_EQ(0, pt_thread_create(&writer_one_thread, ft_nullptr,
        rwlock_writer_routine, &writer_one));
    FT_ASSERT(rwlock_wait_for_value(writer_one.ready, 1));
    writer_one.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_one.queued, 1));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_one_thread, ft_nullptr,
        rwlock_reader_routine, &reader_one));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_two_thread, ft_nullptr,
        rwlock_reader_routine, &reader_two));
    FT_ASSERT(rwlock_wait_for_value(reader_ready, 2));
    FT_ASSERT_EQ(0, active.load());
    FT_ASSERT_EQ(0, pt_thread_create(&writer_two_thread, ft_nullptr,
        rwlock_writer_routine, &writer_two));
    FT_ASSERT(rwlock_wait_for_value(writer_two.ready, 1));
    writer_two.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_two.queued, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(0, pt_thread_join(writer_one_thread, ft_nullptr));
    FT_ASSERT(rwlock_wait_for_value(active, 2));
    FT_ASSERT_EQ(0, pt_thread_create(&reader_three_thread, ft_nullptr,
        rwlock_reader_routine, &reader_three));
    FT_ASSERT(rwlock_wait_for_value(reader_three_ready, 1));
    pt_thread_sleep(20);
    FT_ASSERT_EQ(2, active.load());
    FT_ASSERT_EQ(0, reader_three_sequence.load());
    release.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(reader_one_thread, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(reader_two_thread, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(writer_two_thread, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(reader_three_thread, ft_nullptr));
    FT_ASSERT(writer_two.acquired_sequence.load() >
        reader_one_sequence.load());
    FT_ASSERT(writer_two.acquired_sequence.load() >
        reader_two_sequence.load());
    FT_ASSERT(reader_three_sequence.load() >
        writer_two.acquired_sequence.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_destroy_rejects_waiting_threads)
{
    t_pt_rwlock lock;
    std::atomic<int> sequence(0);
    s_rwlock_writer_state writer_state;
    pthread_t writer_thread;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    rwlock_initialize_writer_state(&writer_state, &lock, 1, &sequence,
        ft_nullptr);
    FT_ASSERT_EQ(0, pt_thread_create(&writer_thread, ft_nullptr,
            rwlock_writer_routine, &writer_state));
    FT_ASSERT(rwlock_wait_for_value(writer_state.ready, 1));
    writer_state.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_state.queued, 1));
    pt_thread_sleep(20);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, pt_rwlock_strategy_destroy(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(0, pt_thread_join(writer_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer_state.result.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_rejects_foreign_unlock_and_invalid_init)
{
    t_pt_rwlock lock;
    s_rwlock_unlock_state unlock_state;
    pthread_t unlock_thread;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, pt_rwlock_strategy_init(ft_nullptr,
            PT_RWLOCK_STRATEGY_WRITER_PRIORITY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    unlock_state.lock = &lock;
    unlock_state.result.store(FT_ERR_INTERNAL);
    FT_ASSERT_EQ(0, pt_thread_create(&unlock_thread, ft_nullptr,
            rwlock_foreign_unlock_routine, &unlock_state));
    FT_ASSERT_EQ(0, pt_thread_join(unlock_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_MUTEX_NOT_OWNER, unlock_state.result.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_normalizes_empty_ticket_counters)
{
    t_pt_rwlock lock;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    lock.next_writer_ticket = UINT64_MAX;
    lock.serving_writer_ticket = UINT64_MAX;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    return (1);
}

FT_TEST(test_pt_rwlock_strategy_failure_paths_roll_back_and_wake)
{
    t_pt_rwlock lock;
    uint32_t failure_stage;
    int initialize_result;
    int lock_result;
    std::atomic<int> sequence(0);
    s_rwlock_writer_state writer_state;
    s_rwlock_writer_state later_writer_state;
    pthread_t writer_thread;
    pthread_t later_writer_thread;

    failure_stage = PT_RWLOCK_TEST_FAIL_MUTEX_INIT;
    while (failure_stage <= PT_RWLOCK_TEST_FAIL_WRITER_CONDITION_INIT)
    {
        pt_rwlock_test_failure_stage.store(failure_stage);
        initialize_result = pt_rwlock_strategy_init(&lock,
                PT_RWLOCK_STRATEGY_WRITER_PRIORITY);
        FT_ASSERT(initialize_result != FT_ERR_SUCCESS);
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED,
            lock.initialised_state.load());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
        failure_stage += 1U;
    }
    pt_rwlock_test_failure_stage.store(
            PT_RWLOCK_TEST_FAIL_READER_BOOKKEEPING);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    lock_result = pt_rwlock_strategy_rdlock(&lock);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, lock_result);
    FT_ASSERT_EQ(0, lock.active_readers);
    FT_ASSERT_EQ(0, lock.waiting_readers);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rwlock_initialize(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdlock(&lock));
    rwlock_initialize_writer_state(&writer_state, &lock, 1, &sequence,
        ft_nullptr);
    FT_ASSERT_EQ(0, pt_thread_create(&writer_thread, ft_nullptr,
            rwlock_writer_routine, &writer_state));
    FT_ASSERT(rwlock_wait_for_value(writer_state.ready, 1));
    pt_rwlock_test_failure_stage.store(PT_RWLOCK_TEST_FAIL_CONDITION_WAIT);
    writer_state.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(writer_state.queued, 1));
    FT_ASSERT_EQ(0, pt_thread_join(writer_thread, ft_nullptr));
    lock_result = writer_state.result.load();
    FT_ASSERT(lock_result != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(0, lock.waiting_writers);
    rwlock_initialize_writer_state(&later_writer_state, &lock, 2,
        &sequence, ft_nullptr);
    FT_ASSERT_EQ(0, pt_thread_create(&later_writer_thread, ft_nullptr,
            rwlock_writer_routine, &later_writer_state));
    FT_ASSERT(rwlock_wait_for_value(later_writer_state.ready, 1));
    later_writer_state.go.store(1);
    FT_ASSERT(rwlock_wait_for_value(later_writer_state.queued, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&lock));
    FT_ASSERT_EQ(0, pt_thread_join(later_writer_thread, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, later_writer_state.result.load());
    pt_rwlock_test_failure_stage.store(PT_RWLOCK_TEST_FAIL_CONDITION_WAKE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_wrunlock(&lock));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_destroy(&lock));
    pt_rwlock_test_failure_stage.store(0U);
    return (1);
}

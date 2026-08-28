#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include "pthread.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"

#ifdef LIBFT_TEST_BUILD
std::atomic<uint32_t> pt_rwlock_test_failure_stage(0U);

static int pt_rwlock_test_take_failure(uint32_t failure_stage)
{
    uint32_t expected_stage;

    expected_stage = failure_stage;
    if (pt_rwlock_test_failure_stage.compare_exchange_strong(expected_stage,
            0U, std::memory_order_acq_rel) == true)
        return (FT_TRUE);
    return (FT_FALSE);
}
#endif

static int pt_rwlock_report_result(int return_value)
{
    return (return_value);
}

static int pt_rwlock_report_system_error(int system_error)
{
    int mapped_error;

    mapped_error = cmp_map_system_error_to_ft(system_error);
    return (pt_rwlock_report_result(mapped_error));
}

static int pt_rwlock_strategy_report_result(t_pt_rwlock *rwlock,
    int error_code, int return_value)
{
    if (rwlock != ft_nullptr)
        rwlock->error_code.store(error_code, std::memory_order_release);
    return (return_value);
}

static int pt_rwlock_strategy_lock_mutex(t_pt_rwlock *rwlock)
{
    int system_error;

    system_error = pthread_mutex_lock(&rwlock->mutex);
    if (system_error != 0)
        return (cmp_map_system_error_to_ft(system_error));
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_try_lock_mutex(t_pt_rwlock *rwlock)
{
    int system_error;

    system_error = pthread_mutex_trylock(&rwlock->mutex);
    if (system_error != 0)
    {
        if (system_error == EBUSY)
            return (FT_ERR_THREAD_BUSY);
        return (cmp_map_system_error_to_ft(system_error));
    }
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_unlock_mutex(t_pt_rwlock *rwlock)
{
    int system_error;

    system_error = pthread_mutex_unlock(&rwlock->mutex);
    if (system_error != 0)
        return (cmp_map_system_error_to_ft(system_error));
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_broadcast(pthread_cond_t *condition)
{
    int system_error;

#ifdef LIBFT_TEST_BUILD
    if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_CONDITION_WAKE)
        == FT_TRUE)
        system_error = EAGAIN;
    else
        system_error = pthread_cond_broadcast(condition);
    if (system_error != 0)
        system_error = pthread_cond_broadcast(condition);
#else
    system_error = pthread_cond_broadcast(condition);
#endif
    if (system_error != 0)
        return (cmp_map_system_error_to_ft(system_error));
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_close_empty_reader_phase(
    t_pt_rwlock *rwlock)
{
    if (rwlock->reader_phase_open == FT_TRUE
        && rwlock->active_readers == 0
        && rwlock->waiting_readers == 0)
    {
        rwlock->reader_phase_open = FT_FALSE;
        if (rwlock->waiting_writers != 0)
            return (pt_rwlock_strategy_broadcast(
                    &rwlock->writer_condition));
    }
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_thread_in_readers(const t_pt_rwlock *rwlock,
    pt_thread_id_type thread_id, ft_size_t *reader_index)
{
    ft_size_t index;

    index = 0;
    while (index < rwlock->active_reader_threads.size)
    {
        if (pt_thread_equal(rwlock->active_reader_threads.data[index],
                thread_id) != 0)
        {
            if (reader_index != ft_nullptr)
                *reader_index = index;
            return (FT_TRUE);
        }
        index += 1;
    }
    return (FT_FALSE);
}

static int pt_rwlock_strategy_advance_cancelled(t_pt_rwlock *rwlock)
{
    ft_size_t index;

    index = 0;
    while (index < rwlock->cancelled_writer_tickets.size)
    {
        if (rwlock->cancelled_writer_tickets.data[index]
            == rwlock->serving_writer_ticket)
        {
            pt_buffer_erase(rwlock->cancelled_writer_tickets, index);
            if (rwlock->serving_writer_ticket == UINT64_MAX)
                return (FT_ERR_OUT_OF_RANGE);
            rwlock->serving_writer_ticket += 1U;
            index = 0;
        }
        else
            index += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int pt_rwlock_strategy_cancel_ticket(t_pt_rwlock *rwlock,
    uint64_t ticket)
{
    int error_code;

    error_code = pt_buffer_push(rwlock->cancelled_writer_tickets, ticket);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (ticket == rwlock->serving_writer_ticket)
        error_code = pt_rwlock_strategy_advance_cancelled(rwlock);
    return (error_code);
}

static int pt_rwlock_strategy_finish_writer_queue(t_pt_rwlock *rwlock)
{
    int error_code;

    if (rwlock->waiting_writers == 0 && rwlock->writer_active == FT_FALSE
        && rwlock->next_writer_ticket == rwlock->serving_writer_ticket)
    {
        rwlock->serving_writer_ticket = 0U;
        rwlock->next_writer_ticket = 0U;
        return (FT_ERR_SUCCESS);
    }
    if (rwlock->serving_writer_ticket == UINT64_MAX)
    {
        if (rwlock->waiting_writers != 0 || rwlock->writer_active != FT_FALSE)
            return (FT_ERR_OUT_OF_RANGE);
        rwlock->serving_writer_ticket = 0U;
        rwlock->next_writer_ticket = 0U;
    }
    error_code = pt_rwlock_strategy_advance_cancelled(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (rwlock->waiting_writers == 0 && rwlock->writer_active == FT_FALSE
        && rwlock->next_writer_ticket == rwlock->serving_writer_ticket
        && rwlock->cancelled_writer_tickets.size == 0)
    {
        rwlock->serving_writer_ticket = 0U;
        rwlock->next_writer_ticket = 0U;
    }
    return (FT_ERR_SUCCESS);
}

int pt_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attributes)
{
    int return_value;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_report_result(-1));
    return_value = pthread_rwlock_init(rwlock, attributes);
    return (pt_rwlock_report_result(return_value));
}

int pt_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    int return_value;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_report_result(-1));
    return_value = pthread_rwlock_rdlock(rwlock);
    return (pt_rwlock_report_result(return_value));
}

int pt_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    int return_value;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_report_result(-1));
    return_value = pthread_rwlock_wrlock(rwlock);
    return (pt_rwlock_report_result(return_value));
}

int pt_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    int return_value;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_report_result(-1));
    return_value = pthread_rwlock_unlock(rwlock);
    return (pt_rwlock_report_result(return_value));
}

int pt_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    int return_value;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_report_result(-1));
    return_value = pthread_rwlock_destroy(rwlock);
    return (pt_rwlock_report_result(return_value));
}

int32_t pt_rwlock_strategy_init(t_pt_rwlock *rwlock, t_pt_rwlock_strategy strategy)
{
    int system_error;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (strategy != PT_RWLOCK_STRATEGY_READER_PRIORITY
        && strategy != PT_RWLOCK_STRATEGY_WRITER_PRIORITY)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (rwlock->initialised_state == FT_CLASS_STATE_INITIALISED)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_STATE, FT_ERR_INVALID_STATE));
    rwlock->initialised_state = FT_CLASS_STATE_UNINITIALISED;
    rwlock->active_readers = 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_writers = 0;
    rwlock->next_writer_ticket = 0U;
    rwlock->serving_writer_ticket = 0U;
    rwlock->next_reader_ticket = 0U;
    rwlock->reader_phase_cutoff = 0U;
    rwlock->active_writer_thread = 0;
    rwlock->writer_active = FT_FALSE;
    rwlock->active_writer_has_ticket = FT_FALSE;
    rwlock->reader_phase_open = FT_FALSE;
    pt_buffer_init(rwlock->active_reader_threads);
    pt_buffer_init(rwlock->cancelled_writer_tickets);
    rwlock->strategy = strategy;
    rwlock->error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
#ifdef LIBFT_TEST_BUILD
    if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_MUTEX_INIT)
        == FT_TRUE)
        system_error = EAGAIN;
    else
#endif
        system_error = pthread_mutex_init(&rwlock->mutex, ft_nullptr);
    if (system_error != 0)
    {
        rwlock->initialised_state = FT_CLASS_STATE_DESTROYED;
        return (pt_rwlock_strategy_report_result(rwlock,
                cmp_map_system_error_to_ft(system_error),
                pt_rwlock_report_system_error(system_error)));
    }
#ifdef LIBFT_TEST_BUILD
    if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_READER_CONDITION_INIT)
        == FT_TRUE)
        system_error = EAGAIN;
    else
#endif
        system_error = pthread_cond_init(&rwlock->reader_condition, ft_nullptr);
    if (system_error != 0)
    {
        pthread_mutex_destroy(&rwlock->mutex);
        rwlock->initialised_state = FT_CLASS_STATE_DESTROYED;
        return (pt_rwlock_strategy_report_result(rwlock,
                cmp_map_system_error_to_ft(system_error),
                pt_rwlock_report_system_error(system_error)));
    }
#ifdef LIBFT_TEST_BUILD
    if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_WRITER_CONDITION_INIT)
        == FT_TRUE)
        system_error = EAGAIN;
    else
#endif
        system_error = pthread_cond_init(&rwlock->writer_condition, ft_nullptr);
    if (system_error != 0)
    {
        pthread_cond_destroy(&rwlock->reader_condition);
        pthread_mutex_destroy(&rwlock->mutex);
        rwlock->initialised_state = FT_CLASS_STATE_DESTROYED;
        return (pt_rwlock_strategy_report_result(rwlock,
                cmp_map_system_error_to_ft(system_error),
                pt_rwlock_report_system_error(system_error)));
    }
    rwlock->initialised_state = FT_CLASS_STATE_INITIALISED;
    return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_SUCCESS,
            FT_ERR_SUCCESS));
}

static int pt_rwlock_strategy_read_lock_internal(t_pt_rwlock *rwlock,
    ft_bool try_only)
{
    pt_thread_id_type thread_id;
    uint64_t reader_ticket;
    ft_bool has_reader_ticket;
    int error_code;
    int system_error;
    int bookkeeping_error;

    if (rwlock == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    thread_id = pt_thread_self();
    if (try_only == FT_TRUE)
        error_code = pt_rwlock_strategy_try_lock_mutex(rwlock);
    else
        error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_INVALID_STATE);
    }
    reader_ticket = 0U;
    has_reader_ticket = FT_FALSE;
    if (pt_rwlock_strategy_thread_in_readers(rwlock, thread_id, ft_nullptr)
        == FT_TRUE || (rwlock->writer_active == FT_TRUE
            && pt_thread_equal(rwlock->active_writer_thread, thread_id) != 0))
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_MUTEX_ALREADY_LOCKED);
    }
    if (try_only == FT_TRUE
        && (rwlock->writer_active == FT_TRUE
            || (rwlock->strategy == PT_RWLOCK_STRATEGY_WRITER_PRIORITY
                && (rwlock->waiting_writers != 0
                    || rwlock->reader_phase_open == FT_TRUE))))
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_THREAD_BUSY);
    }
    if (rwlock->writer_active == FT_TRUE
        || (rwlock->strategy == PT_RWLOCK_STRATEGY_WRITER_PRIORITY
            && (rwlock->waiting_writers != 0
                || rwlock->reader_phase_open == FT_TRUE)))
    {
        if (rwlock->next_reader_ticket == UINT64_MAX)
        {
            if (rwlock->waiting_readers == 0 && rwlock->active_readers == 0
                && rwlock->reader_phase_open == FT_FALSE)
                rwlock->next_reader_ticket = 0U;
            else
            {
                pt_rwlock_strategy_unlock_mutex(rwlock);
                return (FT_ERR_OUT_OF_RANGE);
            }
        }
        reader_ticket = rwlock->next_reader_ticket;
        rwlock->next_reader_ticket += 1U;
        has_reader_ticket = FT_TRUE;
        rwlock->waiting_readers += 1U;
    }
    while (rwlock->writer_active == FT_TRUE
        || (rwlock->strategy == PT_RWLOCK_STRATEGY_WRITER_PRIORITY
            && ((rwlock->reader_phase_open == FT_TRUE
                    && (has_reader_ticket == FT_FALSE
                        || reader_ticket >= rwlock->reader_phase_cutoff))
                || (rwlock->reader_phase_open == FT_FALSE
                    && rwlock->waiting_writers != 0))))
    {
        if (try_only == FT_TRUE)
        {
            rwlock->waiting_readers -= 1U;
            pt_rwlock_strategy_unlock_mutex(rwlock);
            return (FT_ERR_THREAD_BUSY);
        }
#ifdef LIBFT_TEST_BUILD
        if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_CONDITION_WAIT)
            == FT_TRUE)
            system_error = EAGAIN;
        else
#endif
            system_error = pthread_cond_wait(&rwlock->reader_condition,
                    &rwlock->mutex);
        if (system_error != 0)
        {
            rwlock->waiting_readers -= 1U;
            error_code = pt_rwlock_strategy_close_empty_reader_phase(
                    rwlock);
            pt_rwlock_strategy_unlock_mutex(rwlock);
            if (error_code != FT_ERR_SUCCESS)
                return (pt_rwlock_strategy_report_result(rwlock,
                        error_code, error_code));
            return (cmp_map_system_error_to_ft(system_error));
        }
    }
    #ifdef LIBFT_TEST_BUILD
    if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_READER_BOOKKEEPING)
        == FT_TRUE)
        error_code = FT_ERR_NO_MEMORY;
    else
    #endif
        error_code = pt_buffer_push(rwlock->active_reader_threads, thread_id);
    if (error_code != FT_ERR_SUCCESS)
    {
        bookkeeping_error = error_code;
        if (has_reader_ticket == FT_TRUE)
            rwlock->waiting_readers -= 1U;
        error_code = pt_rwlock_strategy_close_empty_reader_phase(rwlock);
        pt_rwlock_strategy_unlock_mutex(rwlock);
        if (error_code != FT_ERR_SUCCESS)
            return (pt_rwlock_strategy_report_result(rwlock,
                    error_code, error_code));
        return (bookkeeping_error);
    }
    if (has_reader_ticket == FT_TRUE)
        rwlock->waiting_readers -= 1U;
    rwlock->active_readers += 1U;
    error_code = pt_rwlock_strategy_unlock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t pt_rwlock_strategy_rdlock(t_pt_rwlock *rwlock)
{
    int error_code;

    error_code = pt_rwlock_strategy_read_lock_internal(rwlock, FT_FALSE);
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

int32_t pt_rwlock_strategy_try_rdlock(t_pt_rwlock *rwlock)
{
    int error_code;

    error_code = pt_rwlock_strategy_read_lock_internal(rwlock, FT_TRUE);
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

static int pt_rwlock_strategy_write_lock_internal(t_pt_rwlock *rwlock,
    ft_bool try_only)
{
    pt_thread_id_type thread_id;
    uint64_t writer_ticket;
    int error_code;
    int system_error;
    int wait_error;

    if (rwlock == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    thread_id = pt_thread_self();
    if (try_only == FT_TRUE)
        error_code = pt_rwlock_strategy_try_lock_mutex(rwlock);
    else
        error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_INVALID_STATE);
    }
    if (rwlock->writer_active == FT_TRUE
        && pt_thread_equal(rwlock->active_writer_thread, thread_id) != 0)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_MUTEX_ALREADY_LOCKED);
    }
    if (pt_rwlock_strategy_thread_in_readers(rwlock, thread_id, ft_nullptr)
        == FT_TRUE)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_MUTEX_ALREADY_LOCKED);
    }
    if (try_only == FT_TRUE
        && (rwlock->writer_active == FT_TRUE || rwlock->active_readers != 0
            || rwlock->reader_phase_open == FT_TRUE
            || rwlock->next_writer_ticket != rwlock->serving_writer_ticket))
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_THREAD_BUSY);
    }
    if (rwlock->next_writer_ticket == UINT64_MAX
        && rwlock->serving_writer_ticket == UINT64_MAX
        && rwlock->waiting_writers == 0 && rwlock->writer_active == FT_FALSE
        && rwlock->active_readers == 0)
    {
        rwlock->next_writer_ticket = 0U;
        rwlock->serving_writer_ticket = 0U;
    }
    if (rwlock->next_writer_ticket == UINT64_MAX)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (try_only == FT_TRUE)
    {
        rwlock->writer_active = FT_TRUE;
        rwlock->active_writer_thread = thread_id;
        rwlock->active_writer_has_ticket = FT_FALSE;
        error_code = pt_rwlock_strategy_unlock_mutex(rwlock);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_SUCCESS);
    }
    error_code = pt_buffer_reserve(rwlock->cancelled_writer_tickets,
            rwlock->cancelled_writer_tickets.size + 1U);
    if (error_code != FT_ERR_SUCCESS)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (error_code);
    }
    writer_ticket = rwlock->next_writer_ticket;
    rwlock->next_writer_ticket += 1U;
    rwlock->waiting_writers += 1U;
    while (rwlock->active_readers != 0 || rwlock->writer_active == FT_TRUE
        || rwlock->reader_phase_open == FT_TRUE
        || writer_ticket != rwlock->serving_writer_ticket)
    {
#ifdef LIBFT_TEST_BUILD
        if (pt_rwlock_test_take_failure(PT_RWLOCK_TEST_FAIL_CONDITION_WAIT)
            == FT_TRUE)
            system_error = EAGAIN;
        else
#endif
            system_error = pthread_cond_wait(&rwlock->writer_condition,
                    &rwlock->mutex);
        if (system_error != 0)
        {
            rwlock->waiting_writers -= 1U;
            wait_error = cmp_map_system_error_to_ft(system_error);
            error_code = pt_rwlock_strategy_cancel_ticket(rwlock,
                    writer_ticket);
            if (error_code == FT_ERR_SUCCESS)
            {
                error_code = pt_rwlock_strategy_broadcast(
                        &rwlock->writer_condition);
            }
            if (error_code == FT_ERR_SUCCESS)
                error_code = wait_error;
            pt_rwlock_strategy_unlock_mutex(rwlock);
            return (error_code);
        }
    }
    rwlock->waiting_writers -= 1U;
    rwlock->writer_active = FT_TRUE;
    rwlock->active_writer_thread = thread_id;
    rwlock->active_writer_has_ticket = FT_TRUE;
    error_code = pt_rwlock_strategy_unlock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t pt_rwlock_strategy_wrlock(t_pt_rwlock *rwlock)
{
    int error_code;

    error_code = pt_rwlock_strategy_write_lock_internal(rwlock, FT_FALSE);
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

int32_t pt_rwlock_strategy_try_wrlock(t_pt_rwlock *rwlock)
{
    int error_code;

    error_code = pt_rwlock_strategy_write_lock_internal(rwlock, FT_TRUE);
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

int32_t pt_rwlock_strategy_rdunlock(t_pt_rwlock *rwlock)
{
    pt_thread_id_type thread_id;
    ft_size_t reader_index;
    int error_code;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
        return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_INVALID_STATE,
                FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (pt_rwlock_strategy_report_result(rwlock, error_code,
                error_code));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_STATE, FT_ERR_INVALID_STATE));
    }
    thread_id = pt_thread_self();
    if (pt_rwlock_strategy_thread_in_readers(rwlock, thread_id,
            &reader_index) == FT_FALSE)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_MUTEX_NOT_OWNER, FT_ERR_MUTEX_NOT_OWNER));
    }
    if (rwlock->active_readers == 0)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_STATE, FT_ERR_INVALID_STATE));
    }
    rwlock->active_readers -= 1U;
    pt_buffer_erase(rwlock->active_reader_threads, reader_index);
    if (rwlock->active_readers == 0)
    {
        rwlock->reader_phase_open = FT_FALSE;
        if (rwlock->waiting_writers != 0)
            error_code = pt_rwlock_strategy_broadcast(
                    &rwlock->writer_condition);
        else if (rwlock->waiting_readers != 0
            && rwlock->strategy == PT_RWLOCK_STRATEGY_WRITER_PRIORITY)
        {
            rwlock->reader_phase_open = FT_TRUE;
            rwlock->reader_phase_cutoff = rwlock->next_reader_ticket;
            error_code = pt_rwlock_strategy_broadcast(
                    &rwlock->reader_condition);
        }
        else
            error_code = FT_ERR_SUCCESS;
        if (error_code != FT_ERR_SUCCESS)
        {
            pt_rwlock_strategy_unlock_mutex(rwlock);
            return (pt_rwlock_strategy_report_result(rwlock, error_code,
                    error_code));
        }
    }
    error_code = pt_rwlock_strategy_unlock_mutex(rwlock);
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

int32_t pt_rwlock_strategy_wrunlock(t_pt_rwlock *rwlock)
{
    pt_thread_id_type thread_id;
    int error_code;
    int system_error;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
        return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_INVALID_STATE,
                FT_ERR_INVALID_STATE));
    error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (pt_rwlock_strategy_report_result(rwlock, error_code,
                error_code));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_STATE, FT_ERR_INVALID_STATE));
    }
    thread_id = pt_thread_self();
    if (rwlock->writer_active == FT_FALSE
        || pt_thread_equal(rwlock->active_writer_thread, thread_id) == 0)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_MUTEX_NOT_OWNER, FT_ERR_MUTEX_NOT_OWNER));
    }
    rwlock->writer_active = FT_FALSE;
    rwlock->active_writer_thread = 0;
    error_code = FT_ERR_SUCCESS;
    if (rwlock->active_writer_has_ticket == FT_TRUE)
    {
        if (rwlock->serving_writer_ticket == UINT64_MAX)
            error_code = FT_ERR_OUT_OF_RANGE;
        else
            rwlock->serving_writer_ticket += 1U;
        if (error_code == FT_ERR_SUCCESS)
            error_code = pt_rwlock_strategy_finish_writer_queue(rwlock);
    }
    rwlock->active_writer_has_ticket = FT_FALSE;
    if (error_code == FT_ERR_SUCCESS)
    {
        if (rwlock->strategy == PT_RWLOCK_STRATEGY_WRITER_PRIORITY
            && rwlock->waiting_readers != 0)
        {
            rwlock->reader_phase_open = FT_TRUE;
            rwlock->reader_phase_cutoff = rwlock->next_reader_ticket;
            error_code = pt_rwlock_strategy_broadcast(
                    &rwlock->reader_condition);
        }
        else if (rwlock->waiting_writers != 0)
            error_code = pt_rwlock_strategy_broadcast(
                    &rwlock->writer_condition);
        else
            error_code = pt_rwlock_strategy_broadcast(
                    &rwlock->reader_condition);
    }
    system_error = pt_rwlock_strategy_unlock_mutex(rwlock);
    if (error_code == FT_ERR_SUCCESS && system_error != FT_ERR_SUCCESS)
        error_code = system_error;
    return (pt_rwlock_strategy_report_result(rwlock, error_code, error_code));
}

int32_t pt_rwlock_strategy_unlock(t_pt_rwlock *rwlock)
{
    pt_thread_id_type thread_id;
    int error_code;
    ft_bool writer_owned;
    ft_bool reader_owned;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
        return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_INVALID_STATE,
                FT_ERR_INVALID_STATE));
    thread_id = pt_thread_self();
    error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (pt_rwlock_strategy_report_result(rwlock, error_code,
                error_code));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_STATE, FT_ERR_INVALID_STATE));
    }
    writer_owned = FT_FALSE;
    reader_owned = FT_FALSE;
    if (rwlock->writer_active == FT_TRUE
        && pt_thread_equal(rwlock->active_writer_thread, thread_id) != 0)
        writer_owned = FT_TRUE;
    else if (pt_rwlock_strategy_thread_in_readers(rwlock, thread_id,
            ft_nullptr) == FT_TRUE)
        reader_owned = FT_TRUE;
    pt_rwlock_strategy_unlock_mutex(rwlock);
    if (writer_owned == FT_TRUE)
        return (pt_rwlock_strategy_wrunlock(rwlock));
    if (reader_owned == FT_TRUE)
        return (pt_rwlock_strategy_rdunlock(rwlock));
    return (pt_rwlock_strategy_report_result(rwlock,
            FT_ERR_MUTEX_NOT_OWNER, FT_ERR_MUTEX_NOT_OWNER));
}

int32_t pt_rwlock_strategy_destroy(t_pt_rwlock *rwlock)
{
    int system_error;
    int error_code;

    if (rwlock == ft_nullptr)
        return (pt_rwlock_strategy_report_result(rwlock,
                FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_ARGUMENT));
    if (rwlock->initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        rwlock->initialised_state = FT_CLASS_STATE_DESTROYED;
        pt_buffer_destroy(rwlock->active_reader_threads);
        pt_buffer_destroy(rwlock->cancelled_writer_tickets);
        return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_SUCCESS,
                FT_ERR_SUCCESS));
    }
    error_code = pt_rwlock_strategy_lock_mutex(rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (pt_rwlock_strategy_report_result(rwlock, error_code,
                error_code));
    if (rwlock->active_readers != 0 || rwlock->writer_active == FT_TRUE
        || rwlock->waiting_readers != 0 || rwlock->waiting_writers != 0)
    {
        pt_rwlock_strategy_unlock_mutex(rwlock);
        return (pt_rwlock_strategy_report_result(rwlock, FT_ERR_THREAD_BUSY,
                FT_ERR_THREAD_BUSY));
    }
    rwlock->initialised_state = FT_CLASS_STATE_DESTROYED;
    system_error = pt_rwlock_strategy_unlock_mutex(rwlock);
    if (system_error != FT_ERR_SUCCESS)
        return (pt_rwlock_strategy_report_result(rwlock, system_error,
                system_error));
    error_code = FT_ERR_SUCCESS;
    system_error = pthread_cond_destroy(&rwlock->reader_condition);
    if (system_error != 0)
        error_code = cmp_map_system_error_to_ft(system_error);
    system_error = pthread_cond_destroy(&rwlock->writer_condition);
    if (system_error != 0 && error_code == FT_ERR_SUCCESS)
        error_code = cmp_map_system_error_to_ft(system_error);
    system_error = pthread_mutex_destroy(&rwlock->mutex);
    if (system_error != 0 && error_code == FT_ERR_SUCCESS)
        error_code = cmp_map_system_error_to_ft(system_error);
    pt_buffer_destroy(rwlock->active_reader_threads);
    pt_buffer_destroy(rwlock->cancelled_writer_tickets);
    return (pt_rwlock_strategy_report_result(rwlock, error_code,
            error_code));
}

int32_t pt_rwlock_strategy_get_error(const t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (rwlock->error_code.load(std::memory_order_acquire));
}

const char *pt_rwlock_strategy_get_error_str(const t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (ft_strerror(FT_ERR_INVALID_ARGUMENT));
    return (ft_strerror(rwlock->error_code.load(std::memory_order_acquire)));
}

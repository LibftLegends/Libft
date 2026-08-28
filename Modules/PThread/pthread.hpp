#ifndef PTHREAD_HPP
# define PTHREAD_HPP

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <pthread.h>
#endif
#ifndef _WIN32
    #include <pthread.h>
#endif
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "pt_buffer.hpp"
#include <atomic>
#include <cstddef>
#ifdef _WIN32
    using pt_thread_id_type = DWORD;
    #define THREAD_ID GetCurrentThreadId()
#else
    using pt_thread_id_type = pthread_t;
    #define THREAD_ID pthread_self()
#endif

extern thread_local pt_thread_id_type pt_thread_id;
pt_thread_id_type pt_thread_self();

int pt_thread_join(pthread_t thread, void **retval);
int pt_thread_timed_join(pthread_t thread, void **retval, long timeout_ms);
int pt_thread_create(pthread_t *thread, const pthread_attr_t *attr,
                void *(*start_routine)(void *), void *arg);
int pt_thread_detach(pthread_t thread);
int pt_thread_cancel(pthread_t thread);
int pt_thread_sleep(unsigned int milliseconds);
int pt_thread_yield();
int pt_thread_equal(pthread_t thread1, pthread_t thread2);
int pt_thread_wait_uint32(std::atomic<uint32_t> *address, uint32_t expected_value);
int pt_thread_wait_uint32_timed(std::atomic<uint32_t> *address,
    uint32_t expected_value, uint64_t timeout_ms);
int pt_thread_wake_one_uint32(std::atomic<uint32_t> *address);
int pt_thread_wake_all_uint32(std::atomic<uint32_t> *address);
int32_t cmp_map_system_error_to_ft(int32_t error_code);

int pt_atomic_load(const std::atomic<int>& atomic_variable);
void pt_atomic_store(std::atomic<int>& atomic_variable, int desired_value);
int pt_atomic_fetch_add(std::atomic<int>& atomic_variable, int increment_value);
bool pt_atomic_compare_exchange(std::atomic<int>& atomic_variable, int& expected_value,
        int desired_value);

int pt_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attributes);
int pt_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pt_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pt_rwlock_unlock(pthread_rwlock_t *rwlock);
int pt_rwlock_destroy(pthread_rwlock_t *rwlock);

typedef enum e_pt_rwlock_strategy
{
    PT_RWLOCK_STRATEGY_READER_PRIORITY,
    PT_RWLOCK_STRATEGY_WRITER_PRIORITY
}   t_pt_rwlock_strategy;

#ifdef LIBFT_TEST_BUILD
# define PT_RWLOCK_TEST_FAIL_MUTEX_INIT 1U
# define PT_RWLOCK_TEST_FAIL_READER_CONDITION_INIT 2U
# define PT_RWLOCK_TEST_FAIL_WRITER_CONDITION_INIT 3U
# define PT_RWLOCK_TEST_FAIL_READER_BOOKKEEPING 4U
# define PT_RWLOCK_TEST_FAIL_CONDITION_WAIT 5U
# define PT_RWLOCK_TEST_FAIL_CONDITION_WAKE 6U
extern std::atomic<uint32_t> pt_rwlock_test_failure_stage;
#endif

typedef struct s_pt_rwlock
{
    pthread_mutex_t          mutex;
    pthread_cond_t           reader_condition;
    pthread_cond_t           writer_condition;
    size_t                   active_readers;
    size_t                   waiting_readers;
    size_t                   waiting_writers;
    uint64_t                 next_writer_ticket;
    uint64_t                 serving_writer_ticket;
    uint64_t                 next_reader_ticket;
    uint64_t                 reader_phase_cutoff;
    pt_thread_id_type        active_writer_thread;
    ft_bool                  writer_active;
    ft_bool                  active_writer_has_ticket;
    ft_bool                  reader_phase_open;
    pt_buffer<pt_thread_id_type> active_reader_threads;
    pt_buffer<uint64_t>      cancelled_writer_tickets;
    t_pt_rwlock_strategy     strategy;
    std::atomic<uint8_t>     initialised_state;
    std::atomic<int32_t>     error_code;

    s_pt_rwlock() noexcept
        : active_readers(0), waiting_readers(0), waiting_writers(0),
          next_writer_ticket(0U), serving_writer_ticket(0U),
          next_reader_ticket(0U), reader_phase_cutoff(0U),
          active_writer_thread(), writer_active(FT_FALSE),
          active_writer_has_ticket(FT_FALSE), reader_phase_open(FT_FALSE),
          active_reader_threads(), cancelled_writer_tickets(),
          strategy(PT_RWLOCK_STRATEGY_READER_PRIORITY),
          initialised_state(FT_CLASS_STATE_UNINITIALISED),
          error_code(FT_ERR_SUCCESS)
    {
        pt_buffer_init(this->active_reader_threads);
        pt_buffer_init(this->cancelled_writer_tickets);
    }
}   t_pt_rwlock;

int32_t pt_rwlock_strategy_init(t_pt_rwlock *rwlock, t_pt_rwlock_strategy strategy);
int32_t pt_rwlock_strategy_rdlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_try_rdlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_rdunlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_wrlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_try_wrlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_wrunlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_unlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_destroy(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_get_error(const t_pt_rwlock *rwlock);
const char *pt_rwlock_strategy_get_error_str(const t_pt_rwlock *rwlock);

#define SLEEP_TIME 100
#define MAX_SLEEP 10000
#define MAX_QUEUE 128

typedef struct s_thread_id
{
    pt_thread_id_type native_id;
}   t_thread_id;

struct s_duration_milliseconds;
struct s_monotonic_time_point;
using t_duration_milliseconds = struct s_duration_milliseconds;
using t_monotonic_time_point = struct s_monotonic_time_point;

t_thread_id    ft_this_thread_get_id();
void    ft_this_thread_sleep_for(t_duration_milliseconds duration);
void    ft_this_thread_sleep_until(t_monotonic_time_point time_point);
void    ft_this_thread_yield();

#ifndef PTHREAD_NO_PROMISE
template <typename ValueType>
class ft_promise;

template <>
class ft_promise<void>;

template <typename ValueType, typename Function>
int pt_async(ft_promise<ValueType>& promise, Function function)
{
    struct AsyncData
    {
        ft_promise<ValueType>* promise;
        Function function;
    };

    auto start_routine = [](void* arg) -> void*
    {
        AsyncData* data = static_cast<AsyncData*>(arg);
        data->promise->set_value(data->function());
        delete data;
        return (ft_nullptr);
    };

    AsyncData* data = new AsyncData{&promise, ft_move(function)};
    pthread_t thread;
    int ret = pt_thread_create(&thread, ft_nullptr, start_routine, data);
    if (ret != 0)
    {
        delete data;
        return (ret);
    }
    pt_thread_detach(thread);
    return (ret);
}

#endif

#endif

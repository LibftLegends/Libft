#include "task_scheduler.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <new>
#include <utility>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/pair.hpp"
#include "../Template/queue.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

static unsigned long long ft_threading_trace_thread_id(t_thread_id thread_identifier)
{
#ifdef __APPLE__
    return (static_cast<unsigned long long>(
            reinterpret_cast<uintptr_t>(thread_identifier.native_id)));
#else
    return (static_cast<unsigned long long>(thread_identifier.native_id));
#endif
}

ft_task_scheduler::scheduled_task::scheduled_task()
{
    this->_initialised_state = ft_task_scheduler::scheduled_task::_state_uninitialised;
    (void)this->initialize();
    return ;
}

ft_task_scheduler::scheduled_task::~scheduled_task()
{
    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_initialised)
        (void)this->destroy();
    return ;
}

ft_task_scheduler::scheduled_task::scheduled_task(const scheduled_task &other)
{
    this->_initialised_state = ft_task_scheduler::scheduled_task::_state_uninitialised;
    (void)this->initialize(other);
    return ;
}

ft_task_scheduler::scheduled_task::scheduled_task(scheduled_task &&other)
{
    this->_initialised_state = ft_task_scheduler::scheduled_task::_state_uninitialised;
    (void)this->initialize_move(other);
    return ;
}

ft_task_scheduler::scheduled_task &
ft_task_scheduler::scheduled_task::operator=(const scheduled_task &other)
{
    if (other._initialised_state != ft_task_scheduler::scheduled_task::_state_initialised)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "scheduled_task::operator=(const scheduled_task &) source",
            "called with source object that is not initialised");
        return (*this);
    }
    if (this == &other)
        return (*this);
    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_initialised)
        (void)this->destroy();
    (void)this->initialize(other);
    return (*this);
}

ft_task_scheduler::scheduled_task &
ft_task_scheduler::scheduled_task::operator=(scheduled_task &&other)
{
    if (other._initialised_state != ft_task_scheduler::scheduled_task::_state_initialised)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "scheduled_task::operator=(scheduled_task &&) source",
            "called with source object that is not initialised");
        return (*this);
    }
    if (this == &other)
        return (*this);
    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_initialised)
        (void)this->destroy();
    (void)this->initialize_move(other);
    return (*this);
}

int ft_task_scheduler::scheduled_task::initialize()
{
    int state_initialize_error;

    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_initialised)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "scheduled_task::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_time = t_monotonic_time_point();
    this->_interval_ms = 0;
    this->_function = ft_function<void()>();
    (void)this->_state.destroy();
    state_initialize_error = this->_state.initialize();
    if (state_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = ft_task_scheduler::scheduled_task::_state_destroyed;
        return (state_initialize_error);
    }
    this->_trace_id = 0;
    this->_parent_id = 0;
    this->_label = ft_nullptr;
    this->_initialised_state = ft_task_scheduler::scheduled_task::_state_initialised;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::scheduled_task::initialize(const scheduled_task &other)
{
    if (other._initialised_state != ft_task_scheduler::scheduled_task::_state_initialised)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "scheduled_task::initialize(const scheduled_task &) source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    this->_time = other._time;
    this->_interval_ms = other._interval_ms;
    this->_function = other._function;
    this->_state = other._state;
    this->_trace_id = other._trace_id;
    this->_parent_id = other._parent_id;
    this->_label = other._label;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::scheduled_task::initialize_move(scheduled_task &other)
{
    if (other._initialised_state != ft_task_scheduler::scheduled_task::_state_initialised)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "scheduled_task::initialize_move(scheduled_task &) source",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    const int32_t state_move_error = this->_state.move(other._state);
    if (state_move_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (state_move_error);
    }
    this->_time = other._time;
    this->_interval_ms = other._interval_ms;
    this->_function = ft_move(other._function);
    this->_trace_id = other._trace_id;
    this->_parent_id = other._parent_id;
    this->_label = other._label;
    other._time = t_monotonic_time_point();
    other._interval_ms = 0;
    other._function = ft_function<void()>();
    other._trace_id = 0;
    other._parent_id = 0;
    other._label = ft_nullptr;
    other._initialised_state = ft_task_scheduler::scheduled_task::_state_destroyed;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::scheduled_task::destroy()
{
    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_uninitialised
        || this->_initialised_state == ft_task_scheduler::scheduled_task::_state_destroyed)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state != ft_task_scheduler::scheduled_task::_state_initialised)
        return (FT_ERR_INVALID_STATE);
    this->_time = t_monotonic_time_point();
    this->_interval_ms = 0;
    this->_function = ft_function<void()>();
    this->_state.reset();
    this->_trace_id = 0;
    this->_parent_id = 0;
    this->_label = ft_nullptr;
    this->_initialised_state = ft_task_scheduler::scheduled_task::_state_destroyed;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::scheduled_task::is_initialised() const
{
    if (this->_initialised_state == ft_task_scheduler::scheduled_task::_state_initialised)
        return (1);
    return (0);
}

ft_scheduled_task_state::ft_scheduled_task_state()
    : _cancelled(false), _state_mutex(ft_nullptr)
{
    return ;
}

ft_scheduled_task_state::~ft_scheduled_task_state()
{
    this->teardown_thread_safety();
    return ;
}

void ft_scheduled_task_state::cancel()
{
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_cancelled.store(true);
    this->unlock_internal(lock_acquired);
    return ;
}

bool ft_scheduled_task_state::is_cancelled() const
{
    bool cancelled_value;
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (false);
    cancelled_value = this->_cancelled.load();
    this->unlock_internal(lock_acquired);
    return (cancelled_value);
}

int ft_scheduled_task_state::enable_thread_safety()
{
    pt_mutex *mutex_pointer;
    int mutex_error;

    if (this->_state_mutex != ft_nullptr)
        return (0);
    mutex_pointer = new (std::nothrow) pt_mutex();
    if (mutex_pointer == ft_nullptr)
        return (-1);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (-1);
    }
    this->_state_mutex = mutex_pointer;
    return (0);
}

int ft_scheduled_task_state::disable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return (FT_ERR_SUCCESS);
}

bool ft_scheduled_task_state::is_thread_safe() const
{
    return (this->_state_mutex != ft_nullptr);
}

int ft_scheduled_task_state::lock(bool *lock_acquired) const
{
    int lock_result = this->lock_internal(lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

void ft_scheduled_task_state::unlock(bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int ft_scheduled_task_state::lock_internal(bool *lock_acquired) const
{
    int mutex_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    mutex_error = pt_mutex_lock_if_not_null(this->_state_mutex);

    if (mutex_error != FT_ERR_SUCCESS)
    {
        if (mutex_error == FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            bool state_lock_acquired;

            state_lock_acquired = false;
            if (this->_state_mutex->lock_state(&state_lock_acquired) == 0)
                this->_state_mutex->unlock_state(state_lock_acquired);
            return (FT_ERR_SUCCESS);
        }
        return (mutex_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int ft_scheduled_task_state::unlock_internal(bool lock_acquired) const
{
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_mutex_unlock_if_not_null(this->_state_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_scheduled_task_state::teardown_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        (void)destroy_error;
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return ;
}

ft_scheduled_task_handle::ft_scheduled_task_handle()
    : _state(), _scheduler(ft_nullptr), _state_mutex(ft_nullptr)
{
    return ;
}

ft_scheduled_task_handle::ft_scheduled_task_handle(ft_task_scheduler *scheduler,
        const ft_sharedptr<ft_scheduled_task_state> &state)
    : _state(), _scheduler(scheduler), _state_mutex(ft_nullptr)
{
    int state_initialize_error;

    if (!scheduler || !state)
    {
        this->_scheduler = ft_nullptr;
        return ;
    }
    state_initialize_error = this->_state.initialize(state);
    if (state_initialize_error != FT_ERR_SUCCESS)
    {
        this->_scheduler = ft_nullptr;
        return ;
    }
    {
        int state_error = this->_state.get_error();

        if (state_error != FT_ERR_SUCCESS)
        {
            this->_scheduler = ft_nullptr;
            return ;
        }
    }
    return ;
}

ft_scheduled_task_handle::ft_scheduled_task_handle(const ft_scheduled_task_handle &other)
    : _state(), _scheduler(ft_nullptr), _state_mutex(ft_nullptr)
{
    bool lock_acquired;
    int state_initialize_error;

    lock_acquired = false;
    int lock_error = other.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (!other._state)
    {
        other.unlock_internal(lock_acquired);
        return ;
    }
    state_initialize_error = this->_state.initialize(other._state);
    if (state_initialize_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(lock_acquired);
        return ;
    }
    this->_scheduler = other._scheduler;
    {
        int state_error = this->_state.get_error();

        if (state_error != FT_ERR_SUCCESS)
        {
            this->_scheduler = ft_nullptr;
            other.unlock_internal(lock_acquired);
            return ;
        }
    }
    if (other._state_mutex != ft_nullptr)
    {
        if (this->enable_thread_safety() != 0)
        {
            other.unlock_internal(lock_acquired);
            return ;
        }
    }
    other.unlock_internal(lock_acquired);
    return ;
}

ft_scheduled_task_handle &ft_scheduled_task_handle::operator=(const ft_scheduled_task_handle &other)
{
    if (this == &other)
        return (*this);
    bool this_lock_acquired;
    bool other_lock_acquired;
    int state_initialize_error;

    this_lock_acquired = false;
    int this_lock_error = this->lock_internal(&this_lock_acquired);
    if (this_lock_error != FT_ERR_SUCCESS)
        return (*this);
    other_lock_acquired = false;
    int other_lock_error = other.lock_internal(&other_lock_acquired);
    if (other_lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(this_lock_acquired);
        return (*this);
    }
    if (!other._state)
    {
        (void)this->_state.destroy();
        this->_scheduler = ft_nullptr;
        other.unlock_internal(other_lock_acquired);
        this->unlock_internal(this_lock_acquired);
        return (*this);
    }
    (void)this->_state.destroy();
    state_initialize_error = this->_state.initialize(other._state);
    if (state_initialize_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_lock_acquired);
        this->unlock_internal(this_lock_acquired);
        return (*this);
    }
    this->_scheduler = other._scheduler;
    {
        int state_error = this->_state.get_error();

        if (state_error != FT_ERR_SUCCESS)
        {
            this->_scheduler = ft_nullptr;
            other.unlock_internal(other_lock_acquired);
            this->unlock_internal(this_lock_acquired);
            return (*this);
        }
    }
    this->teardown_thread_safety();
    if (other._state_mutex != ft_nullptr)
    {
        if (this->enable_thread_safety() != 0)
        {
            other.unlock_internal(other_lock_acquired);
            this->unlock_internal(this_lock_acquired);
            return (*this);
        }
    }
    other.unlock_internal(other_lock_acquired);
    this->unlock_internal(this_lock_acquired);
    return (*this);
}

ft_scheduled_task_handle::~ft_scheduled_task_handle()
{
    bool lock_acquired;

    lock_acquired = false;
    int lock_result = this->lock_internal(&lock_acquired);
    if (lock_result == FT_ERR_SUCCESS)
    {
        (void)this->_state.destroy();
        this->_scheduler = ft_nullptr;
        this->unlock_internal(lock_acquired);
    }
    else
    {
        (void)this->_state.destroy();
        this->_scheduler = ft_nullptr;
    }
    this->teardown_thread_safety();
    return ;
}

bool ft_scheduled_task_handle::cancel()
{
    bool cancel_result;
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (false);
    if (!this->_scheduler || !this->_state)
    {
        this->unlock_internal(lock_acquired);
        return (false);
    }
    cancel_result = this->_scheduler->cancel_task_state(this->_state);
    if (!cancel_result)
    {
        this->unlock_internal(lock_acquired);
        return (false);
    }
    this->unlock_internal(lock_acquired);
    return (true);
}

bool ft_scheduled_task_handle::valid() const
{
    bool state_valid;
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (false);
    state_valid = static_cast<bool>(this->_state);
    if (!this->_scheduler || !state_valid)
    {
        this->unlock_internal(lock_acquired);
        return (false);
    }
    this->unlock_internal(lock_acquired);
    return (true);
}

const ft_scheduled_task_state *ft_scheduled_task_handle::get_state() const
{
    const ft_scheduled_task_state *state_pointer;
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    state_pointer = this->_state.get();
    this->unlock_internal(lock_acquired);
    return (state_pointer);
}

int ft_scheduled_task_handle::enable_thread_safety()
{
    pt_mutex *mutex_pointer;
    int mutex_error;

    if (this->_state_mutex != ft_nullptr)
        return (0);
    mutex_pointer = new (std::nothrow) pt_mutex();
    if (mutex_pointer == ft_nullptr)
        return (-1);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (-1);
    }
    this->_state_mutex = mutex_pointer;
    return (0);
}

int ft_scheduled_task_handle::disable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return (FT_ERR_SUCCESS);
}

bool ft_scheduled_task_handle::is_thread_safe() const
{
    return (this->_state_mutex != ft_nullptr);
}

int ft_scheduled_task_handle::lock(bool *lock_acquired) const
{
    int lock_result = this->lock_internal(lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

void ft_scheduled_task_handle::unlock(bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int ft_scheduled_task_handle::lock_internal(bool *lock_acquired) const
{
    int mutex_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    mutex_error = pt_mutex_lock_if_not_null(this->_state_mutex);

    if (mutex_error != FT_ERR_SUCCESS)
    {
        if (mutex_error == FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            bool state_lock_acquired;

            state_lock_acquired = false;
            if (this->_state_mutex->lock_state(&state_lock_acquired) == 0)
                this->_state_mutex->unlock_state(state_lock_acquired);
            return (FT_ERR_SUCCESS);
        }
        return (mutex_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int ft_scheduled_task_handle::unlock_internal(bool lock_acquired) const
{
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    (void)pt_mutex_unlock_if_not_null(this->_state_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_scheduled_task_handle::teardown_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        (void)destroy_error;
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return ;
}


void ft_task_scheduler::abort_if_not_initialised(const char *method_name) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, method_name);
    return ;
}

ft_task_scheduler::ft_task_scheduler(size_t thread_count)
    : _queue(), _workers(), _timer_thread(), _scheduled(), _scheduled_mutex(),
      _scheduled_condition(), _running(false), _queue_metrics_mutex(),
      _worker_metrics_mutex(), _queue_size_counter(0),
      _scheduled_size_counter(0), _worker_active_counter(0),
      _worker_idle_counter(0), _worker_total_count(0), _configured_thread_count(thread_count),
      _state_mutex(ft_nullptr), _initialised_state(ft_task_scheduler::_state_uninitialised)
{
    return ;
}

ft_task_scheduler::~ft_task_scheduler()
{
    if (this->_initialised_state == ft_task_scheduler::_state_initialised)
        (void)this->destroy();
    return ;
}

int ft_task_scheduler::initialize(size_t thread_count)
{
    size_t index;
    unsigned int cpu_count;
    bool worker_failure;
    int queue_initialize_error;
    int workers_initialize_error;
    int scheduled_initialize_error;

    if (this->_initialised_state == ft_task_scheduler::_state_initialised)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "ft_task_scheduler::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (thread_count == 0)
        thread_count = this->_configured_thread_count;
    if (thread_count == 0)
    {
        cpu_count = su_get_cpu_count();
        if (cpu_count == 0)
            thread_count = 1;
        else
            thread_count = cpu_count;
    }
    this->_configured_thread_count = thread_count;
    this->_queue_size_counter = 0;
    this->_scheduled_size_counter = 0;
    this->_worker_active_counter = 0;
    this->_worker_idle_counter = 0;
    this->_worker_total_count = 0;
    this->_running.store(true);
    if (this->_scheduled_mutex.initialize() != FT_ERR_SUCCESS)
    {
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        this->_running.store(false);
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_queue_metrics_mutex.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_scheduled_mutex.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        this->_running.store(false);
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_worker_metrics_mutex.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_queue_metrics_mutex.destroy();
        (void)this->_scheduled_mutex.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        this->_running.store(false);
        return (FT_ERR_INVALID_STATE);
    }
    queue_initialize_error = this->_queue.initialize();
    if (queue_initialize_error != FT_ERR_SUCCESS)
    {
        this->_running.store(false);
        (void)this->_worker_metrics_mutex.destroy();
        (void)this->_queue_metrics_mutex.destroy();
        (void)this->_scheduled_mutex.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        return (queue_initialize_error);
    }
    workers_initialize_error = this->_workers.initialize();
    if (workers_initialize_error != FT_ERR_SUCCESS)
    {
        this->_running.store(false);
        (void)this->_queue.destroy();
        (void)this->_worker_metrics_mutex.destroy();
        (void)this->_queue_metrics_mutex.destroy();
        (void)this->_scheduled_mutex.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        return (workers_initialize_error);
    }
    scheduled_initialize_error = this->_scheduled.initialize();
    if (scheduled_initialize_error != FT_ERR_SUCCESS)
    {
        this->_running.store(false);
        (void)this->_workers.destroy();
        (void)this->_queue.destroy();
        (void)this->_worker_metrics_mutex.destroy();
        (void)this->_queue_metrics_mutex.destroy();
        (void)this->_scheduled_mutex.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        return (scheduled_initialize_error);
    }
    worker_failure = false;
    this->_workers.reserve(thread_count);
    {
        int worker_reserve_error;

        worker_reserve_error = this->_workers.get_error();
        if (worker_reserve_error != FT_ERR_SUCCESS)
        {
            this->_running.store(false);
            (void)this->_scheduled.destroy();
            (void)this->_workers.destroy();
            (void)this->_queue.destroy();
            (void)this->_worker_metrics_mutex.destroy();
            (void)this->_queue_metrics_mutex.destroy();
            (void)this->_scheduled_mutex.destroy();
            this->_initialised_state = ft_task_scheduler::_state_destroyed;
            return (worker_reserve_error);
        }
    }
    index = 0;
    while (index < thread_count)
    {
        ft_thread worker([this]()
        {
            this->worker_loop();
            return ;
        });
        {
            int worker_error;

            if (worker.joinable())
                worker_error = FT_ERR_SUCCESS;
            else
                worker_error = FT_ERR_SYSTEM;
            if (worker_error != FT_ERR_SUCCESS)
            {
                worker_failure = true;
                break ;
            }
        }
        this->_workers.push_back(ft_move(worker));
        {
            int worker_push_error;

            worker_push_error = this->_workers.get_error();
            if (worker_push_error != FT_ERR_SUCCESS)
            {
                worker_failure = true;
                break ;
            }
        }
        if (!this->update_worker_total(1))
        {
            worker_failure = true;
            break ;
        }
        if (!this->update_worker_counters(0, 1))
        {
            worker_failure = true;
            break ;
        }
        index++;
    }
    if (worker_failure)
    {
        size_t join_index;
        size_t worker_count;

        this->_running.store(false);
        worker_count = this->_workers.size();
        join_index = 0;
        while (join_index < worker_count)
        {
            ft_thread &worker_reference = this->_workers[join_index];

            if (worker_reference.joinable())
                worker_reference.join();
            join_index++;
        }
        (void)this->_worker_metrics_mutex.destroy();
        (void)this->_queue_metrics_mutex.destroy();
        (void)this->_scheduled_mutex.destroy();
        (void)this->_scheduled.destroy();
        (void)this->_workers.destroy();
        (void)this->_queue.destroy();
        this->_initialised_state = ft_task_scheduler::_state_destroyed;
        return (FT_ERR_INVALID_STATE);
    }
    {
        ft_thread timer_worker([this]()
        {
            this->timer_loop();
            return ;
        });
        int timer_error;

        if (timer_worker.joinable())
            timer_error = FT_ERR_SUCCESS;
        else
            timer_error = FT_ERR_SYSTEM;
        if (timer_error != FT_ERR_SUCCESS)
        {
            size_t join_index;
            size_t worker_count;

            this->_running.store(false);
            worker_count = this->_workers.size();
            join_index = 0;
            while (join_index < worker_count)
            {
                ft_thread &worker_reference = this->_workers[join_index];

                if (worker_reference.joinable())
                    worker_reference.join();
                join_index++;
            }
            (void)this->_worker_metrics_mutex.destroy();
            (void)this->_queue_metrics_mutex.destroy();
            (void)this->_scheduled_mutex.destroy();
            (void)this->_scheduled.destroy();
            (void)this->_workers.destroy();
            (void)this->_queue.destroy();
            this->_initialised_state = ft_task_scheduler::_state_destroyed;
            return (timer_error);
        }
        this->_timer_thread = ft_move(timer_worker);
    }
    this->_initialised_state = ft_task_scheduler::_state_initialised;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::destroy()
{
    if (this->_initialised_state == ft_task_scheduler::_state_uninitialised
        || this->_initialised_state == ft_task_scheduler::_state_destroyed)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state != ft_task_scheduler::_state_initialised)
        return (FT_ERR_INVALID_STATE);
    this->_running.store(false);
    this->_queue.shutdown();
    int scheduled_mutex_lock_error = this->_scheduled_mutex.lock();
    if (scheduled_mutex_lock_error == FT_ERR_SUCCESS)
    {
        this->_scheduled_condition.broadcast();
        (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
    }
    (void)scheduled_mutex_lock_error;
    size_t index;
    size_t worker_count;

    worker_count = this->_workers.size();
    index = 0;
    while (index < worker_count)
    {
        ft_thread &worker_reference = this->_workers[index];

        if (worker_reference.joinable())
            worker_reference.join();
        index++;
    }
    if (this->_timer_thread.joinable())
        this->_timer_thread.join();
    this->teardown_thread_safety();
    (void)this->_queue.destroy();
    (void)this->_worker_metrics_mutex.destroy();
    (void)this->_queue_metrics_mutex.destroy();
    (void)this->_scheduled_mutex.destroy();
    (void)this->_scheduled.destroy();
    (void)this->_workers.destroy();
    this->_initialised_state = ft_task_scheduler::_state_destroyed;
    return (FT_ERR_SUCCESS);
}

void ft_task_scheduler::worker_loop()
{
    while (true)
    {
        task_queue_entry queue_entry;
        bool has_task;
        unsigned long long previous_span;

        if (!this->_running.load())
            break ;
        has_task = this->_queue.wait_pop(queue_entry, this->_running);
        if (!has_task)
        {
            if (!this->_running.load())
                break ;
            continue;
        }
        if (!this->update_worker_counters(1, -1))
            return ;
        if (!this->update_queue_size(-1))
            return ;
        this->trace_emit_event(FT_TASK_TRACE_PHASE_DEQUEUED, queue_entry._trace_id,
                queue_entry._parent_id, queue_entry._label, false);
        this->trace_emit_event(FT_TASK_TRACE_PHASE_STARTED, queue_entry._trace_id,
                queue_entry._parent_id, queue_entry._label, false);
        previous_span = task_scheduler_trace_push_span(queue_entry._trace_id);
        if (queue_entry._function)
            queue_entry._function();
        task_scheduler_trace_pop_span(previous_span);
        if (!this->update_worker_counters(-1, 1))
            return ;
        this->trace_emit_event(FT_TASK_TRACE_PHASE_FINISHED, queue_entry._trace_id,
                queue_entry._parent_id, queue_entry._label, false);
    }
    (void)this->update_worker_counters(0, -1);
    return ;
}

void ft_task_scheduler::scheduled_heap_sift_up(size_t index)
{
    if (index == 0U)
        return ;
    scheduled_task *entries = this->_scheduled.begin();
    const int64_t deadline = entries[index]._time.milliseconds;
    ft_size_t parent = (index - 1U) / 2U;
    if (entries[parent]._time.milliseconds <= deadline)
        return ;
    scheduled_task task(ft_move(entries[index]));
    while (index != 0U && entries[parent]._time.milliseconds > deadline)
    {
        entries[index] = ft_move(entries[parent]);
        index = parent;
        if (index != 0U)
            parent = (index - 1U) / 2U;
    }
    entries[index] = ft_move(task);
    return ;
}

void ft_task_scheduler::scheduled_heap_sift_down(size_t index)
{
    const ft_size_t size = this->_scheduled.size();
    if (index >= size / 2U)
        return ;
    scheduled_task *entries = this->_scheduled.begin();
    const int64_t deadline = entries[index]._time.milliseconds;
    scheduled_task task;
    const ft_size_t entry_index = index;
    while (index < size / 2U)
    {
        ft_size_t child = index * 2U + 1U;
        if (child + 1U < size
            && entries[child + 1U]._time.milliseconds < entries[child]._time.milliseconds)
            ++child;
        if (entries[child]._time.milliseconds >= deadline)
            break ;
        if (index == entry_index)
            task = ft_move(entries[index]);
        entries[index] = ft_move(entries[child]);
        index = child;
    }
    if (index != entry_index)
        entries[index] = ft_move(task);
    return ;
}

bool ft_task_scheduler::update_queue_size(long long delta)
{
    int queue_metrics_lock_error = this->_queue_metrics_mutex.lock();
    if (queue_metrics_lock_error != FT_ERR_SUCCESS)
        return (false);
    this->_queue_size_counter += delta;
    (void)pt_mutex_unlock_if_not_null(&this->_queue_metrics_mutex);
    return (true);
}

bool ft_task_scheduler::update_worker_counters(long long active_delta, long long idle_delta)
{
    int worker_metrics_lock_error = this->_worker_metrics_mutex.lock();
    if (worker_metrics_lock_error != FT_ERR_SUCCESS)
        return (false);
    this->_worker_active_counter += active_delta;
    this->_worker_idle_counter += idle_delta;
    (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    return (true);
}

bool ft_task_scheduler::update_worker_total(long long delta)
{
    int worker_metrics_lock_error = this->_worker_metrics_mutex.lock();
    if (worker_metrics_lock_error != FT_ERR_SUCCESS)
        return (false);
    if (delta >= 0)
        this->_worker_total_count += static_cast<size_t>(delta);
    else
    {
        size_t decrement;

        decrement = static_cast<size_t>(-delta);
        if (decrement > this->_worker_total_count)
            this->_worker_total_count = 0;
        else
            this->_worker_total_count -= decrement;
    }
    (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    return (true);
}

bool ft_task_scheduler::scheduled_heap_push(scheduled_task &&task)
{
    size_t size;

    this->_scheduled.push_back(ft_move(task));
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
        return (false);
    size = this->_scheduled.size();
    if (size == 0)
    {
        this->_scheduled_size_counter = 0;
        return (true);
    }
    this->scheduled_heap_sift_up(size - 1);
    this->_scheduled_size_counter = static_cast<long long>(size);
    return (true);
}

bool ft_task_scheduler::scheduled_heap_pop(scheduled_task &task)
{
    size_t size;

    size = this->_scheduled.size();
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
        return (false);
    if (size == 0)
        return (false);
    task = ft_move(this->_scheduled[0]);
    if (size == 1)
    {
        this->_scheduled.pop_back();
        if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
            return (false);
        this->_scheduled_size_counter = 0;
        return (true);
    }
    scheduled_task last_task;

    last_task = ft_move(this->_scheduled[size - 1]);
    this->_scheduled.pop_back();
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
        return (false);
    this->_scheduled[0] = ft_move(last_task);
    this->scheduled_heap_sift_down(0);
    this->_scheduled_size_counter = static_cast<long long>(size - 1U);
    return (true);
}

bool ft_task_scheduler::scheduled_remove_index(size_t index)
{
    size_t size;

    size = this->_scheduled.size();
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
        return (false);
    if (size == 0)
        return (false);
    if (index >= size)
        return (false);
    if (index == size - 1)
    {
        this->_scheduled.pop_back();
        if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
            return (false);
        this->_scheduled_size_counter = static_cast<long long>(size - 1U);
        return (true);
    }
    scheduled_task last_task;

    last_task = ft_move(this->_scheduled[size - 1]);
    this->_scheduled.pop_back();
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
        return (false);
    this->_scheduled[index] = ft_move(last_task);
    const scheduled_task *entries = this->_scheduled.begin();
    if (index != 0U
        && entries[index]._time.milliseconds < entries[(index - 1U) / 2U]._time.milliseconds)
        this->scheduled_heap_sift_up(index);
    else
        this->scheduled_heap_sift_down(index);
    this->_scheduled_size_counter = static_cast<long long>(size - 1U);
    return (true);
}

bool ft_task_scheduler::cancel_task_state(const ft_sharedptr<ft_scheduled_task_state> &state)
{
    ft_sharedptr<ft_scheduled_task_state> state_copy;
    ft_scheduled_task_state *state_pointer;
    unsigned long long cancelled_trace_id;
    unsigned long long cancelled_parent_id;
    const char *cancelled_label;
    bool scheduler_lock_acquired;
    int scheduled_mutex_error;

    scheduler_lock_acquired = false;
    int scheduler_lock_error = this->lock_internal(&scheduler_lock_acquired);
    if (scheduler_lock_error != FT_ERR_SUCCESS)
        return (false);
    if (!state)
    {
        this->unlock_internal(scheduler_lock_acquired);
        return (false);
    }
    state_copy = state;
    {
        int state_error = state_copy.get_error();

        if (state_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(scheduler_lock_acquired);
            return (false);
        }
    }
    state_pointer = state_copy.get();
    if (!state_pointer)
    {
        this->unlock_internal(scheduler_lock_acquired);
        return (false);
    }
    state_pointer->cancel();
    scheduled_mutex_error = this->_scheduled_mutex.lock();
    if (scheduled_mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(scheduler_lock_acquired);
        return (false);
    }
    const ft_size_t scheduled_count = this->_scheduled.size();
    const scheduled_task *scheduled_entries = this->_scheduled.begin();
    if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
        this->unlock_internal(scheduler_lock_acquired);
        return (false);
    }
    ft_size_t index = 0U;
    cancelled_trace_id = 0;
    cancelled_parent_id = 0;
    cancelled_label = ft_nullptr;
    while (index < scheduled_count)
    {
        const scheduled_task &entry = scheduled_entries[index];
        if (entry._state.get() == state_pointer)
        {
            cancelled_trace_id = entry._trace_id;
            cancelled_parent_id = entry._parent_id;
            cancelled_label = entry._label;
            if (!this->scheduled_remove_index(index))
            {
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                this->unlock_internal(scheduler_lock_acquired);
                return (false);
            }
            break ;
        }
        ++index;
    }
    (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
    if (this->_scheduled_condition.broadcast() != 0)
    {
        this->unlock_internal(scheduler_lock_acquired);
        return (false);
    }
    if (index < scheduled_count)
    {
        this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED, cancelled_trace_id,
                cancelled_parent_id, cancelled_label, false);
    }
    this->unlock_internal(scheduler_lock_acquired);
    return (true);
}

void ft_task_scheduler::timer_loop()
{
    while (true)
    {
        if (!this->_running.load())
            break ;
        if (this->_scheduled_mutex.lock() != FT_ERR_SUCCESS)
            return ;
        while (true)
        {
            size_t scheduled_count;
            bool has_entries;

            if (!this->_running.load())
            {
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                return ;
            }
            scheduled_count = this->_scheduled.size();
            if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
            {
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                return ;
            }
            has_entries = scheduled_count > 0;
            if (!has_entries)
            {
                if (this->_scheduled_condition.wait(this->_scheduled_mutex) != 0)
                {
                    (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                    return ;
                }
                continue;
            }
            t_monotonic_time_point earliest_time;
            t_monotonic_time_point current_time;
            long long wait_milliseconds;

            earliest_time = this->_scheduled[0]._time;
            current_time = time_monotonic_point_now();
            wait_milliseconds = time_monotonic_point_diff_ms(current_time, earliest_time);
            if (wait_milliseconds > 0)
            {
                struct timespec wake_time;
                long long seconds;
                long long nanoseconds;
                int wait_result;

                if (clock_gettime(CLOCK_REALTIME, &wake_time) != 0)
                {
                    (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                    return ;
                }
                seconds = wait_milliseconds / 1000;
                nanoseconds = (wait_milliseconds % 1000) * 1000000;
                wake_time.tv_sec += seconds;
                wake_time.tv_nsec += static_cast<decltype(wake_time.tv_nsec)>(nanoseconds);
                if (wake_time.tv_nsec >= 1000000000)
                {
                    long long carry;

                    carry = wake_time.tv_nsec / 1000000000;
                    wake_time.tv_sec += carry;
                    wake_time.tv_nsec = wake_time.tv_nsec % 1000000000;
                }
                wait_result = this->_scheduled_condition.wait_until(this->_scheduled_mutex, wake_time);
                if (wait_result == 0)
                    continue;
                if (wait_result != ETIMEDOUT)
                {
                    (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                    return ;
                }
            }
            break ;
        }
        while (true)
        {
            size_t scheduled_count;
            int time_comparison;
            t_monotonic_time_point now;
            scheduled_task expired_task;
            bool pop_success;

            scheduled_count = this->_scheduled.size();
            if (this->_scheduled.get_error() != FT_ERR_SUCCESS)
            {
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                return ;
            }
            if (scheduled_count == 0)
                break ;
            now = time_monotonic_point_now();
            time_comparison = time_monotonic_point_compare(now, this->_scheduled[0]._time);
            if (time_comparison < 0)
                break ;
            pop_success = this->scheduled_heap_pop(expired_task);
            if (!pop_success)
            {
                int scheduled_error = this->_scheduled.get_error();
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                (void)scheduled_error;
                return ;
            }
            bool task_cancelled;

            task_cancelled = false;
            if (expired_task._state)
            {
                task_cancelled = expired_task._state->is_cancelled();
            }
            if (task_cancelled)
                continue;
            if (!expired_task._function)
            {
                ft_function<void()> original_function;

                original_function = ft_move(expired_task._function);
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED,
                        expired_task._trace_id, expired_task._parent_id,
                        expired_task._label, true);
                if (original_function)
                    original_function();
                if (!this->_running.load())
                    return ;
                if (this->_scheduled_mutex.lock() != FT_ERR_SUCCESS)
                    return ;
                if (expired_task._interval_ms > 0)
                {
                    bool should_reschedule;

                    should_reschedule = true;
                    if (expired_task._state)
                    {
                        bool cancelled_now;

                        cancelled_now = expired_task._state->is_cancelled();
                        if (cancelled_now)
                            should_reschedule = false;
                    }
                    if (should_reschedule)
                    {
                        t_monotonic_time_point updated_time;
                        unsigned long long previous_trace_id;
                        unsigned long long new_trace_id;
                        const char *reschedule_label;

                        updated_time = time_monotonic_point_add_ms(time_monotonic_point_now(),
                                expired_task._interval_ms);
                        expired_task._time = updated_time;
                        expired_task._function = ft_move(original_function);
                        previous_trace_id = expired_task._trace_id;
                        new_trace_id = task_scheduler_trace_generate_span_id();
                        reschedule_label = expired_task._label;
                        expired_task._trace_id = new_trace_id;
                        expired_task._parent_id = previous_trace_id;
                        expired_task._label = reschedule_label;
                        if (!this->scheduled_heap_push(ft_move(expired_task)))
                        {
                            (void)this->_scheduled.get_error();
                        }
                        else
                            this->trace_emit_event(FT_TASK_TRACE_PHASE_TIMER_REGISTERED,
                                    new_trace_id, previous_trace_id, reschedule_label, true);
                    }
                }
                continue;
            }
            task_queue_entry queue_entry;

            queue_entry._function = expired_task._function;
            if (!queue_entry._function)
            {
                ft_function<void()> original_function;

                original_function = ft_move(expired_task._function);
                (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
                if (original_function)
                    original_function();
                if (!this->_running.load())
                    return ;
                if (this->_scheduled_mutex.lock() != FT_ERR_SUCCESS)
                    return ;
                if (expired_task._interval_ms > 0)
                {
                    bool should_reschedule;

                    should_reschedule = true;
                    if (expired_task._state)
                    {
                        bool cancelled_now;

                        cancelled_now = expired_task._state->is_cancelled();
                        if (cancelled_now)
                            should_reschedule = false;
                    }
                    if (should_reschedule)
                    {
                        t_monotonic_time_point updated_time;
                        updated_time = time_monotonic_point_add_ms(time_monotonic_point_now(),
                                expired_task._interval_ms);
                        expired_task._time = updated_time;
                        expired_task._function = ft_move(original_function);
                        if (!this->scheduled_heap_push(ft_move(expired_task)))
                            (void)this->_scheduled.get_error();
                    }
                }
                continue;
            }
            (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
            queue_entry._trace_id = expired_task._trace_id;
            queue_entry._parent_id = expired_task._parent_id;
            queue_entry._label = expired_task._label;
            this->trace_emit_event(FT_TASK_TRACE_PHASE_TIMER_TRIGGERED,
                    queue_entry._trace_id, queue_entry._parent_id,
                    queue_entry._label, true);
            this->trace_emit_event(FT_TASK_TRACE_PHASE_ENQUEUED,
                    expired_task._trace_id, expired_task._parent_id,
                    expired_task._label, true);
            int queue_push_error = this->_queue.push(ft_move(queue_entry));
            if (queue_push_error != FT_ERR_SUCCESS)
            {
                this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED,
                        expired_task._trace_id, expired_task._parent_id,
                        expired_task._label, true);
                if (expired_task._function)
                    expired_task._function();
            }
            else
            {
                bool metrics_updated;

                metrics_updated = this->update_queue_size(1);
                if (!metrics_updated)
                    return ;
            }
            if (!this->_running.load())
                return ;
            if (this->_scheduled_mutex.lock() != FT_ERR_SUCCESS)
                return ;
            if (expired_task._interval_ms > 0)
            {
                bool should_reschedule;

                should_reschedule = true;
                if (expired_task._state)
                {
                    bool cancelled_now;

                    cancelled_now = expired_task._state->is_cancelled();
                    if (cancelled_now)
                        should_reschedule = false;
                }
                if (should_reschedule)
                {
                    t_monotonic_time_point updated_time;
                    unsigned long long previous_trace_id;
                    unsigned long long new_trace_id;
                    const char *reschedule_label;

                    updated_time = time_monotonic_point_add_ms(time_monotonic_point_now(),
                            expired_task._interval_ms);
                    expired_task._time = updated_time;
                    previous_trace_id = expired_task._trace_id;
                    new_trace_id = task_scheduler_trace_generate_span_id();
                    reschedule_label = expired_task._label;
                    expired_task._trace_id = new_trace_id;
                    expired_task._parent_id = previous_trace_id;
                    expired_task._label = reschedule_label;
                    if (!this->scheduled_heap_push(ft_move(expired_task)))
                        (void)this->_scheduled.get_error();
                    else
                        this->trace_emit_event(FT_TASK_TRACE_PHASE_TIMER_REGISTERED,
                                new_trace_id, previous_trace_id, reschedule_label, true);
                }
            }
            continue;
        }
        (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
    }
    return ;
}

bool ft_task_scheduler::capture_metrics(ft_task_trace_event &event) const
{
    pt_thread_id_type current_thread_id;
    bool queue_locked;
    int queue_lock_error;

    current_thread_id = THREAD_ID;
    queue_locked = false;
    queue_lock_error = this->_queue_metrics_mutex.lock();
    if (queue_lock_error == FT_ERR_SUCCESS)
        queue_locked = true;
    else if (queue_lock_error == FT_ERR_MUTEX_ALREADY_LOCKED)
    {
        if (!this->_queue_metrics_mutex.is_owned_by_thread(current_thread_id))
            return (false);
    }
    else
        return (false);
    event.queue_depth = this->_queue_size_counter;
    if (queue_locked)
    {
        (void)pt_mutex_unlock_if_not_null(&this->_queue_metrics_mutex);
    }
    bool scheduled_locked;
    int scheduled_lock_error;

    scheduled_locked = false;
    scheduled_lock_error = this->_scheduled_mutex.lock();
    if (scheduled_lock_error == FT_ERR_SUCCESS)
        scheduled_locked = true;
    else if (scheduled_lock_error == FT_ERR_MUTEX_ALREADY_LOCKED)
    {
        if (!this->_scheduled_mutex.is_owned_by_thread(current_thread_id))
            return (false);
    }
    else
        return (false);
    event.scheduled_depth = this->_scheduled_size_counter;
    if (scheduled_locked)
    {
        (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
    }
    bool worker_locked;
    int worker_lock_error;

    worker_locked = false;
    worker_lock_error = this->_worker_metrics_mutex.lock();
    if (worker_lock_error == FT_ERR_SUCCESS)
        worker_locked = true;
    else if (worker_lock_error == FT_ERR_MUTEX_ALREADY_LOCKED)
    {
        if (!this->_worker_metrics_mutex.is_owned_by_thread(current_thread_id))
            return (false);
    }
    else
        return (false);
    event.worker_active_count = this->_worker_active_counter;
    event.worker_idle_count = this->_worker_idle_counter;
    if (worker_locked)
    {
        (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    }
    return (true);
}

void ft_task_scheduler::trace_emit_event(e_ft_task_trace_phase phase, unsigned long long trace_id,
        unsigned long long parent_id, const char *label, bool timer_thread)
{
    if (task_scheduler_trace_has_sinks() == FT_FALSE)
        return ;
    ft_task_trace_event event;
    bool captured;
    t_thread_id thread_identifier;

    event.phase = phase;
    event.trace_id = trace_id;
    event.parent_id = parent_id;
    event.label = label;
    event.timestamp = time_monotonic_point_now();
    thread_identifier = ft_this_thread_get_id();
    event.thread_id = ft_threading_trace_thread_id(thread_identifier);
    event.queue_depth = -1;
    event.scheduled_depth = -1;
    event.worker_active_count = -1;
    event.worker_idle_count = -1;
    event.timer_thread = timer_thread;
    captured = this->capture_metrics(event);
    (void)captured;
    task_scheduler_trace_emit(event);
    return ;
}

long long ft_task_scheduler::get_queue_size() const
{
    long long queue_size;

    queue_size = 0;
    (void)this->get_queue_size(&queue_size);
    return (queue_size);
}

int ft_task_scheduler::get_queue_size(long long *queue_size) const
{
    if (queue_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->abort_if_not_initialised("ft_task_scheduler::get_queue_size");
    if (this->_queue_metrics_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYSTEM);
    *queue_size = this->_queue_size_counter;
    (void)pt_mutex_unlock_if_not_null(&this->_queue_metrics_mutex);
    return (FT_ERR_SUCCESS);
}

long long ft_task_scheduler::get_scheduled_task_count() const
{
    long long scheduled_count;

    scheduled_count = 0;
    (void)this->get_scheduled_task_count(&scheduled_count);
    return (scheduled_count);
}

int ft_task_scheduler::get_scheduled_task_count(long long *scheduled_count) const
{
    if (scheduled_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->abort_if_not_initialised("ft_task_scheduler::get_scheduled_task_count");
    if (this->_scheduled_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYSTEM);
    *scheduled_count = this->_scheduled_size_counter;
    (void)pt_mutex_unlock_if_not_null(&this->_scheduled_mutex);
    return (FT_ERR_SUCCESS);
}

long long ft_task_scheduler::get_worker_active_count() const
{
    long long active_count;

    active_count = 0;
    (void)this->get_worker_active_count(&active_count);
    return (active_count);
}

int ft_task_scheduler::get_worker_active_count(long long *active_count) const
{
    if (active_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->abort_if_not_initialised("ft_task_scheduler::get_worker_active_count");
    if (this->_worker_metrics_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYSTEM);
    *active_count = this->_worker_active_counter;
    (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    return (FT_ERR_SUCCESS);
}

long long ft_task_scheduler::get_worker_idle_count() const
{
    long long idle_count;

    idle_count = 0;
    (void)this->get_worker_idle_count(&idle_count);
    return (idle_count);
}

int ft_task_scheduler::get_worker_idle_count(long long *idle_count) const
{
    if (idle_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->abort_if_not_initialised("ft_task_scheduler::get_worker_idle_count");
    if (this->_worker_metrics_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYSTEM);
    *idle_count = this->_worker_idle_counter;
    (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    return (FT_ERR_SUCCESS);
}

size_t ft_task_scheduler::get_worker_total_count() const
{
    size_t total_count;

    total_count = 0;
    (void)this->get_worker_total_count(&total_count);
    return (total_count);
}

int ft_task_scheduler::get_worker_total_count(size_t *total_count) const
{
    if (total_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->abort_if_not_initialised("ft_task_scheduler::get_worker_total_count");
    if (this->_worker_metrics_mutex.lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYSTEM);
    *total_count = this->_worker_total_count;
    (void)pt_mutex_unlock_if_not_null(&this->_worker_metrics_mutex);
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::enable_thread_safety()
{
    pt_recursive_mutex *state_mutex;
    int mutex_error;

    this->abort_if_not_initialised("ft_task_scheduler::enable_thread_safety");
    if (this->_state_mutex != ft_nullptr)
        return (0);
    
    state_mutex = new (std::nothrow) pt_recursive_mutex();
    if (state_mutex == ft_nullptr)
        return (-1);
    mutex_error = state_mutex->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete state_mutex;
        return (-1);
    }
    this->_state_mutex = state_mutex;
    return (0);
}

int ft_task_scheduler::disable_thread_safety()
{
    pt_recursive_mutex *state_mutex;
    int destroy_error;

    this->abort_if_not_initialised("ft_task_scheduler::disable_thread_safety");
    state_mutex = this->_state_mutex;
    if (state_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_state_mutex = ft_nullptr;
    destroy_error = state_mutex->destroy();
    delete state_mutex;
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

bool ft_task_scheduler::is_thread_safe() const
{
    bool enabled;

    this->abort_if_not_initialised("ft_task_scheduler::is_thread_safe");
    enabled = (this->_state_mutex != ft_nullptr);
    return (enabled);
}

int ft_task_scheduler::lock(bool *lock_acquired) const
{
    int lock_result;

    this->abort_if_not_initialised("ft_task_scheduler::lock");
    lock_result = this->lock_internal(lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

int ft_task_scheduler::unlock(bool lock_acquired) const
{
    int unlock_result;

    this->abort_if_not_initialised("ft_task_scheduler::unlock");
    unlock_result = this->unlock_internal(lock_acquired);
    if (unlock_result != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

int ft_task_scheduler::lock_internal(bool *lock_acquired) const
{
    int lock_result;

    this->abort_if_not_initialised("ft_task_scheduler::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_result = pt_recursive_mutex_lock_if_not_null(this->_state_mutex);
    if (lock_result != FT_ERR_SUCCESS)
    {
        return (lock_result);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int ft_task_scheduler::unlock_internal(bool lock_acquired) const
{
    this->abort_if_not_initialised("ft_task_scheduler::unlock_internal");
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_state_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_task_scheduler::teardown_thread_safety()
{
    int destroy_error;

    if (this->_state_mutex != ft_nullptr)
    {
        destroy_error = this->_state_mutex->destroy();
        (void)destroy_error;
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return ;
}

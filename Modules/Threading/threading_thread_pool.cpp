#include "thread_pool.hpp"
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/queue.hpp"
#include "../Template/vector.hpp"

int32_t ft_thread_pool::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

int32_t ft_thread_pool::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_thread_safe_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    lock_result = pt_recursive_mutex_lock_if_not_null(this->_thread_safe_mutex);
    if (lock_result != FT_ERR_SUCCESS)
        return (set_error(lock_result));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void ft_thread_pool::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return ;
    if (this->_thread_safe_mutex == ft_nullptr)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_thread_safe_mutex);
    return ;
}

void ft_thread_pool::worker_entry(ft_thread_pool *pool)
{
    if (pool == ft_nullptr)
        return ;
    pool->worker();
    return ;
}

void ft_thread_pool::worker()
{
    int32_t work_lock_error;

    while (FT_TRUE)
    {
        ft_function<void()> task;

        work_lock_error = pt_recursive_mutex_lock_if_not_null(this->_work_mutex);
        if (work_lock_error != FT_ERR_SUCCESS)
            return ;
        if (this->_stop && this->_tasks.empty())
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
            return ;
        }
        if (this->_tasks.empty())
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
            (void)pt_thread_sleep(1);
            continue ;
        }
        task = this->_tasks.dequeue();
        this->_active += 1;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
        task();
        work_lock_error = pt_recursive_mutex_lock_if_not_null(this->_work_mutex);
        if (work_lock_error != FT_ERR_SUCCESS)
            return ;
        this->_active -= 1;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
    }
    return ;
}

ft_thread_pool::ft_thread_pool(ft_size_t thread_count, ft_size_t max_tasks)
    : _workers(), _tasks(), _configured_thread_count(thread_count),
      _max_tasks(max_tasks), _stop(FT_FALSE), _active(0), _work_mutex(ft_nullptr),
      _thread_safe_mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_thread_pool::~ft_thread_pool()
{
    int32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_thread_safe_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

int32_t ft_thread_pool::initialize()
{
    ft_size_t worker_index;
    int32_t mutex_result;
    int32_t queue_result;
    int32_t workers_result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_thread_pool::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_stop = FT_FALSE;
    this->_active = 0;
    this->_work_mutex = new (std::nothrow) pt_recursive_mutex();
    if (this->_work_mutex == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    mutex_result = this->_work_mutex->initialize();
    if (mutex_result != FT_ERR_SUCCESS)
    {
        delete this->_work_mutex;
        this->_work_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(mutex_result));
    }
    workers_result = this->_workers.initialize();
    if (workers_result != FT_ERR_SUCCESS)
    {
        (void)this->_work_mutex->destroy();
        delete this->_work_mutex;
        this->_work_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(workers_result));
    }
    queue_result = this->_tasks.initialize();
    if (queue_result != FT_ERR_SUCCESS)
    {
        (void)this->_workers.destroy();
        (void)this->_work_mutex->destroy();
        delete this->_work_mutex;
        this->_work_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(queue_result));
    }
    worker_index = 0;
    while (worker_index < this->_configured_thread_count)
    {
        ft_thread worker(&ft_thread_pool::worker_entry, this);

        this->_workers.push_back(ft_move(worker));
        if (this->_workers.get_error() != FT_ERR_SUCCESS)
        {
            (void)this->_tasks.destroy();
            (void)this->_workers.destroy();
            (void)this->_work_mutex->destroy();
            delete this->_work_mutex;
            this->_work_mutex = ft_nullptr;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(this->_workers.get_error()));
        }
        ++worker_index;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_thread_pool::initialize(ft_size_t thread_count, ft_size_t max_tasks)
{
    this->_configured_thread_count = thread_count;
    this->_max_tasks = max_tasks;
    return (this->initialize());
}

int32_t ft_thread_pool::destroy()
{
    ft_size_t worker_index;
    ft_size_t worker_count;
    int32_t first_error;
    int32_t work_lock_error;
    int32_t work_destroy_error;
    pt_recursive_mutex *work_mutex_pointer;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    work_lock_error = pt_recursive_mutex_lock_if_not_null(this->_work_mutex);
    if (work_lock_error != FT_ERR_SUCCESS)
        first_error = work_lock_error;
    else
    {
        this->_stop = FT_TRUE;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
    }
    worker_index = 0;
    worker_count = this->_workers.size();
    while (worker_index < worker_count)
    {
        if (this->_workers[worker_index].joinable())
            this->_workers[worker_index].join();
        ++worker_index;
    }
    this->_workers.clear();
    (void)this->_workers.destroy();
    (void)this->_tasks.destroy();
    work_mutex_pointer = this->_work_mutex;
    this->_work_mutex = ft_nullptr;
    if (work_mutex_pointer != ft_nullptr)
    {
        work_destroy_error = work_mutex_pointer->destroy();
        delete work_mutex_pointer;
        if (first_error == FT_ERR_SUCCESS && work_destroy_error != FT_ERR_SUCCESS)
            first_error = work_destroy_error;
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

int32_t ft_thread_pool::move(ft_thread_pool &other) noexcept
{
    int32_t destroy_other_result;
    int32_t destroy_result;
    int32_t enable_result;
    int32_t initialize_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_thread_pool::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    this->_configured_thread_count = other._configured_thread_count;
    this->_max_tasks = other._max_tasks;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_stop = FT_FALSE;
        this->_active = 0;
        this->_work_mutex = ft_nullptr;
        this->_thread_safe_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    initialize_result = this->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
        return (set_error(initialize_result));
    if (other._thread_safe_mutex != ft_nullptr)
    {
        enable_result = this->enable_thread_safety();
        if (enable_result != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (set_error(enable_result));
        }
    }
    destroy_other_result = other.destroy();
    if (destroy_other_result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (set_error(destroy_other_result));
    }
    return (set_error(FT_ERR_SUCCESS));
}

void ft_thread_pool::wait()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t work_lock_error;
    ft_bool done;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_thread_pool::wait");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    done = FT_FALSE;
    while (done == FT_FALSE)
    {
        work_lock_error = pt_recursive_mutex_lock_if_not_null(this->_work_mutex);
        if (work_lock_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            set_error(work_lock_error);
            return ;
        }
        if (this->_tasks.empty() == FT_TRUE && this->_active == 0)
            done = FT_TRUE;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
        if (done == FT_FALSE)
            (void)pt_thread_sleep(1);
    }
    this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t ft_thread_pool::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_thread_pool::enable_thread_safety");
    if (this->_thread_safe_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (set_error(initialize_result));
    }
    this->_thread_safe_mutex = new_mutex;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_thread_pool::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (set_error(FT_ERR_INVALID_STATE));
    mutex_pointer = this->_thread_safe_mutex;
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    this->_thread_safe_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (set_error(destroy_result));
    return (set_error(FT_ERR_SUCCESS));
}

ft_bool ft_thread_pool::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_thread_pool::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_thread_safe_mutex != ft_nullptr);
}

int32_t ft_thread_pool::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_thread_pool::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

void ft_thread_pool::unlock(ft_bool lock_acquired) const
{
    this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t ft_thread_pool::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_thread_pool::get_error");
    return (_last_error);
}

const char *ft_thread_pool::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_thread_pool::get_error_str");
    return (ft_strerror(_last_error));
}

thread_local int32_t ft_thread_pool::_last_error = FT_ERR_SUCCESS;

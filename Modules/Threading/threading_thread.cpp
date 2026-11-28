#include "thread.hpp"
#include <cerrno>
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Template/move.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_thread::start_payload::start_payload()
    : function()
{
    return ;
}

ft_thread::start_payload::~start_payload()
{
    return ;
}

void *ft_thread::start_routine(void *data)
{
    std::shared_ptr<start_payload> *payload_capsule;
    payload_capsule = static_cast<std::shared_ptr<start_payload> *>(data);
    if (payload_capsule == ft_nullptr)
        return (ft_nullptr);
    std::shared_ptr<start_payload> payload(ft_move(*payload_capsule));
    delete payload_capsule;
    if (!payload)
        return (ft_nullptr);
    payload->function();
    return (ft_nullptr);
}

ft_thread::ft_thread()
    : _thread(), _joinable(false), _start_payload(),
      _state_mutex(ft_nullptr), _thread_safe_enabled(false)
{
    return ;
}

ft_thread::~ft_thread()
{
    bool lock_acquired;
    int lock_error;

    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error == FT_ERR_SUCCESS)
    {
        if (this->_joinable)
            this->detach_locked();
        else
            this->_start_payload.reset();
        this->unlock_internal(lock_acquired);
    }
    this->teardown_thread_safety();
    return ;
}

ft_thread::ft_thread(ft_thread &&other)
    : _thread(), _joinable(false), _start_payload(),
      _state_mutex(ft_nullptr), _thread_safe_enabled(false)
{
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = other.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_thread = other._thread;
    this->_joinable = other._joinable;
    this->_start_payload = ft_move(other._start_payload);
    if (other._thread_safe_enabled && other._state_mutex != ft_nullptr)
    {
        this->_state_mutex = other._state_mutex;
        this->_thread_safe_enabled = true;
        other._state_mutex = ft_nullptr;
        other._thread_safe_enabled = false;
    }
    other._joinable = false;
    other.unlock_internal(lock_acquired);
    return ;
}

ft_thread &ft_thread::operator=(ft_thread &&other)
{
    bool this_lock_acquired;
    bool other_lock_acquired;

    if (this != &other)
    {
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
        if (this->_joinable)
            this->detach_locked();
        this->_thread = other._thread;
        this->_joinable = other._joinable;
        this->_start_payload = ft_move(other._start_payload);
        this->teardown_thread_safety();
        if (other._thread_safe_enabled && other._state_mutex != ft_nullptr)
        {
            this->_state_mutex = other._state_mutex;
            this->_thread_safe_enabled = true;
            other._state_mutex = ft_nullptr;
            other._thread_safe_enabled = false;
        }
        other._joinable = false;
        other._start_payload.reset();
        other.unlock_internal(other_lock_acquired);
        this->unlock_internal(this_lock_acquired);
    }
    return (*this);
}

bool ft_thread::joinable() const
{
    bool lock_acquired;
    bool joinable_state;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (false);
    joinable_state = this->_joinable;
    this->unlock_internal(lock_acquired);
    return (joinable_state);
}

void ft_thread::join()
{
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (!this->_joinable)
    {
        this->_start_payload.reset();
        this->unlock_internal(lock_acquired);
        return ;
    }
    int join_result = pt_thread_join(this->_thread, ft_nullptr);
    if (join_result != 0)
    {
        if (join_result == ESRCH || join_result == EINVAL)
        {
            this->_joinable = false;
            this->_start_payload.reset();
        }
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->_joinable = false;
    this->_start_payload.reset();
    this->unlock_internal(lock_acquired);
    return ;
}

void ft_thread::detach()
{
    bool lock_acquired;

    lock_acquired = false;
    int lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->detach_locked();
    this->unlock_internal(lock_acquired);
    return ;
}

int ft_thread::enable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
        return (0);
    this->_state_mutex = new (std::nothrow) pt_mutex();
    if (this->_state_mutex == ft_nullptr)
        return (-1);
    int init_error = this->_state_mutex->initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
        return (init_error);
    }
    this->_thread_safe_enabled = true;
    return (FT_ERR_SUCCESS);
}

int ft_thread::disable_thread_safety()
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
    this->_thread_safe_enabled = false;
    return (FT_ERR_SUCCESS);
}

bool ft_thread::is_thread_safe() const
{
    return (this->_state_mutex != ft_nullptr);
}

int ft_thread::lock(bool *lock_acquired) const
{
    return (this->lock_internal(lock_acquired));
}

void ft_thread::unlock(bool lock_acquired) const
{
    this->unlock_internal(lock_acquired);
}

int ft_thread::lock_internal(bool *lock_acquired) const
{
    int mutex_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    if (!this->_thread_safe_enabled)
        return (FT_ERR_SUCCESS);
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

void ft_thread::unlock_internal(bool lock_acquired) const
{
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
        return ;
    (void)pt_mutex_unlock_if_not_null(this->_state_mutex);
    return ;
}

void ft_thread::teardown_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        this->_state_mutex->destroy();
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    this->_thread_safe_enabled = false;
    return ;
}

int ft_thread::detach_locked()
{
    if (!this->_joinable)
    {
        this->_start_payload.reset();
        return (FT_ERR_SUCCESS);
    }
    int detach_result = pt_thread_detach(this->_thread);
    if (detach_result != 0)
    {
        if (detach_result == ESRCH || detach_result == EINVAL)
        {
            this->_joinable = false;
            this->_start_payload.reset();
        }
        return (cmp_map_system_error_to_ft(detach_result));
    }
    this->_joinable = false;
    this->_start_payload.reset();
    return (FT_ERR_SUCCESS);
}

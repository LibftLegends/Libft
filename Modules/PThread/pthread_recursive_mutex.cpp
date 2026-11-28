#include "pthread.hpp"
#include "recursive_mutex.hpp"
#include "mutex.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "pthread_lock_tracking.hpp"
#include <cstdlib>
#include <new>

pt_recursive_mutex::pt_recursive_mutex()
    : _owner(0), _lock(false), _lock_depth(0),
    _native_mutex(ft_nullptr), _initialised_state(pt_recursive_mutex::_state_uninitialised),
    _state_mutex(ft_nullptr), _valid_state(false)
{
    return ;
}

pt_recursive_mutex::~pt_recursive_mutex()
{
    if (this->_initialised_state == pt_recursive_mutex::_state_initialised)
        (void)this->destroy();
    this->teardown_thread_safety();
    return ;
}

int pt_recursive_mutex::ensure_native_mutex() const
{
    if (this->_initialised_state != pt_recursive_mutex::_state_initialised)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "pt_recursive_mutex::ensure_native_mutex");
    if (this->_native_mutex == ft_nullptr)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "pt_recursive_mutex::ensure_native_mutex",
            "native mutex pointer is null while object is initialised");
        return (FT_ERR_INVALID_STATE);
    }
    return (FT_ERR_SUCCESS);
}

int pt_recursive_mutex::initialize()
{
    if (this->_initialised_state == pt_recursive_mutex::_state_initialised)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "pt_recursive_mutex::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_native_mutex != ft_nullptr)
    {
        this->_native_mutex->~mutex();
        std::free(this->_native_mutex);
        this->_native_mutex = ft_nullptr;
    }
    void *native_storage = std::malloc(sizeof(std::mutex));
    if (native_storage == ft_nullptr)
    {
        this->_initialised_state = pt_recursive_mutex::_state_destroyed;
        this->_valid_state.store(false, std::memory_order_release);
        return (FT_ERR_NO_MEMORY);
    }
    std::mutex *native_mutex = new (native_storage) std::mutex();
    if (native_mutex == ft_nullptr)
    {
        std::free(native_storage);
        this->_initialised_state = pt_recursive_mutex::_state_destroyed;
        this->_valid_state.store(false, std::memory_order_release);
        return (FT_ERR_NO_MEMORY);
    }
    this->_native_mutex = native_mutex;
    this->_initialised_state = pt_recursive_mutex::_state_initialised;
    this->_valid_state.store(true, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int pt_recursive_mutex::destroy()
{
    if (this->_initialised_state == pt_recursive_mutex::_state_uninitialised
        || this->_initialised_state == pt_recursive_mutex::_state_destroyed)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state != pt_recursive_mutex::_state_initialised)
        return (FT_ERR_INVALID_STATE);
    if (this->_native_mutex == ft_nullptr)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "pt_recursive_mutex::destroy",
            "native mutex pointer is null while object is initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_lock.load(std::memory_order_acquire))
        return (FT_ERR_THREAD_BUSY);
    if (this->_lock_depth.load(std::memory_order_acquire) != 0)
        return (FT_ERR_THREAD_BUSY);
    this->_native_mutex->~mutex();
    std::free(this->_native_mutex);
    this->_native_mutex = ft_nullptr;
    this->_initialised_state = pt_recursive_mutex::_state_destroyed;
    this->_valid_state.store(false, std::memory_order_release);
    this->_lock.store(false, std::memory_order_release);
    this->_owner.store(0, std::memory_order_release);
    this->_lock_depth.store(0, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

bool pt_recursive_mutex::lockState() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::lockState");
    return (this->_lock.load(std::memory_order_acquire));
}

int pt_recursive_mutex::lock_internal(bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    if (this->_state_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int state_result = this->_state_mutex->lock_state(lock_acquired);
    if (state_result == 0)
    {
        if (lock_acquired != ft_nullptr)
            *lock_acquired = true;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_STATE);
}

int pt_recursive_mutex::unlock_internal(bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::unlock_internal");
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_state_mutex->unlock_state(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void pt_recursive_mutex::teardown_thread_safety()
{
    if (this->_state_mutex == ft_nullptr)
        return ;
    (void)this->_state_mutex->destroy();
    delete this->_state_mutex;
    this->_state_mutex = ft_nullptr;
    return ;
}

int pt_recursive_mutex::lock_state(bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::lock_state");
    int lock_error;

    lock_error = this->lock();
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (0);
}

void pt_recursive_mutex::unlock_state(bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::unlock_state");
    if (lock_acquired == false)
        return ;
    (void)this->unlock();
    return ;
}

bool pt_recursive_mutex::is_owned_by_thread(pt_thread_id_type thread_id) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "pt_recursive_mutex::is_owned_by_thread");
    pthread_t owner_thread;
    pt_mutex_vector owned_mutexes;
    ft_size_t index;
    bool lock_flag;
    int tracking_error;

    lock_flag = this->_lock.load(std::memory_order_acquire);
    if (!lock_flag)
    {
        return (false);
    }
    owner_thread = this->_owner.load(std::memory_order_relaxed);
    if (owner_thread != 0)
    {
        bool matches_owner;

        matches_owner = (pt_thread_equal(owner_thread, thread_id) != 0);
        return (matches_owner);
    }
    tracking_error = FT_ERR_SUCCESS;
    owned_mutexes = pt_lock_tracking::get_owned_mutexes(thread_id, &tracking_error);
    if (tracking_error != FT_ERR_SUCCESS)
        return (false);
    const void *mutex_handle = static_cast<const void *>(this);
    index = 0;
    while (index < owned_mutexes.size)
    {
        if (owned_mutexes.data[index] == mutex_handle)
        {
            pt_buffer_destroy(owned_mutexes);
            return (true);
        }
        index += 1;
    }
    pt_buffer_destroy(owned_mutexes);
    return (false);
}

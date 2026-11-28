#ifndef FT_PROMISE_HPP
#define FT_PROMISE_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include <atomic>
#include <chrono>
#include <new>
#include <cstdlib>
#include "move.hpp"

template <typename ValueType>
class ft_future;

namespace ft_promise_detail
{
    inline constexpr uint32_t PENDING = 0U;
    // Only publication clears WAITING, so one timeout cannot hide other waiters.
    inline constexpr uint32_t WAITING = 1U;
    inline constexpr uint32_t READY = 2U;

    inline void publish_ready(std::atomic<uint32_t> &state)
    {
        if (state.exchange(READY, std::memory_order_release) == WAITING)
            (void)pt_thread_wake_all_uint32(&state);
        return ;
    }

    inline int32_t wait_ready(std::atomic<uint32_t> &state,
        std::chrono::milliseconds timeout = std::chrono::seconds(30))
    {
        if (state.load(std::memory_order_acquire) == READY)
            return (FT_ERR_SUCCESS);
        uint32_t expected = PENDING;
        if (!state.compare_exchange_strong(expected, WAITING,
                std::memory_order_acq_rel) && expected == READY)
            return (FT_ERR_SUCCESS);

        using wait_clock = std::chrono::steady_clock;
        const auto deadline = wait_clock::now() + timeout;
        while (state.load(std::memory_order_acquire) != READY)
        {
            const auto remaining = deadline - wait_clock::now();
            if (remaining <= wait_clock::duration::zero())
                return (FT_ERR_TIMEOUT);
            const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(remaining);
            const int32_t wait_error = pt_thread_wait_uint32_timed(&state, WAITING,
                static_cast<uint64_t>(milliseconds.count()));
            if (wait_error != FT_ERR_SUCCESS && wait_error != FT_ERR_TIMEOUT)
                return (wait_error);
        }
        return (FT_ERR_SUCCESS);
    }
}

template <typename ValueType>
class ft_promise
{
    private:
        friend class ft_future<ValueType>;
        ValueType _value;
        mutable std::atomic<uint32_t> _readiness;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        int32_t wait_ready() const;

    protected:
    public:
        ft_promise();
        ft_promise(const ft_promise& other) = delete;
        ft_promise(ft_promise&& other) = delete;
        ~ft_promise();

        ft_promise& operator=(const ft_promise&) = delete;
        ft_promise& operator=(ft_promise&&) = delete;

        int32_t initialize();
        int32_t destroy();
        void set_value(const ValueType& value);
        void set_value(ValueType&& value);
        ValueType get() const;
        ft_bool is_ready() const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;
    };

template <>
class ft_promise<void>
{
    private:
        friend class ft_future<void>;
        mutable std::atomic<uint32_t> _readiness;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        int32_t wait_ready() const;

    public:
        ft_promise();
        ft_promise(const ft_promise& other) = delete;
        ft_promise(ft_promise&& other) = delete;
        ~ft_promise();

        ft_promise& operator=(const ft_promise&) = delete;
        ft_promise& operator=(ft_promise&&) = delete;

        int32_t initialize();
        int32_t destroy();
        void set_value();
        void get() const;
        ft_bool is_ready() const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;
    };

template <typename ValueType>
ft_promise<ValueType>::ft_promise()
    : _value(), _readiness(ft_promise_detail::PENDING), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    (void)this->initialize();
    return ;
}

template <typename ValueType>
ft_promise<ValueType>::~ft_promise()
{
    (void)this->destroy();
    return ;
}

inline ft_promise<void>::ft_promise()
    : _readiness(ft_promise_detail::PENDING), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    (void)this->initialize();
    return ;
}

inline ft_promise<void>::~ft_promise()
{
    (void)this->destroy();
    return ;
}

template <typename ValueType>
int32_t ft_promise<ValueType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_promise::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_value = ValueType();
    this->_readiness.store(ft_promise_detail::PENDING, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_promise<void>::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_readiness.store(ft_promise_detail::PENDING, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_promise<ValueType>::destroy()
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_value = ValueType();
    this->_readiness.store(ft_promise_detail::PENDING, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::destroy()
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_readiness.store(ft_promise_detail::PENDING, std::memory_order_release);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_promise<ValueType>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::lock");
    return (this->lock_internal(lock_acquired));
}

inline int32_t ft_promise<void>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::lock");
    return (this->lock_internal(lock_acquired));
}

template <typename ValueType>
void ft_promise<ValueType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

inline void ft_promise<void>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ValueType>
int32_t ft_promise<ValueType>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t mutex_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    mutex_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (mutex_result != FT_ERR_SUCCESS)
        return (mutex_result);
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t mutex_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    mutex_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (mutex_result != FT_ERR_SUCCESS)
        return (mutex_result);
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_promise<ValueType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
void ft_promise<ValueType>::set_value(const ValueType& value)
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::set_value(copy)");
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_value = value;
    ft_promise_detail::publish_ready(this->_readiness);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

inline void ft_promise<void>::set_value()
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::set_value");
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_promise_detail::publish_ready(this->_readiness);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ValueType>
void ft_promise<ValueType>::set_value(ValueType&& value)
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::set_value(move)");
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_value = ft_move(value);
    ft_promise_detail::publish_ready(this->_readiness);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ValueType>
ValueType ft_promise<ValueType>::get() const
{
    ft_bool lock_acquired;
    ValueType value_copy;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::get");
    if (this->_readiness.load(std::memory_order_acquire) != ft_promise_detail::READY)
        return (ValueType());
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (ValueType());
    value_copy = this->_value;
    (void)this->unlock_internal(lock_acquired);
    return (value_copy);
}

inline void ft_promise<void>::get() const
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::get");
    if (this->_readiness.load(std::memory_order_acquire) != ft_promise_detail::READY)
        return ;
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ValueType>
ft_bool ft_promise<ValueType>::is_ready() const
{
    ft_bool ready;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::is_ready");
    ready = this->_readiness.load(std::memory_order_acquire) == ft_promise_detail::READY;
    return (ready);
}

inline ft_bool ft_promise<void>::is_ready() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::is_ready");
    return (this->_readiness.load(std::memory_order_acquire) == ft_promise_detail::READY);
}

template <typename ValueType>
int32_t ft_promise<ValueType>::wait_ready() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::wait_ready");
    return (ft_promise_detail::wait_ready(this->_readiness));
}

inline int32_t ft_promise<void>::wait_ready() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::wait_ready");
    return (ft_promise_detail::wait_ready(this->_readiness));
}

template <typename ValueType>
int32_t ft_promise<ValueType>::enable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::enable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_promise<ValueType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_INVALID_STATE);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

inline int32_t ft_promise<void>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_INVALID_STATE);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
ft_bool ft_promise<ValueType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

inline ft_bool ft_promise<void>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_promise<void>::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}


#endif

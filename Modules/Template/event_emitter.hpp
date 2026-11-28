#ifndef FT_EVENT_EMITTER_HPP
#define FT_EVENT_EMITTER_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

template <typename EventType, typename... Args>
class ft_event_emitter
{
    private:
        struct Listener
        {
            EventType _event;
            void (*_callback)(Args...);
        };

        Listener                   *_listeners;
        ft_size_t                  _configured_initial_capacity;
        ft_size_t                  _capacity;
        ft_size_t                  _size;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                    _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        ft_bool ensure_capacity_unlocked(ft_size_t desired);
        ft_bool ensure_capacity(ft_size_t desired);
        void clear_unlocked();

    public:
        ft_event_emitter(ft_size_t initial_capacity = 0);
        ft_event_emitter(const ft_event_emitter<EventType, Args...> &other) = delete;
        ft_event_emitter(ft_event_emitter<EventType, Args...> &&other) = delete;
        ~ft_event_emitter();
        ft_event_emitter& operator=(const ft_event_emitter&) = delete;
        ft_event_emitter& operator=(ft_event_emitter&&) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_event_emitter<EventType, Args...> &other);

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void on(const EventType& event, void (*callback)(Args...));
        void emit(const EventType& event, Args... args);
        void remove_listener(const EventType& event, void (*callback)(Args...));
        ft_size_t size() const;
        ft_bool empty() const;
        void clear();

        int32_t get_error() const;
        const char *get_error_str() const;
};

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::set_error(int32_t error_code) noexcept
{
    ft_event_emitter<EventType, Args...>::_last_error = error_code;
    return (error_code);
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename EventType, typename... Args>
ft_bool ft_event_emitter<EventType, Args...>::ensure_capacity_unlocked(ft_size_t desired)
{
    ft_size_t maximum_capacity;
    ft_size_t new_capacity;
    Listener *new_data;

    if (desired <= this->_capacity)
        return (FT_TRUE);
    maximum_capacity = FT_SYSTEM_SIZE_MAX / sizeof(Listener);
    if (desired > maximum_capacity)
    {
        set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (this->_capacity == 0)
        new_capacity = 1;
    else
        new_capacity = this->_capacity;
    while (new_capacity < desired)
    {
        if (new_capacity >= maximum_capacity)
        {
            new_capacity = maximum_capacity;
            break ;
        }
        if (new_capacity > (maximum_capacity / 2))
            new_capacity = maximum_capacity;
        else
            new_capacity = new_capacity * 2;
    }
    new_data = static_cast<Listener *>(cma_malloc(sizeof(Listener) * new_capacity));
    if (new_data == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    if constexpr (std::is_trivially_move_constructible_v<Listener>
        && std::is_trivially_destructible_v<Listener>)
    {
        if (this->_size != 0U)
            std::uninitialized_move_n(this->_listeners, this->_size, new_data);
    }
    else
    {
        ft_size_t listener_index = 0U;
        while (listener_index < this->_size)
        {
            ::construct_at(&new_data[listener_index],
                ft_move(this->_listeners[listener_index]));
            ::destroy_at(&this->_listeners[listener_index]);
            ++listener_index;
        }
    }
    if (this->_listeners != ft_nullptr)
        cma_free(this->_listeners);
    this->_listeners = new_data;
    this->_capacity = new_capacity;
    return (FT_TRUE);
}

template <typename EventType, typename... Args>
ft_bool ft_event_emitter<EventType, Args...>::ensure_capacity(ft_size_t desired)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool ensured;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::ensure_capacity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_FALSE);
    }
    ensured = this->ensure_capacity_unlocked(desired);
    (void)this->unlock_internal(lock_acquired);
    if (ensured == FT_FALSE)
        return (FT_FALSE);
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::clear_unlocked()
{
    ft_size_t listener_index;

    listener_index = 0;
    while (listener_index < this->_size)
    {
        destroy_at(&this->_listeners[listener_index]);
        listener_index = listener_index + 1;
    }
    this->_size = 0;
    return ;
}

template <typename EventType, typename... Args>
ft_event_emitter<EventType, Args...>::ft_event_emitter(ft_size_t initial_capacity)
    : _listeners(ft_nullptr)
    , _configured_initial_capacity(initial_capacity)
    , _capacity(0)
    , _size(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename EventType, typename... Args>
ft_event_emitter<EventType, Args...>::~ft_event_emitter()
{
    uint32_t previous_error;

    previous_error = ft_event_emitter<EventType, Args...>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_event_emitter::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_listeners = ft_nullptr;
    this->_capacity = 0;
    this->_size = 0;
    if (this->_configured_initial_capacity > FT_SYSTEM_SIZE_MAX / sizeof(Listener))
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    if (this->_configured_initial_capacity > 0)
    {
        this->_listeners = static_cast<Listener *>(
                cma_malloc(sizeof(Listener) * this->_configured_initial_capacity));
        if (this->_listeners == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
        this->_capacity = this->_configured_initial_capacity;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::destroy()
{
    int32_t first_error;
    int32_t disable_error;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->clear_unlocked();
    if (this->_listeners != ft_nullptr)
    {
        cma_free(this->_listeners);
        this->_listeners = ft_nullptr;
    }
    this->_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::move(
    ft_event_emitter<EventType, Args...> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_event_emitter::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    this->_configured_initial_capacity = other._configured_initial_capacity;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_listeners = ft_nullptr;
        this->_capacity = 0;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_listeners = other._listeners;
    this->_capacity = other._capacity;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._listeners = ft_nullptr;
    other._capacity = 0;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::disable_thread_safety()
{
    int32_t destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

template <typename EventType, typename... Args>
ft_bool ft_event_emitter<EventType, Args...>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::lock");
    lock_error = this->lock_internal(lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    return (set_error(FT_ERR_SUCCESS));
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::on(const EventType& event,
    void (*callback)(Args...))
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::on");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->ensure_capacity_unlocked(this->_size + 1) == FT_FALSE)
    {
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    ::new (static_cast<void *>(this->_listeners + this->_size)) Listener{event, callback};
    this->_size = this->_size + 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::emit(const EventType& event, Args... args)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool found;
    ft_size_t listener_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::emit");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    found = FT_FALSE;
    listener_index = 0;
    while (listener_index < this->_size)
    {
        if (this->_listeners[listener_index]._event == event)
        {
            found = FT_TRUE;
            this->_listeners[listener_index]._callback(args...);
        }
        listener_index = listener_index + 1;
    }
    (void)this->unlock_internal(lock_acquired);
    if (found == FT_FALSE)
    {
        set_error(FT_ERR_NOT_FOUND);
        return ;
    }
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::remove_listener(const EventType& event,
    void (*callback)(Args...))
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t listener_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::remove_listener");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    listener_index = 0;
    while (listener_index < this->_size)
    {
        if (this->_listeners[listener_index]._event == event
            && this->_listeners[listener_index]._callback == callback)
        {
            if constexpr (std::is_trivially_copyable_v<Listener>
                && std::is_trivially_move_assignable_v<Listener>)
            {
                ft_memmove(this->_listeners + listener_index,
                    this->_listeners + listener_index + 1U,
                    (this->_size - listener_index - 1U) * sizeof(Listener));
            }
            else
            {
                ::destroy_at(&this->_listeners[listener_index]);
                ft_size_t shift_index = listener_index;
                while (shift_index + 1U < this->_size)
                {
                    ::construct_at(&this->_listeners[shift_index],
                        ft_move(this->_listeners[shift_index + 1U]));
                    ::destroy_at(&this->_listeners[shift_index + 1U]);
                    ++shift_index;
                }
            }
            this->_size = this->_size - 1;
            (void)this->unlock_internal(lock_acquired);
            set_error(FT_ERR_SUCCESS);
            return ;
        }
        listener_index = listener_index + 1;
    }
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_NOT_FOUND);
    return ;
}

template <typename EventType, typename... Args>
ft_size_t ft_event_emitter<EventType, Args...>::size() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t current_size;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    current_size = this->_size;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (current_size);
}

template <typename EventType, typename... Args>
ft_bool ft_event_emitter<EventType, Args...>::empty() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool is_empty;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (is_empty);
}

template <typename EventType, typename... Args>
void ft_event_emitter<EventType, Args...>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_event_emitter::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->clear_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename EventType, typename... Args>
int32_t ft_event_emitter<EventType, Args...>::get_error() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_event_emitter::get_error");
    return (ft_event_emitter<EventType, Args...>::_last_error);
}

template <typename EventType, typename... Args>
const char *ft_event_emitter<EventType, Args...>::get_error_str() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_event_emitter::get_error_str");
    return (ft_strerror(this->get_error()));
}

template <typename EventType, typename... Args>
thread_local int32_t ft_event_emitter<EventType, Args...>::_last_error = FT_ERR_SUCCESS;

#endif

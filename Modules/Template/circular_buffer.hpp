#ifndef FT_CIRCULAR_BUFFER_HPP
#define FT_CIRCULAR_BUFFER_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdint>
#include <new>
#include <type_traits>

template <typename ElementType>
class ft_circular_buffer
{
    private:
        ElementType                 *_buffer;
        ft_size_t                   _configured_capacity;
        ft_size_t                   _head;
        ft_size_t                   _tail;
        ft_size_t                   _size;
        mutable pt_recursive_mutex  *_mutex;
        uint8_t                     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void destroy_elements_locked();
        void release_buffer_locked();

    public:
        explicit ft_circular_buffer(ft_size_t capacity);
        ft_circular_buffer(const ft_circular_buffer<ElementType> &other) = delete;
        ft_circular_buffer(ft_circular_buffer<ElementType> &&other) = delete;
        ~ft_circular_buffer();
        ft_circular_buffer& operator=(const ft_circular_buffer&) = delete;
        ft_circular_buffer& operator=(ft_circular_buffer&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_circular_buffer<ElementType> &other);

        void push(const ElementType& value);
        void push(ElementType&& value);
        ElementType pop();

        ft_bool is_full() const;
        ft_bool is_empty() const;
        ft_size_t size() const;
        ft_size_t capacity() const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void clear();

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    lock_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_result != FT_ERR_SUCCESS)
        return (set_error(lock_result));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::destroy_elements_locked()
{
    if constexpr (!std::is_trivially_destructible_v<ElementType>)
    {
        ft_size_t remaining = this->_size;
        ft_size_t index = this->_head;
        while (remaining != 0U)
        {
            ::destroy_at(&this->_buffer[index]);
            if (++index == this->_configured_capacity)
                index = 0U;
            --remaining;
        }
    }
    return ;
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::release_buffer_locked()
{
    if (this->_buffer != ft_nullptr)
    {
        cma_free(this->_buffer);
        this->_buffer = ft_nullptr;
    }
    return ;
}

template <typename ElementType>
ft_circular_buffer<ElementType>::ft_circular_buffer(ft_size_t capacity)
    : _buffer(ft_nullptr), _configured_capacity(capacity), _head(0), _tail(0),
      _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_circular_buffer<ElementType>::~ft_circular_buffer()
{
    uint32_t previous_error;

    previous_error = ft_circular_buffer<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_circular_buffer::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_buffer = ft_nullptr;
    this->_head = 0;
    this->_tail = 0;
    this->_size = 0;
    if (this->_configured_capacity > FT_SYSTEM_SIZE_MAX / sizeof(ElementType))
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    if (this->_configured_capacity > 0)
    {
        this->_buffer = static_cast<ElementType*>(cma_malloc(sizeof(ElementType)
                    * this->_configured_capacity));
        if (this->_buffer == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_result;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS)
        first_error = disable_result;
    this->destroy_elements_locked();
    this->release_buffer_locked();
    this->_head = 0;
    this->_tail = 0;
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::move(ft_circular_buffer<ElementType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_circular_buffer::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    this->_configured_capacity = other._configured_capacity;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_buffer = ft_nullptr;
        this->_head = 0;
        this->_tail = 0;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_buffer = other._buffer;
    this->_head = other._head;
    this->_tail = other._tail;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._buffer = ft_nullptr;
    other._head = 0;
    other._tail = 0;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::push(const ElementType& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::push(copy)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_size == this->_configured_capacity)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_FULL);
        return ;
    }
    construct_at(&this->_buffer[this->_tail], value);
    if (++this->_tail == this->_configured_capacity)
        this->_tail = 0U;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::push(ElementType&& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::push(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_size == this->_configured_capacity)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_FULL);
        return ;
    }
    construct_at(&this->_buffer[this->_tail], ft_move(value));
    if (++this->_tail == this->_configured_capacity)
        this->_tail = 0U;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ElementType ft_circular_buffer<ElementType>::pop()
{
    ElementType value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::pop");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ElementType());
    }
    if (this->_size == 0)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (ElementType());
    }
    value = ft_move(this->_buffer[this->_head]);
    destroy_at(&this->_buffer[this->_head]);
    if (++this->_head == this->_configured_capacity)
        this->_head = 0U;
    this->_size -= 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ft_bool ft_circular_buffer<ElementType>::is_full() const
{
    ft_bool value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::is_full");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_FALSE);
    }
    value = (this->_size == this->_configured_capacity);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ft_bool ft_circular_buffer<ElementType>::is_empty() const
{
    ft_bool value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::is_empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_TRUE);
    }
    value = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ft_size_t ft_circular_buffer<ElementType>::size() const
{
    ft_size_t value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    value = this->_size;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ft_size_t ft_circular_buffer<ElementType>::capacity() const
{
    ft_size_t value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::capacity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    value = this->_configured_capacity;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
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
    this->_mutex = new_mutex;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (set_error(FT_ERR_INVALID_STATE));
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (set_error(destroy_result));
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
ft_bool ft_circular_buffer<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_circular_buffer<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_circular_buffer::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->destroy_elements_locked();
    this->_head = 0;
    this->_tail = 0;
    this->_size = 0;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_circular_buffer<ElementType>::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_circular_buffer::get_error");
    return (_last_error);
}

template <typename ElementType>
const char *ft_circular_buffer<ElementType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_circular_buffer::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename ElementType>
thread_local int32_t ft_circular_buffer<ElementType>::_last_error = FT_ERR_SUCCESS;

#endif

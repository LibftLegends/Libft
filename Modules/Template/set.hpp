#ifndef FT_SET_HPP
#define FT_SET_HPP

#include "constructor.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "move.hpp"
#include <memory>
#include <type_traits>

template <typename ElementType>
class ft_set
{
    private:
        ElementType*                  _data;
        ft_size_t                     _configured_initial_capacity;
        ft_size_t                     _capacity;
        ft_size_t                     _size;
        mutable pt_recursive_mutex*   _mutex;
        uint8_t                       _initialised_state;
        static thread_local int32_t   _last_error;
        static constexpr ft_bool CAN_SHIFT_BYTES = std::is_trivially_copyable_v<ElementType>
            && std::is_trivially_move_constructible_v<ElementType>
            && std::is_trivially_move_assignable_v<ElementType>;

        static int32_t set_error(int32_t error_code);
        ft_bool ensure_capacity(ft_size_t desired_capacity);
        void make_room(ft_size_t position);
        ft_size_t find_index(const ElementType& value) const;
        ft_size_t lower_bound(const ElementType& value) const;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        ft_set(ft_size_t initial_capacity = 0);
        ft_set(const ft_set &other) = delete;
        ft_set(ft_set &&other) = delete;
        ~ft_set();
        ft_set& operator=(const ft_set&) = delete;
        ft_set& operator=(ft_set&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_set<ElementType> &other);
        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void insert(const ElementType& value);
        void insert(ElementType&& value);

        ElementType* find(const ElementType& value);
        const ElementType* find(const ElementType& value) const;

        void remove(const ElementType& value);

        ft_size_t size() const;
        ft_bool empty() const;

        void clear();

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ElementType>
thread_local int32_t ft_set<ElementType>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType>
int32_t ft_set<ElementType>::set_error(int32_t error_code)
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
ft_bool ft_set<ElementType>::ensure_capacity(ft_size_t desired_capacity)
{
    ft_size_t new_capacity;
    ElementType* new_data;
    if (desired_capacity <= this->_capacity)
        return (FT_TRUE);
    constexpr ft_size_t MAXIMUM_CAPACITY = FT_SYSTEM_SIZE_MAX / sizeof(ElementType);
    if (desired_capacity > MAXIMUM_CAPACITY)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    new_capacity = desired_capacity;
    if (this->_capacity <= MAXIMUM_CAPACITY / 2U
        && this->_capacity * 2U > desired_capacity)
        new_capacity = this->_capacity * 2U;
    new_data = static_cast<ElementType*>(cma_malloc(sizeof(ElementType) * new_capacity));
    if (new_data == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    if constexpr (std::is_trivially_move_constructible_v<ElementType>
        && std::is_trivially_destructible_v<ElementType>)
    {
        if (this->_size != 0U)
            std::uninitialized_move_n(this->_data, this->_size, new_data);
    }
    else
    {
        ft_size_t index = 0U;
        while (index < this->_size)
        {
            ::construct_at(&new_data[index], ft_move(this->_data[index]));
            ::destroy_at(&this->_data[index]);
            ++index;
        }
    }
    if (this->_data != ft_nullptr)
        cma_free(this->_data);
    this->_data = new_data;
    this->_capacity = new_capacity;
    return (FT_TRUE);
}

template <typename ElementType>
void ft_set<ElementType>::make_room(ft_size_t position)
{
    if (position == this->_size)
        return ;
    if constexpr (CAN_SHIFT_BYTES)
    {
        const ft_size_t last_index = this->_size - 1U;
        // The extra slot has no live object until it is constructed here.
        ::construct_at(this->_data + this->_size, ft_move(this->_data[last_index]));
        if (position < last_index)
            ft_memmove(this->_data + position + 1U, this->_data + position,
                (last_index - position) * sizeof(ElementType));
    }
    else
    {
        ft_size_t index = this->_size;
        while (index > position)
        {
            ::construct_at(&this->_data[index], ft_move(this->_data[index - 1U]));
            ::destroy_at(&this->_data[index - 1U]);
            --index;
        }
    }
    return ;
}

template <typename ElementType>
ft_size_t ft_set<ElementType>::find_index(const ElementType& value) const
{
    ft_size_t left;
    ft_size_t right;
    ft_size_t middle;

    left = 0;
    right = this->_size;
    while (left < right)
    {
        middle = left + (right - left) / 2;
        if (this->_data[middle] < value)
            left = middle + 1;
        else if (value < this->_data[middle])
            right = middle;
        else
            return (middle);
    }
    return (this->_size);
}

template <typename ElementType>
ft_size_t ft_set<ElementType>::lower_bound(const ElementType& value) const
{
    ft_size_t left;
    ft_size_t right;
    ft_size_t middle;

    left = 0;
    right = this->_size;
    while (left < right)
    {
        middle = left + (right - left) / 2;
        if (this->_data[middle] < value)
            left = middle + 1;
        else
            right = middle;
    }
    return (left);
}

template <typename ElementType>
int32_t ft_set<ElementType>::lock_internal(ft_bool *lock_acquired) const
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int32_t ft_set<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
ft_set<ElementType>::ft_set(ft_size_t initial_capacity)
    : _data(ft_nullptr), _configured_initial_capacity(initial_capacity),
      _capacity(0), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_set<ElementType>::~ft_set()
{
    uint32_t previous_error;

    previous_error = ft_set<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_set<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_set::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = ft_nullptr;
    this->_capacity = 0;
    this->_size = 0;
    this->_mutex = ft_nullptr;
    if (this->_configured_initial_capacity > FT_SYSTEM_SIZE_MAX / sizeof(ElementType))
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    if (this->_configured_initial_capacity > 0)
    {
        this->_data = static_cast<ElementType*>(cma_malloc(sizeof(ElementType)
                    * this->_configured_initial_capacity));
        if (this->_data == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
        this->_capacity = this->_configured_initial_capacity;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_set<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_result;
    ft_size_t index;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS)
        first_error = disable_result;
    index = 0;
    while (index < this->_size)
    {
        ::destroy_at(&this->_data[index]);
        index += 1;
    }
    this->_size = 0;
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    this->_capacity = 0;
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_set<ElementType>::move(ft_set<ElementType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_set::move",
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
        this->_data = ft_nullptr;
        this->_capacity = 0;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_data = other._data;
    this->_capacity = other._capacity;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._capacity = 0;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_set<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_set::enable_thread_safety");
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
int32_t ft_set<ElementType>::disable_thread_safety()
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
ft_bool ft_set<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_set::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_set<ElementType>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::lock");
    return (set_error(this->lock_internal(lock_acquired)));
}

template <typename ElementType>
void ft_set<ElementType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_set<ElementType>::insert(const ElementType& value)
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t position;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::insert");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    position = this->lower_bound(value);
    if (position < this->_size && !(value < this->_data[position])
        && !(this->_data[position] < value))
    {
        set_error(FT_ERR_SUCCESS);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    if (!this->ensure_capacity(this->_size + 1))
    {
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->make_room(position);
    ::construct_at(&this->_data[position], value);
    this->_size += 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
void ft_set<ElementType>::insert(ElementType&& value)
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t position;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::insert(move)");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    position = this->lower_bound(value);
    if (position < this->_size && !(value < this->_data[position])
        && !(this->_data[position] < value))
    {
        set_error(FT_ERR_SUCCESS);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    if (!this->ensure_capacity(this->_size + 1))
    {
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->make_room(position);
    ::construct_at(&this->_data[position], ft_move(value));
    this->_size += 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
ElementType* ft_set<ElementType>::find(const ElementType& value)
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t index;
    ElementType *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::find");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (ft_nullptr);
    index = this->find_index(value);
    if (index == this->_size)
    {
        set_error(FT_ERR_NOT_FOUND);
        (void)this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    result = &this->_data[index];
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

template <typename ElementType>
const ElementType* ft_set<ElementType>::find(const ElementType& value) const
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t index;
    const ElementType *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::find const");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (ft_nullptr);
    index = this->find_index(value);
    if (index == this->_size)
    {
        set_error(FT_ERR_NOT_FOUND);
        (void)this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    result = &this->_data[index];
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

template <typename ElementType>
void ft_set<ElementType>::remove(const ElementType& value)
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::remove");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    index = this->find_index(value);
    if (index == this->_size)
    {
        set_error(FT_ERR_NOT_FOUND);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    if constexpr (CAN_SHIFT_BYTES)
    {
        if (index + 1U < this->_size)
            ft_memmove(this->_data + index, this->_data + index + 1U,
                (this->_size - index - 1U) * sizeof(ElementType));
    }
    else
    {
        ::destroy_at(&this->_data[index]);
        while (index + 1U < this->_size)
        {
            ::construct_at(&this->_data[index], ft_move(this->_data[index + 1U]));
            ::destroy_at(&this->_data[index + 1U]);
            ++index;
        }
    }
    this->_size -= 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
ft_size_t ft_set<ElementType>::size() const
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t current_size;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::size");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (0);
    current_size = this->_size;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (current_size);
}

template <typename ElementType>
ft_bool ft_set<ElementType>::empty() const
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_bool result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::empty");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (FT_TRUE);
    result = (this->_size == 0);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

template <typename ElementType>
void ft_set<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_set::clear");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    index = 0;
    while (index < this->_size)
    {
        ::destroy_at(&this->_data[index]);
        index += 1;
    }
    this->_size = 0;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
int32_t ft_set<ElementType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_set::get_error");
    return (_last_error);
}

template <typename ElementType>
const char *ft_set<ElementType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_set::get_error_str");
    return (ft_strerror(_last_error));
}

#endif

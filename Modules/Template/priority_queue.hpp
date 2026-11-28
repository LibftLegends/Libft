#ifndef FT_PRIORITY_QUEUE_HPP
#define FT_PRIORITY_QUEUE_HPP

#include "constructor.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include "move.hpp"

template <typename ElementType, typename Compare = std::less<ElementType> >
class ft_priority_queue
{
    private:
        ElementType*                 _data;
        ft_size_t                    _configured_initial_capacity;
        ft_size_t                    _capacity;
        ft_size_t                    _size;
        Compare                      _comp;
        mutable pt_recursive_mutex*  _mutex;
        uint8_t                      _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code);
        ft_bool ensure_capacity(ft_size_t desired);
        void heapify_up(ft_size_t index);
        void heapify_down(ft_size_t index);
        void rebuild_heap();
        ft_size_t preferred_child(ft_size_t index, ElementType &value);
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        ft_priority_queue(ft_size_t initial_capacity = 0,
            const Compare& comp = Compare());
        ft_priority_queue(const ft_priority_queue<ElementType, Compare> &other) = delete;
        ft_priority_queue(ft_priority_queue<ElementType, Compare> &&other) = delete;
        ~ft_priority_queue();
        ft_priority_queue& operator=(const ft_priority_queue&) = delete;
        ft_priority_queue& operator=(ft_priority_queue&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_priority_queue<ElementType, Compare> &other);
        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void push(const ElementType& value);
        void push(ElementType&& value);
        ElementType pop();
        // Callbacks run under the queue lock in heap order and must not throw
        // or call back into the queue. A true result erases or marks a change.
        template <typename Predicate>
        ft_bool erase_if(Predicate predicate);
        template <typename Update>
        ft_bool update_elements(Update update);

        ElementType& top();
        const ElementType& top() const;

        ft_size_t size() const;
        ft_size_t capacity() const;
        int32_t reserve(ft_size_t capacity);
        ft_bool empty() const;

        void clear();

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ElementType, typename Compare>
thread_local int32_t ft_priority_queue<ElementType, Compare>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::set_error(int32_t error_code)
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType, typename Compare>
ft_bool ft_priority_queue<ElementType, Compare>::ensure_capacity(ft_size_t desired)
{
    ft_size_t new_capacity;
    ElementType* new_data;
    constexpr ft_size_t MAXIMUM_CAPACITY = FT_SYSTEM_SIZE_MAX / sizeof(ElementType);
    if (desired > MAXIMUM_CAPACITY)
    {
        this->set_error(FT_ERR_PRIORITY_QUEUE_NO_MEMORY);
        return (FT_FALSE);
    }
    new_capacity = desired;
    if (this->_capacity <= MAXIMUM_CAPACITY / 2U && desired < this->_capacity * 2U)
        new_capacity = this->_capacity * 2U;
    new_data = static_cast<ElementType*>(cma_malloc(sizeof(ElementType) * new_capacity));
    if (new_data == ft_nullptr)
    {
        set_error(FT_ERR_PRIORITY_QUEUE_NO_MEMORY);
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
        ft_size_t element_index = 0U;
        while (element_index < this->_size)
        {
            ::construct_at(&new_data[element_index], ft_move(this->_data[element_index]));
            ::destroy_at(&this->_data[element_index]);
            ++element_index;
        }
    }
    if (this->_data != ft_nullptr)
        cma_free(this->_data);
    this->_data = new_data;
    this->_capacity = new_capacity;
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::heapify_up(ft_size_t index)
{
    if (index == 0U)
        return ;
    ft_size_t parent_index = (index - 1U) / 2U;
    if (!this->_comp(this->_data[parent_index], this->_data[index]))
        return ;
    ElementType value(ft_move(this->_data[index]));
    while (FT_TRUE)
    {
        this->_data[index] = ft_move(this->_data[parent_index]);
        index = parent_index;
        if (index == 0U)
            break ;
        parent_index = (index - 1U) / 2U;
        if (!this->_comp(this->_data[parent_index], value))
            break ;
    }
    this->_data[index] = ft_move(value);
    return ;
}

template <typename ElementType, typename Compare>
ft_size_t ft_priority_queue<ElementType, Compare>::preferred_child(ft_size_t index,
    ElementType &value)
{
    if (index >= this->_size / 2U)
        return (index);
    const ft_size_t left_child_index = index * 2U + 1U;
    const ft_size_t right_child_index = left_child_index + 1U;
    ft_size_t preferred_index = index;
    ElementType *preferred = std::addressof(value);
    if (this->_comp(value, this->_data[left_child_index]))
    {
        preferred_index = left_child_index;
        preferred = this->_data + left_child_index;
    }
    if (right_child_index < this->_size
        && this->_comp(*preferred, this->_data[right_child_index]))
        preferred_index = right_child_index;
    return (preferred_index);
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::heapify_down(ft_size_t index)
{
    ft_size_t child_index = this->preferred_child(index, this->_data[index]);
    if (child_index == index)
        return ;
    ElementType value(ft_move(this->_data[index]));
    while (child_index != index)
    {
        this->_data[index] = ft_move(this->_data[child_index]);
        index = child_index;
        child_index = this->preferred_child(index, value);
    }
    this->_data[index] = ft_move(value);
    return ;
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::rebuild_heap()
{
    ft_size_t parent_count = this->_size / 2U;
    while (parent_count > 0U)
    {
        --parent_count;
        this->heapify_down(parent_count);
    }
    return ;
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::lock_internal(ft_bool *lock_acquired) const
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

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType, typename Compare>
ft_priority_queue<ElementType, Compare>::ft_priority_queue(ft_size_t initial_capacity,
    const Compare& comp)
    : _data(ft_nullptr), _configured_initial_capacity(initial_capacity),
      _capacity(0), _size(0), _comp(comp), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType, typename Compare>
ft_priority_queue<ElementType, Compare>::~ft_priority_queue()
{
    uint32_t previous_error;

    previous_error = ft_priority_queue<ElementType, Compare>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "ft_priority_queue::initialize",
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
        return (this->set_error(FT_ERR_PRIORITY_QUEUE_NO_MEMORY));
    }
    if (this->_configured_initial_capacity > 0)
    {
        this->_data = static_cast<ElementType *>(cma_malloc(sizeof(ElementType)
                    * this->_configured_initial_capacity));
        if (this->_data == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_PRIORITY_QUEUE_NO_MEMORY));
        }
        this->_capacity = this->_configured_initial_capacity;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::destroy()
{
    int32_t first_error;
    int32_t disable_result;
    ft_size_t element_index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_result;
    element_index = 0;
    while (element_index < this->_size)
    {
        ::destroy_at(&this->_data[element_index]);
        element_index += 1;
    }
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

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::move(
    ft_priority_queue<ElementType, Compare> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_priority_queue::move",
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
    this->_comp = other._comp;
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

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::enable_thread_safety");
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

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::disable_thread_safety()
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

template <typename ElementType, typename Compare>
ft_bool ft_priority_queue<ElementType, Compare>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::lock");
    return (set_error(this->lock_internal(lock_acquired)));
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::push(const ElementType& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::push");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_size == this->_capacity && !this->ensure_capacity(this->_size + 1U))
    {
        set_error(FT_ERR_NO_MEMORY);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    ::construct_at(&this->_data[this->_size], value);
    this->heapify_up(this->_size);
    this->_size += 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::push(ElementType&& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::push(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_size == this->_capacity && !this->ensure_capacity(this->_size + 1U))
    {
        set_error(FT_ERR_NO_MEMORY);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    ::construct_at(&this->_data[this->_size], ft_move(value));
    this->heapify_up(this->_size);
    this->_size += 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType, typename Compare>
ElementType ft_priority_queue<ElementType, Compare>::pop()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::pop");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ElementType());
    if (this->_size == 0)
    {
        set_error(FT_ERR_PRIORITY_QUEUE_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (ElementType());
    }
    value = ft_move(this->_data[0]);
    ::destroy_at(&this->_data[0]);
    this->_size -= 1;
    if (this->_size > 0)
    {
        ::construct_at(&this->_data[0], ft_move(this->_data[this->_size]));
        ::destroy_at(&this->_data[this->_size]);
        this->heapify_down(0);
    }
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

template <typename ElementType, typename Compare>
template <typename Predicate>
ft_bool ft_priority_queue<ElementType, Compare>::erase_if(Predicate predicate)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::erase_if");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    ft_size_t source_index = 0U;
    ft_size_t retained_count = 0U;
    while (source_index < this->_size)
    {
        if (predicate(static_cast<const ElementType &>(this->_data[source_index])))
            ::destroy_at(&this->_data[source_index]);
        else
        {
            if (retained_count != source_index)
            {
                ::construct_at(&this->_data[retained_count], ft_move(this->_data[source_index]));
                ::destroy_at(&this->_data[source_index]);
            }
            ++retained_count;
        }
        ++source_index;
    }
    const ft_bool erased = retained_count != this->_size;
    if (erased)
    {
        this->_size = retained_count;
        this->rebuild_heap();
    }
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (erased);
}

template <typename ElementType, typename Compare>
template <typename Update>
ft_bool ft_priority_queue<ElementType, Compare>::update_elements(Update update)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::update_elements");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    ft_size_t index = 0U;
    ft_bool changed = FT_FALSE;
    while (index < this->_size)
    {
        if (update(this->_data[index]))
            changed = FT_TRUE;
        ++index;
    }
    if (changed)
        this->rebuild_heap();
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (changed);
}

template <typename ElementType, typename Compare>
ElementType& ft_priority_queue<ElementType, Compare>::top()
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType *value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::top");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (error_element);
    if (this->_size == 0)
    {
        set_error(FT_ERR_PRIORITY_QUEUE_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (error_element);
    }
    value = &this->_data[0];
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*value);
}

template <typename ElementType, typename Compare>
const ElementType& ft_priority_queue<ElementType, Compare>::top() const
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    const ElementType *value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::top const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (error_element);
    if (this->_size == 0)
    {
        set_error(FT_ERR_PRIORITY_QUEUE_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (error_element);
    }
    value = &this->_data[0];
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*value);
}

template <typename ElementType, typename Compare>
ft_size_t ft_priority_queue<ElementType, Compare>::size() const
{
    ft_size_t current_size;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    current_size = this->_size;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (current_size);
}

template <typename ElementType, typename Compare>
ft_size_t ft_priority_queue<ElementType, Compare>::capacity() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::capacity");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (0U);
    const ft_size_t capacity = this->_capacity;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (capacity);
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::reserve(ft_size_t capacity)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::reserve");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    int32_t result = FT_ERR_SUCCESS;
    if (capacity > this->_capacity && !this->ensure_capacity(capacity))
        result = this->get_error();
    set_error(result);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

template <typename ElementType, typename Compare>
ft_bool ft_priority_queue<ElementType, Compare>::empty() const
{
    ft_bool result;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_TRUE);
    result = (this->_size == 0);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

template <typename ElementType, typename Compare>
void ft_priority_queue<ElementType, Compare>::clear()
{
    ft_size_t element_index;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    element_index = 0;
    while (element_index < this->_size)
    {
        ::destroy_at(&this->_data[element_index]);
        element_index += 1;
    }
    this->_size = 0;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType, typename Compare>
int32_t ft_priority_queue<ElementType, Compare>::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::get_error");
    return (_last_error);
}

template <typename ElementType, typename Compare>
const char *ft_priority_queue<ElementType, Compare>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_priority_queue::get_error_str");
    return (ft_strerror(_last_error));
}

#endif

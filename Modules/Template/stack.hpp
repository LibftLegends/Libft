#ifndef FT_STACK_HPP
#define FT_STACK_HPP

#include "constructor.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstddef>
#include <utility>
#include "move.hpp"

template <typename ElementType>
class ft_stack
{
    private:
        struct StackNode
        {
            ElementType _data;
            StackNode* _next;
        };

        StackNode* _top;
        StackNode* _spare_node;
        ft_size_t _size;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code);
        void destroy_all_unlocked();
        template <typename ValueType>
        void push_impl(ValueType &&value, const char *operation);
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        ft_stack();
        ft_stack(const ft_stack &other) = delete;
        ft_stack(ft_stack &&other) = delete;
        ~ft_stack();
        ft_stack& operator=(const ft_stack&) = delete;
        ft_stack& operator=(ft_stack&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_stack<ElementType> &other);
        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void push(const ElementType& value);
        void push(ElementType&& value);
        ElementType pop();

        ElementType& top();
        const ElementType& top() const;

        ft_size_t size() const;
        ft_bool empty() const;

        void clear();

        int32_t get_error() const;
        const char *get_error_str() const;
};

template <typename ElementType>
thread_local int32_t ft_stack<ElementType>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType>
int32_t ft_stack<ElementType>::set_error(int32_t error_code)
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
void ft_stack<ElementType>::destroy_all_unlocked()
{
    StackNode *node;

    while (this->_top != ft_nullptr)
    {
        node = this->_top;
        this->_top = node->_next;
        ::destroy_at(&node->_data);
        cma_free(node);
    }
    if (this->_spare_node != ft_nullptr)
    {
        cma_free(this->_spare_node);
        this->_spare_node = ft_nullptr;
    }
    this->_size = 0;
    return ;
}

template <typename ElementType>
int32_t ft_stack<ElementType>::lock_internal(ft_bool *lock_acquired) const
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
int32_t ft_stack<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
ft_stack<ElementType>::ft_stack()
    : _top(ft_nullptr), _spare_node(ft_nullptr), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_stack<ElementType>::~ft_stack()
{
    uint32_t previous_error;

    previous_error = ft_stack<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_stack<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_stack::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_top = ft_nullptr;
    this->_spare_node = ft_nullptr;
    this->_size = 0;
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_stack<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_error;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->destroy_all_unlocked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_stack<ElementType>::move(ft_stack<ElementType> &other)
{
    int32_t destroy_result;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_stack::move",
            "source object is not initialised");
    }
    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(other._last_error));
    }
    this->_top = other._top;
    this->_spare_node = other._spare_node;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._top = ft_nullptr;
    other._spare_node = ft_nullptr;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(other._last_error));
}

template <typename ElementType>
int32_t ft_stack<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_stack::enable_thread_safety");
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
int32_t ft_stack<ElementType>::disable_thread_safety()
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
ft_bool ft_stack<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_stack::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_stack<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::lock");
    result = this->lock_internal(lock_acquired);
    return (set_error(result));
}

template <typename ElementType>
void ft_stack<ElementType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_stack<ElementType>::push(const ElementType& value)
{
    this->push_impl(value, "ft_stack::push");
    return ;
}

template <typename ElementType>
void ft_stack<ElementType>::push(ElementType&& value)
{
    this->push_impl(ft_move(value), "ft_stack::push(move)");
    return ;
}

template <typename ElementType>
template <typename ValueType>
void ft_stack<ElementType>::push_impl(ValueType &&value, const char *operation)
{
    StackNode *new_node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, operation);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    new_node = this->_spare_node;
    if (new_node != ft_nullptr)
        this->_spare_node = ft_nullptr;
    else
        new_node = static_cast<StackNode *>(cma_malloc(sizeof(StackNode)));
    if (new_node == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    ::construct_at(&new_node->_data, std::forward<ValueType>(value));
    new_node->_next = this->_top;
    this->_top = new_node;
    this->_size += 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
ElementType ft_stack<ElementType>::pop()
{
    ft_bool lock_acquired;
    StackNode *node;
    ElementType value;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::pop");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ElementType());
    if (this->_top == ft_nullptr)
    {
        set_error(FT_ERR_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (ElementType());
    }
    node = this->_top;
    this->_top = node->_next;
    value = ft_move(node->_data);
    ::destroy_at(&node->_data);
    if (this->_spare_node == ft_nullptr)
        this->_spare_node = node;
    else
        cma_free(node);
    this->_size -= 1;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

template <typename ElementType>
ElementType& ft_stack<ElementType>::top()
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType *value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::top");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (error_element);
    if (this->_top == ft_nullptr)
    {
        set_error(FT_ERR_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (error_element);
    }
    value = &this->_top->_data;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*value);
}

template <typename ElementType>
const ElementType& ft_stack<ElementType>::top() const
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    const ElementType *value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::top const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (error_element);
    if (this->_top == ft_nullptr)
    {
        set_error(FT_ERR_EMPTY);
        (void)this->unlock_internal(lock_acquired);
        return (error_element);
    }
    value = &this->_top->_data;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*value);
}

template <typename ElementType>
ft_size_t ft_stack<ElementType>::size() const
{
    ft_size_t current_size;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    current_size = this->_size;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (current_size);
}

template <typename ElementType>
ft_bool ft_stack<ElementType>::empty() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool is_empty;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_TRUE);
    is_empty = (this->_top == ft_nullptr);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (is_empty);
}

template <typename ElementType>
void ft_stack<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stack::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->destroy_all_unlocked();
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
int32_t ft_stack<ElementType>::get_error() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_stack::get_error");
    return (_last_error);
}

template <typename ElementType>
const char *ft_stack<ElementType>::get_error_str() const
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_stack::get_error_str");
    return (ft_strerror(_last_error));
}

#endif

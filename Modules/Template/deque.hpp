#ifndef FT_DEQUE_HPP
#define FT_DEQUE_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "../Basic/limits.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdint>
#include <new>

template <typename ElementType>
class ft_deque
{
    private:
        struct deque_node
        {
            ElementType _data;
            deque_node  *_prev;
            deque_node  *_next;
        };

        deque_node                 *_front;
        deque_node                 *_back;
        ft_size_t                  _size;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                    _initialised_state;

        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void destroy_all_unlocked();

    public:
        ft_deque();
        ft_deque(const ft_deque &other) = delete;
        ft_deque(ft_deque &&other) = delete;
        ~ft_deque();
        ft_deque &operator=(const ft_deque &other) = delete;
        ft_deque &operator=(ft_deque &&other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_deque<ElementType> &other);

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void push_front(const ElementType& value);
        void push_front(ElementType&& value);
        void push_back(const ElementType& value);
        void push_back(ElementType&& value);

        ElementType pop_front();
        ElementType pop_back();

        ElementType& front();
        const ElementType& front() const;
        ElementType& back();
        const ElementType& back() const;

        ft_size_t size() const;
        ft_bool empty() const;
        void clear();

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ElementType>
int32_t ft_deque<ElementType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
int32_t ft_deque<ElementType>::lock_internal(ft_bool *lock_acquired) const
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
int32_t ft_deque<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_deque<ElementType>::destroy_all_unlocked()
{
    deque_node *node;

    while (this->_front != ft_nullptr)
    {
        node = this->_front;
        this->_front = this->_front->_next;
        destroy_at(&node->_data);
        cma_free(node);
    }
    this->_back = ft_nullptr;
    this->_size = 0;
    return ;
}

template <typename ElementType>
ft_deque<ElementType>::ft_deque()
    : _front(ft_nullptr), _back(ft_nullptr), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_deque<ElementType>::~ft_deque()
{
    int32_t previous_error;

    previous_error = ft_deque<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_deque<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_deque::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_front = ft_nullptr;
    this->_back = ft_nullptr;
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_deque<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_result;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS)
        first_error = disable_result;
    this->destroy_all_unlocked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_deque<ElementType>::move(ft_deque<ElementType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_deque::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_front = ft_nullptr;
        this->_back = ft_nullptr;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_front = other._front;
    this->_back = other._back;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._front = ft_nullptr;
    other._back = ft_nullptr;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_deque<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::enable_thread_safety");
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
int32_t ft_deque<ElementType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::disable_thread_safety");
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
ft_bool ft_deque<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_deque<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename ElementType>
void ft_deque<ElementType>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_deque<ElementType>::push_front(const ElementType& value)
{
    deque_node *new_node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::push_front");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    new_node = static_cast<deque_node*>(cma_malloc(sizeof(deque_node)));
    if (new_node == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    construct_at(&new_node->_data, value);
    new_node->_prev = ft_nullptr;
    new_node->_next = this->_front;
    if (this->_front == ft_nullptr)
        this->_back = new_node;
    else
        this->_front->_prev = new_node;
    this->_front = new_node;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_deque<ElementType>::push_front(ElementType&& value)
{
    deque_node *new_node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::push_front(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    new_node = static_cast<deque_node*>(cma_malloc(sizeof(deque_node)));
    if (new_node == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    construct_at(&new_node->_data, ft_move(value));
    new_node->_prev = ft_nullptr;
    new_node->_next = this->_front;
    if (this->_front == ft_nullptr)
        this->_back = new_node;
    else
        this->_front->_prev = new_node;
    this->_front = new_node;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_deque<ElementType>::push_back(const ElementType& value)
{
    deque_node *new_node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::push_back");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    new_node = static_cast<deque_node*>(cma_malloc(sizeof(deque_node)));
    if (new_node == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    construct_at(&new_node->_data, value);
    new_node->_next = ft_nullptr;
    new_node->_prev = this->_back;
    if (this->_back == ft_nullptr)
        this->_front = new_node;
    else
        this->_back->_next = new_node;
    this->_back = new_node;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_deque<ElementType>::push_back(ElementType&& value)
{
    deque_node *new_node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::push_back(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    new_node = static_cast<deque_node*>(cma_malloc(sizeof(deque_node)));
    if (new_node == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    construct_at(&new_node->_data, ft_move(value));
    new_node->_next = ft_nullptr;
    new_node->_prev = this->_back;
    if (this->_back == ft_nullptr)
        this->_front = new_node;
    else
        this->_back->_next = new_node;
    this->_back = new_node;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ElementType ft_deque<ElementType>::pop_front()
{
    deque_node *node;
    ElementType value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::pop_front");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ElementType());
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (ElementType());
    }
    node = this->_front;
    this->_front = node->_next;
    if (this->_front == ft_nullptr)
        this->_back = ft_nullptr;
    else
        this->_front->_prev = ft_nullptr;
    value = ft_move(node->_data);
    destroy_at(&node->_data);
    cma_free(node);
    this->_size -= 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ElementType ft_deque<ElementType>::pop_back()
{
    deque_node *node;
    ElementType value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::pop_back");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ElementType());
    }
    if (this->_back == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (ElementType());
    }
    node = this->_back;
    this->_back = node->_prev;
    if (this->_back == ft_nullptr)
        this->_front = ft_nullptr;
    else
        this->_back->_next = ft_nullptr;
    value = ft_move(node->_data);
    destroy_at(&node->_data);
    cma_free(node);
    this->_size -= 1;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (value);
}

template <typename ElementType>
ElementType& ft_deque<ElementType>::front()
{
    static ElementType error_element = ElementType();
    ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::front");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value_pointer = &this->_front->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
const ElementType& ft_deque<ElementType>::front() const
{
    static ElementType error_element = ElementType();
    const ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::front const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value_pointer = &this->_front->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
ElementType& ft_deque<ElementType>::back()
{
    static ElementType error_element = ElementType();
    ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::back");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (this->_back == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value_pointer = &this->_back->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
const ElementType& ft_deque<ElementType>::back() const
{
    static ElementType error_element = ElementType();
    const ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::back const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (this->_back == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value_pointer = &this->_back->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
ft_size_t ft_deque<ElementType>::size() const
{
    ft_size_t current_size;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::size");
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

template <typename ElementType>
ft_bool ft_deque<ElementType>::empty() const
{
    ft_bool is_empty;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::empty");
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

template <typename ElementType>
void ft_deque<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_deque::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->destroy_all_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_deque<ElementType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_deque::get_error");
    return (_last_error);
}

template <typename ElementType>
const char *ft_deque<ElementType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_deque::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename ElementType>
thread_local int32_t ft_deque<ElementType>::_last_error = FT_ERR_SUCCESS;

#endif

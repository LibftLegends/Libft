#ifndef FT_QUEUE_HPP
#define FT_QUEUE_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "constructor.hpp"
#include "move.hpp"
#include <cstdint>
#include <new>
#include <utility>

template <typename ElementType>
class ft_queue
{
    private:
        struct QueueNode
        {
            ElementType _data;
            QueueNode*  _next;
        };

        QueueNode*                  _front;
        QueueNode*                  _rear;
        QueueNode*                  _spare_node;
        ft_size_t                      _size;
        mutable pt_recursive_mutex* _mutex;
        mutable uint8_t             _initialised_state;

        static thread_local int32_t _last_error;

        void destroy_all_unlocked();
        template <typename ValueType>
        void enqueue_impl(ValueType &&value, const char *operation);
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        class value_proxy
        {
            private:
                ft_queue<ElementType> * _parent_queue;
                ElementType *            _element_pointer;
                int32_t                  _last_error;
                int32_t                  _is_valid;

            public:
                value_proxy();
                value_proxy(ft_queue<ElementType> *parent_queue,
                    ElementType *element_pointer, int32_t error_code,
                    int32_t is_valid);
                ~value_proxy();

                ElementType *operator->();
                ElementType &operator*();
                operator ElementType() const;
                int32_t get_error() const;
                int32_t is_valid() const;
        };

        ft_queue();
        ft_queue(const ft_queue &other) = delete;
        ft_queue(ft_queue &&other) = delete;
        ~ft_queue();
        ft_queue& operator=(const ft_queue&) = delete;
        ft_queue& operator=(ft_queue&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_queue<ElementType> &other);

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        void enqueue(const ElementType& value);
        void enqueue(ElementType&& value);
        ElementType dequeue();

        ElementType& front();
        const ElementType& front() const;
        value_proxy front_proxy();

        ft_size_t size() const;
        ft_bool empty() const;

        void clear();

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

template <typename ElementType>
thread_local int32_t ft_queue<ElementType>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType>
int32_t ft_queue<ElementType>::set_error(int32_t error_code) noexcept
{
    ft_queue<ElementType>::_last_error = error_code;
    return (error_code);
}

template <typename ElementType>
ft_queue<ElementType>::value_proxy::value_proxy()
    : _parent_queue(ft_nullptr)
    , _element_pointer(ft_nullptr)
    , _last_error(FT_ERR_INVALID_STATE)
    , _is_valid(0)
{
    return ;
}

template <typename ElementType>
ft_queue<ElementType>::value_proxy::value_proxy(
    ft_queue<ElementType> *parent_queue,
    ElementType *element_pointer,
    int32_t error_code,
    int32_t is_valid)
    : _parent_queue(parent_queue)
    , _element_pointer(element_pointer)
    , _last_error(error_code)
    , _is_valid(is_valid)
{
    return ;
}

template <typename ElementType>
ft_queue<ElementType>::value_proxy::~value_proxy()
{
    return ;
}

template <typename ElementType>
ElementType *ft_queue<ElementType>::value_proxy::operator->()
{
    if (this->_is_valid == 0 || this->_element_pointer == ft_nullptr)
    {
        if (this->_parent_queue != ft_nullptr)
            this->_parent_queue->set_error(this->_last_error);
        return (ft_nullptr);
    }
    if (this->_parent_queue != ft_nullptr)
        this->_parent_queue->set_error(FT_ERR_SUCCESS);
    return (this->_element_pointer);
}

template <typename ElementType>
ElementType &ft_queue<ElementType>::value_proxy::operator*()
{
    if (this->_is_valid == 0 || this->_element_pointer == ft_nullptr)
    {
        static ElementType fallback_value = ElementType();

        if (this->_parent_queue != ft_nullptr)
            this->_parent_queue->set_error(this->_last_error);
        return (fallback_value);
    }
    if (this->_parent_queue != ft_nullptr)
        this->_parent_queue->set_error(FT_ERR_SUCCESS);
    return (*this->_element_pointer);
}

template <typename ElementType>
ft_queue<ElementType>::value_proxy::operator ElementType() const
{
    if (this->_is_valid == 0 || this->_element_pointer == ft_nullptr)
        return (ElementType());
    return (*this->_element_pointer);
}

template <typename ElementType>
int32_t ft_queue<ElementType>::value_proxy::get_error() const
{
    return (this->_last_error);
}

template <typename ElementType>
int32_t ft_queue<ElementType>::value_proxy::is_valid() const
{
    if (this->_parent_queue != ft_nullptr)
        this->_parent_queue->set_error(FT_ERR_SUCCESS);
    return (this->_is_valid);
}

template <typename ElementType>
ft_queue<ElementType>::ft_queue()
    : _front(ft_nullptr)
    , _rear(ft_nullptr)
    , _spare_node(ft_nullptr)
    , _size(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_queue<ElementType>::~ft_queue()
{
    uint32_t previous_error;

    previous_error = ft_queue<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)ft_queue<ElementType>::set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_queue<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_queue::initialize", "called while object is already initialised");
        return (ft_queue<ElementType>::set_error(FT_ERR_INVALID_STATE));
    }
    this->_front = ft_nullptr;
    this->_rear = ft_nullptr;
    this->_spare_node = ft_nullptr;
    this->_size = 0;
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_queue<ElementType>::destroy()
{
    int32_t first_error;
    int32_t mutex_destroy_error;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
    mutex_destroy_error = this->disable_thread_safety();
    if (mutex_destroy_error != FT_ERR_SUCCESS)
        first_error = mutex_destroy_error;
    this->destroy_all_unlocked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_queue<ElementType>::set_error(first_error));
}

template <typename ElementType>
int32_t ft_queue<ElementType>::move(ft_queue<ElementType> &other)
{
    int32_t destroy_result;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_queue::move",
            "source object is not initialised");
    }
    if (this == &other)
        return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (ft_queue<ElementType>::set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_queue<ElementType>::set_error(other._last_error));
    }
    this->_front = other._front;
    this->_rear = other._rear;
    this->_spare_node = other._spare_node;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._front = ft_nullptr;
    other._rear = ft_nullptr;
    other._spare_node = ft_nullptr;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_queue<ElementType>::set_error(other._last_error));
}

template <typename ElementType>
void ft_queue<ElementType>::destroy_all_unlocked()
{
    QueueNode *node;

    while (this->_front != ft_nullptr)
    {
        node = this->_front;
        this->_front = this->_front->_next;
        ::destroy_at(&node->_data);
        cma_free(node);
    }
    if (this->_spare_node != ft_nullptr)
    {
        cma_free(this->_spare_node);
        this->_spare_node = ft_nullptr;
    }
    this->_rear = ft_nullptr;
    this->_size = 0;
    return ;
}

template <typename ElementType>
int32_t ft_queue<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_queue<ElementType>::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (ft_queue<ElementType>::set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_queue<ElementType>::disable_thread_safety()
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (ft_queue<ElementType>::set_error(destroy_error));
}

template <typename ElementType>
ft_bool ft_queue<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::is_thread_safe");
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_queue<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::lock");
    lock_error = this->lock_internal(lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_queue<ElementType>::set_error(lock_error));
    return (ft_queue<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
void ft_queue<ElementType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::unlock");
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_queue<ElementType>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int32_t ft_queue<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (!lock_acquired || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_queue<ElementType>::enqueue(const ElementType& value)
{
    this->enqueue_impl(value, "ft_queue::enqueue(const ElementType&)");
    return ;
}

template <typename ElementType>
void ft_queue<ElementType>::enqueue(ElementType&& value)
{
    this->enqueue_impl(ft_move(value), "ft_queue::enqueue(ElementType&&)");
    return ;
}

template <typename ElementType>
template <typename ValueType>
void ft_queue<ElementType>::enqueue_impl(ValueType &&value, const char *operation)
{
    QueueNode *node;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, operation);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return ;
    }
    node = this->_spare_node;
    if (node != ft_nullptr)
        this->_spare_node = ft_nullptr;
    else
        node = static_cast<QueueNode *>(cma_malloc(sizeof(QueueNode)));
    if (node == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    ::construct_at(&node->_data, std::forward<ValueType>(value));
    node->_next = ft_nullptr;
    if (this->_rear == ft_nullptr)
        this->_front = node;
    else
        this->_rear->_next = node;
    this->_rear = node;
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ElementType ft_queue<ElementType>::dequeue()
{
    QueueNode   *node;
    ElementType result;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::dequeue");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (ElementType());
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
            ft_queue<ElementType>::set_error(FT_ERR_EMPTY);
        return (ElementType());
    }
    node = this->_front;
    result = ft_move(node->_data);
    ::destroy_at(&node->_data);
    this->_front = node->_next;
    if (this->_front == ft_nullptr)
        this->_rear = ft_nullptr;
    if (this->_spare_node == ft_nullptr)
        this->_spare_node = node;
    else
        cma_free(node);
    this->_size -= 1;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename ElementType>
ElementType& ft_queue<ElementType>::front()
{
    static ElementType error_element = ElementType();
    ElementType       *value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::front");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (error_element);
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
            ft_queue<ElementType>::set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value = &this->_front->_data;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (*value);
}

template <typename ElementType>
const ElementType& ft_queue<ElementType>::front() const
{
    static ElementType error_element = ElementType();
    const ElementType *value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::front const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (error_element);
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
            ft_queue<ElementType>::set_error(FT_ERR_EMPTY);
        return (error_element);
    }
    value = &this->_front->_data;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (*value);
}

template <typename ElementType>
typename ft_queue<ElementType>::value_proxy ft_queue<ElementType>::front_proxy()
{
    ElementType *value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::front_proxy");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (typename ft_queue<ElementType>::value_proxy(this,
            ft_nullptr, lock_error, 0));
    }
    if (this->_front == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        ft_queue<ElementType>::set_error(FT_ERR_EMPTY);
        return (typename ft_queue<ElementType>::value_proxy(this,
            ft_nullptr, FT_ERR_EMPTY, 0));
    }
    value = &this->_front->_data;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (typename ft_queue<ElementType>::value_proxy(this,
        value, FT_ERR_SUCCESS, 1));
}

template <typename ElementType>
ft_size_t ft_queue<ElementType>::size() const
{
    ft_size_t current_size;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (0);
    }
    current_size = this->_size;
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (current_size);
}

template <typename ElementType>
ft_bool ft_queue<ElementType>::empty() const
{
    ft_bool lock_acquired;
    ft_bool is_empty;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return (is_empty);
}

template <typename ElementType>
void ft_queue<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_queue::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_queue<ElementType>::set_error(lock_error);
        return ;
    }
    this->destroy_all_unlocked();
    (void)this->unlock_internal(lock_acquired);
    ft_queue<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_queue<ElementType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_queue::get_error");
    return (ft_queue<ElementType>::_last_error);
}

template <typename ElementType>
const char *ft_queue<ElementType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_queue::get_error_str");
    return (ft_strerror(this->get_error()));
}


#endif

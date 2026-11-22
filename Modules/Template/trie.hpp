#ifndef FT_TEMPLATE_TRIE_HPP
#define FT_TEMPLATE_TRIE_HPP

#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "constructor.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>

template <typename ValueType>
class ft_trie
{
    private:
        struct node_value
        {
            ft_size_t  _key_length;
            int32_t    _unset_value;
            ValueType  *_value_pointer;
        };

        struct child_edge
        {
            uint8_t _character;
            ft_trie *_child;
        };

        node_value                                  *_data;
        // Descendants are private and share the root operation's lock.
        child_edge                                  *_children;
        ft_size_t                                    _child_count;
        ft_size_t                                    _child_capacity;
        mutable pt_recursive_mutex                  *_mutex;
        uint8_t                                     _initialised_state;
        static thread_local int32_t                 _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        ft_size_t child_index(uint8_t character) const;
        int32_t insert_child(ft_size_t position, uint8_t character, ft_trie *child);
        void destroy_children_unlocked();
        int32_t insert_helper(const char *key, int32_t unset_value,
            ValueType *value_pointer);

    public:
        ft_trie();
        ft_trie(const ft_trie &other) = delete;
        ft_trie(ft_trie &&other) = delete;
        ~ft_trie();

        ft_trie &operator=(const ft_trie &other) = delete;
        ft_trie &operator=(ft_trie &&other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_trie<ValueType> &other);

        int32_t insert(const char *key, ValueType *value_pointer,
            int32_t unset_value = 0);
        const node_value *search(const char *key) const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ValueType>
int32_t ft_trie<ValueType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ValueType>
int32_t ft_trie<ValueType>::lock_internal(ft_bool *lock_acquired) const
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

template <typename ValueType>
int32_t ft_trie<ValueType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
ft_size_t ft_trie<ValueType>::child_index(uint8_t character) const
{
    ft_size_t first = 0U;
    ft_size_t last = this->_child_count;
    while (first < last)
    {
        const ft_size_t middle = first + (last - first) / 2U;
        if (this->_children[middle]._character < character)
            first = middle + 1U;
        else
            last = middle;
    }
    return (first);
}

template <typename ValueType>
int32_t ft_trie<ValueType>::insert_child(ft_size_t position, uint8_t character,
    ft_trie *child)
{
    if (this->_child_count == this->_child_capacity)
    {
        ft_size_t capacity = 1U;
        if (this->_child_capacity != 0U)
            capacity = this->_child_capacity * 2U;
        // The nonzero byte alphabet limits the capacity to 256 edges.
        child_edge *storage = static_cast<child_edge *>(cma_malloc(capacity * sizeof(child_edge)));
        if (storage == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (position != 0U)
            std::uninitialized_copy_n(this->_children, position, storage);
        ::construct_at(storage + position, child_edge{character, child});
        if (position < this->_child_count)
            std::uninitialized_copy_n(this->_children + position,
                this->_child_count - position, storage + position + 1U);
        if (this->_children != ft_nullptr)
            cma_free(this->_children);
        this->_children = storage;
        this->_child_capacity = capacity;
    }
    else if (position == this->_child_count)
        ::construct_at(this->_children + position, child_edge{character, child});
    else
    {
        child_edge *end = this->_children + this->_child_count;
        ::construct_at(end, *(end - 1U));
        std::copy_backward(this->_children + position, end - 1U, end);
        this->_children[position] = child_edge{character, child};
    }
    ++this->_child_count;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
void ft_trie<ValueType>::destroy_children_unlocked()
{
    ft_size_t index = 0U;
    while (index < this->_child_count)
    {
        delete this->_children[index]._child;
        ++index;
    }
    if (this->_children != ft_nullptr)
        cma_free(this->_children);
    this->_children = ft_nullptr;
    this->_child_count = 0U;
    this->_child_capacity = 0U;
    return ;
}

template <typename ValueType>
int32_t ft_trie<ValueType>::insert_helper(const char *key, int32_t unset_value,
    ValueType *value_pointer)
{
    if (key == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_trie *current_node = this;
    const char *key_iterator = key;
    while (*key_iterator != '\0')
    {
        const uint8_t character = static_cast<uint8_t>(*key_iterator);
        const ft_size_t position = current_node->child_index(character);
        if (position == current_node->_child_count
            || current_node->_children[position]._character != character)
        {
            ft_trie *new_child = new (std::nothrow) ft_trie();
            if (new_child == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            const int32_t initialize_error = new_child->initialize();
            if (initialize_error != FT_ERR_SUCCESS)
            {
                delete new_child;
                return (initialize_error);
            }
            const int32_t insert_error = current_node->insert_child(position, character, new_child);
            if (insert_error != FT_ERR_SUCCESS)
            {
                delete new_child;
                return (insert_error);
            }
        }
        current_node = current_node->_children[position]._child;
        ++key_iterator;
    }
    const node_value value{static_cast<ft_size_t>(key_iterator - key), unset_value, value_pointer};
    if (current_node->_data == ft_nullptr)
    {
        current_node->_data = static_cast<node_value *>(cma_malloc(sizeof(node_value)));
        if (current_node->_data == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ::construct_at(current_node->_data, value);
    }
    else
        *current_node->_data = value;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
ft_trie<ValueType>::ft_trie()
    : _data(ft_nullptr), _children(ft_nullptr), _child_count(0U),
      _child_capacity(0U), _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ValueType>
ft_trie<ValueType>::~ft_trie()
{
    uint32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ValueType>
int32_t ft_trie<ValueType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_trie::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ValueType>
int32_t ft_trie<ValueType>::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t first_error;
    int32_t disable_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS)
        first_error = disable_result;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = lock_error;
    this->destroy_children_unlocked();
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    (void)this->unlock_internal(lock_acquired);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ValueType>
int32_t ft_trie<ValueType>::move(ft_trie<ValueType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_trie::move",
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
        this->_data = ft_nullptr;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_data = other._data;
    this->_children = other._children;
    this->_child_count = other._child_count;
    this->_child_capacity = other._child_capacity;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._children = ft_nullptr;
    other._child_count = 0U;
    other._child_capacity = 0U;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ValueType>
int32_t ft_trie<ValueType>::insert(const char *key, ValueType *value_pointer,
    int32_t unset_value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t insert_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_trie::insert");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    insert_result = this->insert_helper(key, unset_value, value_pointer);
    (void)this->unlock_internal(lock_acquired);
    return (set_error(insert_result));
}

template <typename ValueType>
const typename ft_trie<ValueType>::node_value *ft_trie<ValueType>::search(
    const char *key) const
{
    const ft_trie<ValueType> *current_node;
    const char *key_iterator;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_trie::search");
    if (key == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ft_nullptr);
    }
    current_node = this;
    key_iterator = key;
    while (*key_iterator != '\0')
    {
        const uint8_t character = static_cast<uint8_t>(*key_iterator);
        const ft_size_t position = current_node->child_index(character);
        if (position == current_node->_child_count
            || current_node->_children[position]._character != character)
        {
            (void)this->unlock_internal(lock_acquired);
            set_error(FT_ERR_NOT_FOUND);
            return (ft_nullptr);
        }
        current_node = current_node->_children[position]._child;
        ++key_iterator;
    }
    const node_value *result = current_node->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename ValueType>
int32_t ft_trie<ValueType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_trie::enable_thread_safety");
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

template <typename ValueType>
int32_t ft_trie<ValueType>::disable_thread_safety()
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

template <typename ValueType>
ft_bool ft_trie<ValueType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_trie::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ValueType>
int32_t ft_trie<ValueType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_trie::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename ValueType>
void ft_trie<ValueType>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ValueType>
int32_t ft_trie<ValueType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_trie::get_error");
    return (_last_error);
}

template <typename ValueType>
const char *ft_trie<ValueType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_trie::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename ValueType>
thread_local int32_t ft_trie<ValueType>::_last_error = FT_ERR_SUCCESS;

#endif

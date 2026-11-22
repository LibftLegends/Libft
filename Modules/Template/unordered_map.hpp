#ifndef FT_UNORDERED_MAP_HPP
#define FT_UNORDERED_MAP_HPP

#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "constructor.hpp"
#include "move.hpp"
#include <cstdint>
#include <functional>
#include <new>

template <typename Key, typename MappedType>
struct ft_pair
{
    Key         first;
    MappedType  second;
    ft_pair();
    ft_pair(const Key& key, const MappedType& value);
};

template <typename Key, typename MappedType>
class ft_unordered_map
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_pair<Key, MappedType>      *_data;
        ft_bool                          *_occupied;
        ft_size_t                        _capacity;
        ft_size_t                        _size;
        ft_size_t                        _requested_capacity;
        mutable pt_recursive_mutex    *_mutex;
        mutable uint8_t               _initialised_state;

        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;

        ft_bool    has_storage_unlocked() const;
        void    destroy_elements_unlocked();
        void    release_storage_unlocked();
        int32_t prepare_empty_storage_unlocked(ft_size_t capacity);

        ft_size_t  probe_slot_unlocked(const Key &key, ft_size_t start_index) const;
        ft_size_t  find_index_unlocked(const Key& key) const;
        int32_t insert_internal_unlocked(const Key& key, const MappedType& value);
        int32_t insert_new_unlocked(const Key &key, const MappedType &value,
            ft_size_t hash, ft_size_t &index);
        int32_t resize_unlocked(ft_size_t new_capacity);

        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        class mapped_proxy
        {
            private:
                ft_unordered_map<Key, MappedType> * _parent_map;
                MappedType *                        _mapped_value_pointer;
                uint32_t                            _last_error;
                int32_t                             _is_valid;
                int32_t set_error(int32_t error_code);

            public:
                mapped_proxy();
                mapped_proxy(ft_unordered_map<Key, MappedType> *parent_map,
                    MappedType *mapped_value_pointer, int32_t error_code,
                    int32_t is_valid);
                ~mapped_proxy();

                MappedType *operator->();
                MappedType &operator*();
                mapped_proxy &operator=(const MappedType& mapped_value);
                operator MappedType() const;
                int32_t get_error() const;
                int32_t is_valid() const;
        };

        class iterator
        {
            private:
                ft_pair<Key, MappedType>      *_data;
                ft_bool                          *_occupied;
                ft_size_t                        _index;
                ft_size_t                        _capacity;
                uint8_t                       _initialised_state;

                static thread_local int32_t _last_error;

                static int32_t set_error(int32_t error_code) noexcept;

                void    advance_to_valid_index_unlocked();

            public:
                iterator();
                iterator(ft_pair<Key, MappedType>* data, ft_bool* occupied,
                    ft_size_t index, ft_size_t capacity);
                iterator(const iterator& other) = delete;
                iterator(iterator&& other) = delete;
                iterator& operator=(const iterator& other) = delete;
                iterator& operator=(iterator&& other) = delete;
                ~iterator();

                int32_t initialize(ft_pair<Key, MappedType>* data, ft_bool* occupied,
                    ft_size_t index, ft_size_t capacity);
                int32_t initialize(const iterator& other);
                int32_t initialize(iterator&& other);
                int32_t destroy();
                int32_t move(iterator& other);
                int32_t get_error() const;

                ft_pair<Key, MappedType>& operator*() const;
                ft_pair<Key, MappedType>* operator->() const;
                iterator& operator++();
                ft_bool operator==(const iterator& other) const;
                ft_bool operator!=(const iterator& other) const;
        };

        class const_iterator
        {
            private:
                const ft_pair<Key, MappedType>    *_data;
                const ft_bool                        *_occupied;
                ft_size_t                            _index;
                ft_size_t                            _capacity;
                uint8_t                           _initialised_state;

                static thread_local int32_t _last_error;

                static int32_t set_error(int32_t error_code) noexcept;

                void    advance_to_valid_index_unlocked();

            public:
                const_iterator();
                const_iterator(const ft_pair<Key, MappedType>* data,
                    const ft_bool* occupied, ft_size_t index, ft_size_t capacity);
                const_iterator(const const_iterator& other) = delete;
                const_iterator(const_iterator&& other) = delete;
                const_iterator& operator=(const const_iterator& other) = delete;
                const_iterator& operator=(const_iterator&& other) = delete;
                ~const_iterator();

                int32_t initialize(const ft_pair<Key, MappedType>* data,
                    const ft_bool* occupied, ft_size_t index, ft_size_t capacity);
                int32_t initialize(const const_iterator& other);
                int32_t initialize(const_iterator&& other);
                int32_t destroy();
                int32_t move(const_iterator& other);
                int32_t get_error() const;

                const ft_pair<Key, MappedType>& operator*() const;
                const ft_pair<Key, MappedType>* operator->() const;
                const_iterator& operator++();
                ft_bool operator==(const const_iterator& other) const;
                ft_bool operator!=(const const_iterator& other) const;
        };

        ft_unordered_map(ft_size_t initial_capacity = 10);
        ft_unordered_map(const ft_unordered_map& other) = delete;
        ft_unordered_map& operator=(const ft_unordered_map& other);
        ft_unordered_map(ft_unordered_map&& other) noexcept = delete;
        ft_unordered_map& operator=(ft_unordered_map&& other) noexcept;
        ~ft_unordered_map();

        int32_t     initialize();
        int32_t     initialize(const ft_unordered_map& other) = delete;
        int32_t     initialize(ft_unordered_map&& other) = delete;
        int32_t     destroy();
        int32_t move(ft_unordered_map& other) = delete;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool    is_thread_safe() const;
        uint8_t is_initialised() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const;
        void    unlock(ft_bool lock_acquired) const;

        void            insert(const Key& key, const MappedType& value);
        iterator        find(const Key& key);
        const_iterator  find(const Key& key) const;
        void            erase(const Key& key);
        ft_bool            empty() const;
        void            clear();
        ft_size_t          size() const;
        ft_size_t          bucket_count() const;
        ft_bool            has_valid_storage() const;

        int32_t get_error() const noexcept;
        const char   *get_error_str() const noexcept;

        iterator        begin();
        iterator        end();
        const_iterator  begin() const;
        const_iterator  end() const;

        MappedType          &at(const Key& key);
        const MappedType    &at(const Key& key) const;
        mapped_proxy        operator[](const Key& key);

};

template <typename Key, typename MappedType>
ft_pair<Key, MappedType>::ft_pair()
    : first(), second()
{
    return ;
}

template <typename Key, typename MappedType>
ft_pair<Key, MappedType>::ft_pair(const Key& key, const MappedType& value)
    : first(key), second(value)
{
    return ;
}

template <typename Key, typename MappedType>
thread_local int32_t ft_unordered_map<Key, MappedType>::_last_error = FT_ERR_SUCCESS;

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::set_error(int32_t error_code) noexcept
{
    ft_unordered_map<Key, MappedType>::_last_error = error_code;
    return (error_code);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::has_storage_unlocked() const
{
    if (this->_data == ft_nullptr)
        return (FT_FALSE);
    if (this->_occupied == ft_nullptr)
        return (FT_FALSE);
    if (this->_capacity == 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::destroy_elements_unlocked()
{
    ft_size_t index;
    ft_size_t remaining_elements;

    if (!this->has_storage_unlocked())
        return ;
    index = 0;
    remaining_elements = this->_size;
    while (index < this->_capacity && remaining_elements > 0)
    {
        if (this->_occupied[index])
        {
            ::destroy_at(&this->_data[index]);
            this->_occupied[index] = FT_FALSE;
            remaining_elements -= 1;
        }
        index += 1;
    }
    this->_size = 0;
    return ;
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::release_storage_unlocked()
{
    if (this->_data != ft_nullptr)
        cma_free(this->_data);
    if (this->_occupied != ft_nullptr)
        cma_free(this->_occupied);
    this->_data = ft_nullptr;
    this->_occupied = ft_nullptr;
    this->_capacity = 0;
    this->_size = 0;
    return ;
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::prepare_empty_storage_unlocked(ft_size_t capacity)
{
    void    *raw_data;
    void    *raw_occupied;
    ft_size_t  index;

    if (capacity == 0)
        capacity = 1;
    if (capacity > FT_SYSTEM_SIZE_MAX / sizeof(ft_pair<Key, MappedType>))
        return (FT_ERR_NO_MEMORY);
    raw_data = cma_malloc(sizeof(ft_pair<Key, MappedType>) * capacity);
    if (raw_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    raw_occupied = cma_malloc(sizeof(ft_bool) * capacity);
    if (raw_occupied == ft_nullptr)
    {
        cma_free(raw_data);
        return (FT_ERR_NO_MEMORY);
    }
    this->_data = static_cast<ft_pair<Key, MappedType>*>(raw_data);
    this->_occupied = static_cast<ft_bool*>(raw_occupied);
    this->_capacity = capacity;
    this->_size = 0;
    index = 0;
    while (index < this->_capacity)
    {
        this->_occupied[index] = FT_FALSE;
        index += 1;
    }
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
ft_size_t ft_unordered_map<Key, MappedType>::probe_slot_unlocked(
    const Key &key, ft_size_t start_index) const
{
    ft_size_t index = start_index;
    while (this->_occupied[index] && !(this->_data[index].first == key))
    {
        if (++index == this->_capacity)
            index = 0U;
        if (index == start_index)
            return (this->_capacity);
    }
    return (index);
}

template <typename Key, typename MappedType>
ft_size_t ft_unordered_map<Key, MappedType>::find_index_unlocked(const Key& key) const
{
    if (!this->has_storage_unlocked() || this->_size == 0U)
        return (this->_capacity);
    const ft_size_t hash = std::hash<Key>{}(key);
    const ft_size_t index = this->probe_slot_unlocked(key, hash % this->_capacity);
    if (index != this->_capacity && this->_occupied[index])
        return (index);
    return (this->_capacity);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::resize_unlocked(ft_size_t new_capacity)
{
    ft_pair<Key, MappedType>  *old_data;
    ft_bool                      *old_occupied;
    ft_size_t                    old_capacity;
    ft_size_t                    old_size;
    void                      *raw_data;
    void                      *raw_occupied;
    ft_pair<Key, MappedType>  *new_data;
    ft_bool                      *new_occupied;
    ft_size_t                    old_index;
    ft_size_t                    inserted_count;

    if (new_capacity == 0 || new_capacity > FT_SYSTEM_SIZE_MAX / sizeof(ft_pair<Key, MappedType>))
        return (FT_ERR_NO_MEMORY);
    raw_data = cma_malloc(sizeof(ft_pair<Key, MappedType>) * new_capacity);
    if (raw_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    raw_occupied = cma_malloc(sizeof(ft_bool) * new_capacity);
    if (raw_occupied == ft_nullptr)
    {
        cma_free(raw_data);
        return (FT_ERR_NO_MEMORY);
    }
    new_data = static_cast<ft_pair<Key, MappedType>*>(raw_data);
    new_occupied = static_cast<ft_bool*>(raw_occupied);
    old_index = 0;
    while (old_index < new_capacity)
    {
        new_occupied[old_index] = FT_FALSE;
        old_index += 1;
    }

    old_data = this->_data;
    old_occupied = this->_occupied;
    old_capacity = this->_capacity;
    old_size = this->_size;

    inserted_count = 0;
    old_index = 0;
    while (old_index < old_capacity && inserted_count < old_size)
    {
        if (old_occupied[old_index])
        {
            ft_size_t destination_index;
            std::hash<Key> hasher;

            destination_index = hasher(old_data[old_index].first) % new_capacity;
            while (new_occupied[destination_index])
            {
                if (++destination_index == new_capacity)
                    destination_index = 0U;
            }
            ::construct_at(&new_data[destination_index], ft_move(old_data[old_index]));
            new_occupied[destination_index] = FT_TRUE;
            ::destroy_at(&old_data[old_index]);
            inserted_count += 1;
        }
        old_index += 1;
    }

    cma_free(old_data);
    cma_free(old_occupied);

    this->_data = new_data;
    this->_occupied = new_occupied;
    this->_capacity = new_capacity;
    this->_size = inserted_count;
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::insert_internal_unlocked(
    const Key& key, const MappedType& value)
{
    if (!this->has_storage_unlocked())
        return (FT_ERR_INVALID_STATE);
    const ft_size_t hash = std::hash<Key>{}(key);
    ft_size_t index = this->probe_slot_unlocked(key, hash % this->_capacity);
    if (index != this->_capacity && this->_occupied[index])
    {
        this->_data[index].second = value;
        return (FT_ERR_SUCCESS);
    }
    return (this->insert_new_unlocked(key, value, hash, index));
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::insert_new_unlocked(const Key &key,
    const MappedType &value, ft_size_t hash, ft_size_t &index)
{
    if (this->_size >= this->_capacity / 2U + this->_capacity % 2U)
    {
        if (this->_capacity > FT_SYSTEM_SIZE_MAX / 2U)
            return (FT_ERR_NO_MEMORY);
        // The value may refer to an entry in the old storage.
        ft_pair<Key, MappedType> entry(key, value);
        const int32_t resize_error = this->resize_unlocked(this->_capacity * 2U);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
        index = this->probe_slot_unlocked(entry.first, hash % this->_capacity);
        if (index == this->_capacity)
            return (FT_ERR_INTERNAL);
        ::construct_at(&this->_data[index], ft_move(entry));
    }
    else
    {
        if (index == this->_capacity)
            return (FT_ERR_INTERNAL);
        ::construct_at(&this->_data[index], key, value);
    }
    this->_occupied[index] = FT_TRUE;
    ++this->_size;
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::lock_internal(ft_bool *lock_acquired) const
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::unlock_internal(ft_bool lock_acquired) const
{
    if (!lock_acquired)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::ft_unordered_map(ft_size_t initial_capacity)
    : _data(ft_nullptr)
    , _occupied(ft_nullptr)
    , _capacity(0)
    , _size(0)
    , _requested_capacity(initial_capacity)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::~ft_unordered_map()
{
    uint32_t previous_error;

    previous_error = ft_unordered_map<Key, MappedType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)ft_unordered_map<Key, MappedType>::set_error(previous_error);
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>& ft_unordered_map<Key, MappedType>::operator=(
    const ft_unordered_map<Key, MappedType>& other)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t index;

    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_unordered_map::operator=(const)", "source object is not initialised");
        return (*this);
    }
    if (this == &other)
        return (*this);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_requested_capacity = other._capacity;
        if (this->initialize() != FT_ERR_SUCCESS)
            return (*this);
    }
    this->clear();
    lock_acquired = FT_FALSE;
    lock_error = other.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (*this);
    }
    index = 0;
    while (index < other._capacity)
    {
        if (other._occupied[index])
            this->insert(other._data[index].first, other._data[index].second);
        index += 1;
    }
    (void)other.unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>& ft_unordered_map<Key, MappedType>::operator=(
    ft_unordered_map<Key, MappedType>&& other) noexcept
{
    if (this == &other)
        return (*this);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_unordered_map::operator=(move)",
            "source object is uninitialised");
        return (*this);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_data = ft_nullptr;
        this->_occupied = ft_nullptr;
        this->_capacity = 0;
        this->_size = 0;
        this->_requested_capacity = other._requested_capacity;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_capacity = other._capacity;
    this->_size = other._size;
    this->_requested_capacity = other._requested_capacity;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._occupied = ft_nullptr;
    other._capacity = 0;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::initialize()
{
    ft_size_t initial_capacity;
    int32_t prepare_error;
    int32_t mutex_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }

    this->destroy_elements_unlocked();
    this->release_storage_unlocked();

    initial_capacity = this->_requested_capacity;
    if (initial_capacity == 0)
        initial_capacity = 1;

    prepare_error = this->prepare_empty_storage_unlocked(initial_capacity);
    if (prepare_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_unordered_map<Key, MappedType>::set_error(prepare_error);
        return (prepare_error);
    }

    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    mutex_error = this->enable_thread_safety();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->destroy_elements_unlocked();
        this->release_storage_unlocked();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_unordered_map<Key, MappedType>::set_error(mutex_error);
        return (mutex_error);
    }

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t mutex_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
                return (FT_ERR_SUCCESS);

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (lock_error);
    }

    this->destroy_elements_unlocked();
    this->release_storage_unlocked();
    (void)this->unlock_internal(lock_acquired);

    mutex_destroy_error = FT_ERR_SUCCESS;
    if (this->_mutex != ft_nullptr)
    {
        mutex_destroy_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
    }

    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_unordered_map<Key, MappedType>::set_error(mutex_destroy_error);
    return (mutex_destroy_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::enable_thread_safety()
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::enable_thread_safety");

    if (this->_mutex != ft_nullptr)
    {
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }

    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }

    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        ft_unordered_map<Key, MappedType>::set_error(initialize_error);
        return (initialize_error);
    }

    this->_mutex = mutex_pointer;
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::disable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::disable_thread_safety");

    if (this->_mutex == ft_nullptr)
    {
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }

    int32_t destroy_error;

    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    ft_unordered_map<Key, MappedType>::set_error(destroy_error);
    return (destroy_error);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::is_thread_safe");
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename Key, typename MappedType>
uint8_t ft_unordered_map<Key, MappedType>::is_initialised() const noexcept
{
    return (this->_initialised_state);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::lock");

    int32_t lock_error;

    lock_error = this->lock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(lock_error);
    return (lock_error);
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::unlock");
    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::insert(const Key& key, const MappedType& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t insert_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::insert");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return ;
    }

    insert_error = this->insert_internal_unlocked(key, value);
    (void)this->unlock_internal(lock_acquired);

    if (insert_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(insert_error);
        return ;
    }
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::iterator
ft_unordered_map<Key, MappedType>::find(const Key& key)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t  index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::find");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (iterator(this->_data, this->_occupied, this->_capacity,
            this->_capacity));
    }

    index = this->find_index_unlocked(key);

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    if (index == this->_capacity)
        return (iterator(this->_data, this->_occupied, this->_capacity,
            this->_capacity));
    return (iterator(this->_data, this->_occupied, index, this->_capacity));
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::const_iterator
ft_unordered_map<Key, MappedType>::find(const Key& key) const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t  index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::find const");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (const_iterator(this->_data, this->_occupied, this->_capacity,
            this->_capacity));
    }

    index = this->find_index_unlocked(key);

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    if (index == this->_capacity)
        return (const_iterator(this->_data, this->_occupied, this->_capacity,
            this->_capacity));
    return (const_iterator(this->_data, this->_occupied, index, this->_capacity));
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::erase(const Key& key)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t  index;
    ft_size_t  next_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::erase");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return ;
    }

    index = this->find_index_unlocked(key);
    if (index == this->_capacity)
    {
        (void)this->unlock_internal(lock_acquired);
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
        return ;
    }

    ::destroy_at(&this->_data[index]);
    this->_occupied[index] = FT_FALSE;
    if (this->_size > 0)
        this->_size -= 1;

    const ft_size_t capacity = this->_capacity;
    next_index = index;
    const auto probe_distance = [capacity](ft_size_t home, ft_size_t slot)
    {
        if (slot >= home)
            return (slot - home);
        return (capacity - home + slot);
    };
    while (FT_TRUE)
    {
        if (++next_index == capacity)
            next_index = 0U;
        if (!this->_occupied[next_index])
            break ;
        const ft_size_t home = std::hash<Key>{}(this->_data[next_index].first) % capacity;
        // Move an entry only if it would probe through the hole when looked up.
        if (probe_distance(home, index) < probe_distance(home, next_index))
        {
            ::construct_at(&this->_data[index], ft_move(this->_data[next_index]));
            ::destroy_at(&this->_data[next_index]);
            this->_occupied[index] = FT_TRUE;
            this->_occupied[next_index] = FT_FALSE;
            index = next_index;
        }
    }

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::empty() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool    is_empty;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::empty");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (FT_TRUE);
    }

    is_empty = (this->_size == 0);

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (is_empty);
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::clear");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return ;
    }

    this->destroy_elements_unlocked();

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
ft_size_t ft_unordered_map<Key, MappedType>::size() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t  current_size;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::size");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (0);
    }

    current_size = this->_size;

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (current_size);
}

template <typename Key, typename MappedType>
ft_size_t ft_unordered_map<Key, MappedType>::bucket_count() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t  bucket_total;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::bucket_count");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (0);
    }

    bucket_total = this->_capacity;

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (bucket_total);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::has_valid_storage() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool    valid_storage;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::has_valid_storage");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (FT_FALSE);
    }

    valid_storage = this->has_storage_unlocked();

    (void)this->unlock_internal(lock_acquired);
    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (valid_storage);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_unordered_map::get_error");
    return (ft_unordered_map<Key, MappedType>::_last_error);
}

template <typename Key, typename MappedType>
const char *ft_unordered_map<Key, MappedType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_unordered_map::get_error_str");
    return (ft_strerror(this->get_error()));
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::iterator ft_unordered_map<Key, MappedType>::begin()
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::begin");

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (iterator(this->_data, this->_occupied, 0, this->_capacity));
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::iterator ft_unordered_map<Key, MappedType>::end()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::end");

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (iterator(this->_data, this->_occupied, this->_capacity,
        this->_capacity));
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::const_iterator
ft_unordered_map<Key, MappedType>::begin() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::begin const");

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (const_iterator(this->_data, this->_occupied, 0, this->_capacity));
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::const_iterator
ft_unordered_map<Key, MappedType>::end() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::end const");

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (const_iterator(this->_data, this->_occupied, this->_capacity,
        this->_capacity));
}

template <typename Key, typename MappedType>
MappedType& ft_unordered_map<Key, MappedType>::at(const Key& key)
{
    static MappedType  error_value = MappedType();
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t              key_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::at");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (error_value);
    }

    key_index = this->find_index_unlocked(key);
    if (key_index == this->_capacity)
    {
        (void)this->unlock_internal(lock_acquired);
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_NOT_FOUND);
        return (error_value);
    }

    (void)this->unlock_internal(lock_acquired);

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (this->_data[key_index].second);
}

template <typename Key, typename MappedType>
const MappedType& ft_unordered_map<Key, MappedType>::at(const Key& key) const
{
    static MappedType  error_value = MappedType();
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t              key_index;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::at const");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (error_value);
    }

    key_index = this->find_index_unlocked(key);
    if (key_index == this->_capacity)
    {
        (void)this->unlock_internal(lock_acquired);
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_NOT_FOUND);
        return (error_value);
    }

    (void)this->unlock_internal(lock_acquired);

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (this->_data[key_index].second);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::mapped_proxy::mapped_proxy()
    : _parent_map(ft_nullptr)
    , _mapped_value_pointer(ft_nullptr)
    , _last_error(FT_ERR_INVALID_STATE)
    , _is_valid(0)
{
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::mapped_proxy::mapped_proxy(
    ft_unordered_map<Key, MappedType> *parent_map,
    MappedType *mapped_value_pointer, int32_t error_code, int32_t is_valid)
    : _parent_map(parent_map)
    , _mapped_value_pointer(mapped_value_pointer)
    , _last_error(error_code)
    , _is_valid(is_valid)
{
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::mapped_proxy::~mapped_proxy()
{
    return ;
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::mapped_proxy::set_error(
        int32_t error_code)
{
    this->_last_error = error_code;
    return (error_code);
}

template <typename Key, typename MappedType>
MappedType *ft_unordered_map<Key, MappedType>::mapped_proxy::operator->()
{
    static MappedType error_value = MappedType();

    if (!this->_is_valid || this->_mapped_value_pointer == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        if (this->_parent_map != ft_nullptr)
            ft_unordered_map<Key, MappedType>::set_error(
                this->_last_error);
        return (&error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    if (this->_parent_map != ft_nullptr)
        ft_unordered_map<Key, MappedType>::set_error(
            this->_last_error);
    return (this->_mapped_value_pointer);
}

template <typename Key, typename MappedType>
MappedType &ft_unordered_map<Key, MappedType>::mapped_proxy::operator*()
{
    static MappedType error_value = MappedType();

    if (!this->_is_valid || this->_mapped_value_pointer == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        if (this->_parent_map != ft_nullptr)
            ft_unordered_map<Key, MappedType>::set_error(
                this->_last_error);
        return (error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    if (this->_parent_map != ft_nullptr)
        ft_unordered_map<Key, MappedType>::set_error(
            this->_last_error);
    return (*this->_mapped_value_pointer);
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::mapped_proxy &
ft_unordered_map<Key, MappedType>::mapped_proxy::operator=(
    const MappedType& mapped_value)
{
    if (!this->_is_valid || this->_mapped_value_pointer == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        if (this->_parent_map != ft_nullptr)
            ft_unordered_map<Key, MappedType>::set_error(
                this->_last_error);
        return (*this);
    }
    *this->_mapped_value_pointer = mapped_value;
    (void)set_error(FT_ERR_SUCCESS);
    if (this->_parent_map != ft_nullptr)
        ft_unordered_map<Key, MappedType>::set_error(
            this->_last_error);
    return (*this);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::mapped_proxy::operator MappedType() const
{
    if (!this->_is_valid || this->_mapped_value_pointer == ft_nullptr)
        return (MappedType());
    return (*this->_mapped_value_pointer);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::mapped_proxy::get_error() const
{
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::mapped_proxy::is_valid() const
{
    if (this->_parent_map != ft_nullptr)
        ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (this->_is_valid);
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::mapped_proxy
ft_unordered_map<Key, MappedType>::operator[](const Key& key)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::operator[]");

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_unordered_map<Key, MappedType>::set_error(lock_error);
        return (mapped_proxy(this, ft_nullptr, lock_error, 0));
    }

    if (!this->has_storage_unlocked())
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return (mapped_proxy(this, ft_nullptr, FT_ERR_INVALID_STATE, 0));
    }
    const ft_size_t hash = std::hash<Key>{}(key);
    ft_size_t key_index = this->probe_slot_unlocked(key, hash % this->_capacity);
    if (key_index == this->_capacity || !this->_occupied[key_index])
    {
        const int32_t insert_error = this->insert_new_unlocked(key, MappedType(),
            hash, key_index);
        if (insert_error != FT_ERR_SUCCESS)
        {
            (void)this->unlock_internal(lock_acquired);
            ft_unordered_map<Key, MappedType>::set_error(insert_error);
            return (mapped_proxy(this, ft_nullptr, insert_error, 0));
        }
    }

    (void)this->unlock_internal(lock_acquired);

    ft_unordered_map<Key, MappedType>::set_error(FT_ERR_SUCCESS);
    return (mapped_proxy(this, &this->_data[key_index].second, FT_ERR_SUCCESS,
        1));
}

// Iterator

template <typename Key, typename MappedType>
thread_local int32_t ft_unordered_map<Key, MappedType>::iterator::_last_error = FT_ERR_SUCCESS;

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::set_error(
        int32_t error_code) noexcept
{
    ft_unordered_map<Key, MappedType>::iterator::_last_error = error_code;
    return (error_code);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::iterator::iterator()
    : _data(ft_nullptr)
    , _occupied(ft_nullptr)
    , _index(0)
    , _capacity(0)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::iterator::iterator(
    ft_pair<Key, MappedType>* data, ft_bool* occupied, ft_size_t index, ft_size_t capacity)
    : _data(ft_nullptr)
    , _occupied(ft_nullptr)
    , _index(0)
    , _capacity(0)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    (void)this->initialize(data, occupied, index, capacity);
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::iterator::~iterator()
{
    uint32_t previous_error;

    previous_error = this->_last_error;
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)set_error(previous_error);
    return ;
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::initialize(
    ft_pair<Key, MappedType>* data, ft_bool* occupied, ft_size_t index, ft_size_t capacity)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::iterator::initialize", "called while already initialised");
    this->_data = data;
    this->_occupied = occupied;
    this->_index = index;
    this->_capacity = capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::initialize(const iterator& other)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::iterator::initialize copy", "called while already initialised");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    (void)set_error(other._last_error);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::initialize(iterator&& other)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::iterator::initialize move", "called while already initialised");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    other._data = ft_nullptr;
    other._occupied = ft_nullptr;
    other._index = 0;
    other._capacity = 0;
    other._initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::destroy()
{
    if (this->_initialised_state
        != FT_CLASS_STATE_INITIALISED)
    {
        (void)set_error(FT_ERR_SUCCESS);
        return (this->_last_error);
    }
    this->_data = ft_nullptr;
    this->_occupied = ft_nullptr;
    this->_index = 0;
    this->_capacity = 0;
    this->_initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::move(iterator& other)
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::iterator::move");
    errno_abort_if_uninitialised(other._initialised_state, "ft_unordered_map::iterator::move");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->advance_to_valid_index_unlocked();
    other._data = ft_nullptr;
    other._occupied = ft_nullptr;
    other._index = 0;
    other._capacity = 0;
    other._initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::iterator::get_error() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::iterator::get_error");
    return (this->_last_error);
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::iterator::advance_to_valid_index_unlocked()
{
    if (this->_occupied == ft_nullptr)
        return ;
    while (this->_index < this->_capacity)
    {
        if (this->_occupied[this->_index])
            return ;
        this->_index += 1;
    }
    return ;
}

template <typename Key, typename MappedType>
ft_pair<Key, MappedType>& ft_unordered_map<Key, MappedType>::iterator::operator*() const
{
    static ft_pair<Key, MappedType> error_value;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::iterator::operator*");
    if (this->_data == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (error_value);
    }
    if (this->_occupied == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (error_value);
    }
    if (this->_index >= this->_capacity)
    {
        (void)set_error(FT_ERR_OUT_OF_RANGE);
        return (error_value);
    }
    if (!this->_occupied[this->_index])
    {
        (void)set_error(FT_ERR_NOT_FOUND);
        return (error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_data[this->_index]);
}

template <typename Key, typename MappedType>
ft_pair<Key, MappedType>* ft_unordered_map<Key, MappedType>::iterator::operator->() const
{
    static ft_pair<Key, MappedType> error_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::iterator::operator->");
    if (this->_data == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (&error_value);
    }
    if (this->_occupied == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (&error_value);
    }
    if (this->_index >= this->_capacity)
    {
        (void)set_error(FT_ERR_OUT_OF_RANGE);
        return (&error_value);
    }
    if (!this->_occupied[this->_index])
    {
        (void)set_error(FT_ERR_NOT_FOUND);
        return (&error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (&this->_data[this->_index]);
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::iterator&
ft_unordered_map<Key, MappedType>::iterator::operator++()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::iterator::operator++");
    if (this->_index < this->_capacity)
        this->_index += 1;
    this->advance_to_valid_index_unlocked();
    (void)set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::iterator::operator==(const iterator& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::iterator::operator==");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_unordered_map::iterator::operator==");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_data == other._data
        && this->_occupied == other._occupied
        && this->_index == other._index
        && this->_capacity == other._capacity);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::iterator::operator!=(const iterator& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::iterator::operator!=");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_unordered_map::iterator::operator!=");
    (void)set_error(FT_ERR_SUCCESS);
    return (!(*this == other));
}

template <typename Key, typename MappedType>
thread_local int32_t ft_unordered_map<Key, MappedType>::const_iterator::_last_error
    = FT_ERR_SUCCESS;

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::set_error(
        int32_t error_code) noexcept
{
    ft_unordered_map<Key, MappedType>::const_iterator::_last_error = error_code;
    return (error_code);
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::const_iterator::const_iterator()
    : _data(ft_nullptr)
    , _occupied(ft_nullptr)
    , _index(0)
    , _capacity(0)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::const_iterator::const_iterator(
    const ft_pair<Key, MappedType>* data, const ft_bool* occupied,
    ft_size_t index, ft_size_t capacity)
    : _data(ft_nullptr)
    , _occupied(ft_nullptr)
    , _index(0)
    , _capacity(0)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    (void)this->initialize(data, occupied, index, capacity);
    return ;
}

template <typename Key, typename MappedType>
ft_unordered_map<Key, MappedType>::const_iterator::~const_iterator()
{
    uint32_t previous_error;

    previous_error = this->_last_error;
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)set_error(previous_error);
    return ;
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::initialize(
    const ft_pair<Key, MappedType>* data, const ft_bool* occupied,
    ft_size_t index, ft_size_t capacity)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::const_iterator::initialize", "called while already initialised");
    this->_data = data;
    this->_occupied = occupied;
    this->_index = index;
    this->_capacity = capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::initialize(
    const const_iterator& other)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::const_iterator::initialize copy", "called while already initialised");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    (void)set_error(other._last_error);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::initialize(
    const_iterator&& other)
{
    if (this->_initialised_state
        == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_unordered_map::const_iterator::initialize move", "called while already initialised");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->_initialised_state
        = FT_CLASS_STATE_INITIALISED;
    this->advance_to_valid_index_unlocked();
    other._data = ft_nullptr;
    other._occupied = ft_nullptr;
    other._index = 0;
    other._capacity = 0;
    other._initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::destroy()
{
    if (this->_initialised_state
        != FT_CLASS_STATE_INITIALISED)
    {
        (void)set_error(FT_ERR_SUCCESS);
        return (this->_last_error);
    }
    this->_data = ft_nullptr;
    this->_occupied = ft_nullptr;
    this->_index = 0;
    this->_capacity = 0;
    this->_initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::move(const_iterator& other)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::move");
    errno_abort_if_uninitialised(other._initialised_state, "ft_unordered_map::const_iterator::move");
    this->_data = other._data;
    this->_occupied = other._occupied;
    this->_index = other._index;
    this->_capacity = other._capacity;
    this->advance_to_valid_index_unlocked();
    other._data = ft_nullptr;
    other._occupied = ft_nullptr;
    other._index = 0;
    other._capacity = 0;
    other._initialised_state
        = FT_CLASS_STATE_DESTROYED;
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_last_error);
}

template <typename Key, typename MappedType>
int32_t ft_unordered_map<Key, MappedType>::const_iterator::get_error() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_unordered_map::const_iterator::get_error");
    return (this->_last_error);
}

template <typename Key, typename MappedType>
void ft_unordered_map<Key, MappedType>::const_iterator::advance_to_valid_index_unlocked()
{
    if (this->_occupied == ft_nullptr)
        return ;
    while (this->_index < this->_capacity)
    {
        if (this->_occupied[this->_index])
            return ;
        this->_index += 1;
    }
    return ;
}

template <typename Key, typename MappedType>
const ft_pair<Key, MappedType>&
ft_unordered_map<Key, MappedType>::const_iterator::operator*() const
{
    static ft_pair<Key, MappedType> error_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::operator*");
    if (this->_data == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (error_value);
    }
    if (this->_occupied == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (error_value);
    }
    if (this->_index >= this->_capacity)
    {
        (void)set_error(FT_ERR_OUT_OF_RANGE);
        return (error_value);
    }
    if (!this->_occupied[this->_index])
    {
        (void)set_error(FT_ERR_NOT_FOUND);
        return (error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_data[this->_index]);
}

template <typename Key, typename MappedType>
const ft_pair<Key, MappedType>*
ft_unordered_map<Key, MappedType>::const_iterator::operator->() const
{
    static ft_pair<Key, MappedType> error_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::operator->");
    if (this->_data == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (&error_value);
    }
    if (this->_occupied == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_POINTER);
        return (&error_value);
    }
    if (this->_index >= this->_capacity)
    {
        (void)set_error(FT_ERR_OUT_OF_RANGE);
        return (&error_value);
    }
    if (!this->_occupied[this->_index])
    {
        (void)set_error(FT_ERR_NOT_FOUND);
        return (&error_value);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (&this->_data[this->_index]);
}

template <typename Key, typename MappedType>
typename ft_unordered_map<Key, MappedType>::const_iterator&
ft_unordered_map<Key, MappedType>::const_iterator::operator++()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::operator++");
    if (this->_index < this->_capacity)
        this->_index += 1;
    this->advance_to_valid_index_unlocked();
    (void)set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::const_iterator::operator==(
    const const_iterator& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::operator==");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_unordered_map::const_iterator::operator==");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_data == other._data
        && this->_occupied == other._occupied
        && this->_index == other._index
        && this->_capacity == other._capacity);
}

template <typename Key, typename MappedType>
ft_bool ft_unordered_map<Key, MappedType>::const_iterator::operator!=(
    const const_iterator& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_unordered_map::const_iterator::operator!=");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_unordered_map::const_iterator::operator!=");
    (void)set_error(FT_ERR_SUCCESS);
    return (!(*this == other));
}


template <typename Key, typename MappedType>
using ft_unord_map = ft_unordered_map<Key, MappedType>;

#endif

#ifndef FT_MAP_HPP
#define FT_MAP_HPP

#include "pair.hpp"
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
#include <type_traits>

template <typename Key, typename MappedType>
class ft_map
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        Pair<Key, MappedType>*      _data;
        ft_size_t                      _capacity;
        ft_size_t                      _size;
        mutable pt_recursive_mutex* _mutex;
        uint8_t                     _initialised_state;
        ft_size_t                      _initial_capacity;
        static thread_local int32_t _last_error;

        int32_t set_error(int32_t error_code) const noexcept;

        ft_bool    ensure_capacity(ft_size_t desired_capacity);
        ft_size_t  find_index(const Key& key) const;
        void    destroy_all_unlocked();
        Pair<Key, MappedType>*  end_pointer_unlocked();
        const Pair<Key, MappedType>* end_pointer_unlocked() const;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        int32_t     lock_pair(const ft_map &other, ft_bool *lock_this, ft_bool *lock_other) const;
        void    unlock_pair(const ft_map &other, ft_bool lock_this, ft_bool lock_other) const;
        template <typename Type, typename = void>
        struct has_initialize_move
            : std::false_type
        {
        };
        template <typename Type>
        struct has_initialize_move<Type,
            std::void_t<decltype(std::declval<Type&>().initialize(std::declval<Type&&>()))> >
            : std::true_type
        {
        };
        template <typename Type, typename = void>
        struct has_initialize_copy
            : std::false_type
        {
        };
        template <typename Type>
        struct has_initialize_copy<Type,
            std::void_t<decltype(std::declval<Type&>().initialize(std::declval<const Type&>()))> >
            : std::true_type
        {
        };
        template <typename Type, typename = void>
        struct has_destroy
            : std::false_type
        {
        };
        template <typename Type>
        struct has_destroy<Type, std::void_t<decltype(std::declval<Type&>().destroy())> >
            : std::true_type
        {
        };

    public:
        ft_map(ft_size_t initial_capacity = 10);
        ft_map(const ft_map<Key, MappedType>& other) = delete;
        ft_map(ft_map<Key, MappedType>&& other) = delete;
        ~ft_map();

        ft_map& operator=(const ft_map& other);
        ft_map& operator=(ft_map&& other);

        int32_t     initialize();
        int32_t     destroy();
        int32_t move(ft_map& other);

        int32_t copy_from(const ft_map& other);
        int32_t move_from(ft_map& other);

        void        insert(const Key& key, const MappedType& value);
        void        insert(const Key& key, MappedType&& value);
        Pair<Key, MappedType>* find(const Key& key);
        const Pair<Key, MappedType>* find(const Key& key) const;
        void        remove(const Key& key);
        ft_bool        empty() const;
        void        clear();
        ft_size_t      size() const;
        ft_size_t      capacity() const;
        Pair<Key, MappedType>* end();
        const Pair<Key, MappedType>* end() const;
        MappedType& at(const Key& key);
        const MappedType& at(const Key& key) const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool        is_thread_safe() const;
        uint8_t     is_initialised() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const;
        int32_t         unlock(ft_bool lock_acquired) const;

        int8_t      compare(const ft_map& other) const;

            int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename Key, typename MappedType>
ft_map<Key, MappedType>::ft_map(ft_size_t initial_capacity)
    : _data(ft_nullptr)
    , _capacity(initial_capacity)
    , _size(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _initial_capacity(initial_capacity)
{
    return ;
}

template <typename Key, typename MappedType>
ft_map<Key, MappedType>::~ft_map()
{
    uint32_t previous_error;

    previous_error = ft_map<Key, MappedType>::_last_error;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)this->set_error(previous_error);
        return ;
    }
    (void)this->destroy();
    (void)this->set_error(previous_error);
    return ;
}

template <typename Key, typename MappedType>
ft_map<Key, MappedType>& ft_map<Key, MappedType>::operator=(
    const ft_map<Key, MappedType>& other)
{
    if (this == &other)
        return (*this);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_map::operator=(const ft_map&)", "source object is not initialised");
        return (*this);
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        (void)this->initialize();
    (void)this->copy_from(other);
    return (*this);
}

template <typename Key, typename MappedType>
ft_map<Key, MappedType>& ft_map<Key, MappedType>::operator=(
    ft_map<Key, MappedType>&& other)
{
    if (this == &other)
        return (*this);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_map::operator=(ft_map&&)", "source object is not initialised");
        return (*this);
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        (void)this->initialize();
    (void)this->move_from(other);
    return (*this);
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_map::initialize", "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    this->_capacity = this->_initial_capacity;
    this->_size = 0;
    if (this->_capacity > 0)
    {
        void *raw_memory = cma_malloc(sizeof(Pair<Key, MappedType>) * this->_capacity);

        if (raw_memory == ft_nullptr)
        {
            this->_capacity = 0;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->set_error(FT_ERR_NO_MEMORY));
        }
        this->_data = static_cast<Pair<Key, MappedType> *>(raw_memory);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::destroy()
{
    int32_t first_error;
    int32_t clear_error;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    first_error = this->disable_thread_safety();
    this->clear();
    clear_error = this->get_error();
    if (first_error == FT_ERR_SUCCESS && clear_error != FT_ERR_SUCCESS)
        first_error = clear_error;
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    this->_capacity = 0;
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(first_error));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::move(ft_map<Key, MappedType>& other)
{
    return (this->set_error(this->move_from(other)));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::copy_from(const ft_map<Key, MappedType>& other)
{
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_map::copy_from", "source object is not initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initial_capacity = other._capacity;
        int32_t init_error = this->initialize();
        if (init_error != FT_ERR_SUCCESS)
            return (this->set_error(init_error));
    }
    ft_bool lock_this = FT_FALSE;
    ft_bool lock_other = FT_FALSE;
    int32_t lock_error = this->lock_pair(other, &lock_this, &lock_other);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->destroy_all_unlocked();
    if (this->_capacity < other._size)
    {
        ft_size_t next_capacity = other._size;
        if (!this->ensure_capacity(next_capacity))
        {
            this->unlock_pair(other, lock_this, lock_other);
            return (this->set_error(FT_ERR_NO_MEMORY));
        }
    }
    ft_size_t index = 0;
    while (index < other._size)
    {
        construct_at(&this->_data[index], other._data[index]);
        index++;
    }
    this->_size = other._size;
    this->unlock_pair(other, lock_this, lock_other);
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::move_from(ft_map<Key, MappedType>& other)
{
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_map::move_from", "source object is not initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initial_capacity = 0;
        int32_t init_error = this->initialize();
        if (init_error != FT_ERR_SUCCESS)
            return (this->set_error(init_error));
    }
    ft_bool lock_this = FT_FALSE;
    ft_bool lock_other = FT_FALSE;
    int32_t lock_error = this->lock_pair(other, &lock_this, &lock_other);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->destroy_all_unlocked();
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    this->_data = other._data;
    this->_capacity = other._capacity;
    this->_size = other._size;
    other._data = ft_nullptr;
    other._capacity = 0;
    other._size = 0;
    this->unlock_pair(other, lock_this, lock_other);
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
ft_bool ft_map<Key, MappedType>::ensure_capacity(ft_size_t desired_capacity)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::ensure_capacity");
    if (desired_capacity <= this->_capacity)
        return (FT_TRUE);
    void *raw_memory = cma_malloc(sizeof(Pair<Key, MappedType>) * desired_capacity);

    if (raw_memory == ft_nullptr)
        return (FT_FALSE);
    Pair<Key, MappedType> *new_data = static_cast<Pair<Key, MappedType> *>(raw_memory);
    ft_size_t index = 0;

    while (index < this->_size)
    {
        construct_at(&new_data[index]);
        new_data[index].key = this->_data[index].key;
        if constexpr (std::is_pointer<MappedType>::value)
            new_data[index].value = this->_data[index].value;
        else if constexpr (has_initialize_move<MappedType>::value)
        {
            int32_t initialize_error;

            initialize_error = new_data[index].value.initialize(this->_data[index].value);
            if (initialize_error != FT_ERR_SUCCESS)
            {
                ft_size_t cleanup_index = 0;
                while (cleanup_index <= index)
                {
                    destroy_at(&new_data[cleanup_index]);
                    cleanup_index++;
                }
                cma_free(new_data);
                return (FT_FALSE);
            }
        }
        else
            new_data[index].value = this->_data[index].value;
        destroy_at(&this->_data[index]);
        index++;
    }
    if (this->_data != ft_nullptr)
        cma_free(this->_data);
    this->_data = new_data;
    this->_capacity = desired_capacity;
    return (FT_TRUE);
}

template <typename Key, typename MappedType>
ft_size_t ft_map<Key, MappedType>::find_index(const Key& key) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::find_index");
    ft_size_t index = 0;

    while (index < this->_size)
    {
        if (this->_data[index].key == key)
            return (index);
        index++;
    }
    return (this->_size);
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::destroy_all_unlocked()
{
    ft_size_t index = 0;

    while (index < this->_size)
    {
        destroy_at(&this->_data[index]);
        index++;
    }
    this->_size = 0;
    return ;
}

template <typename Key, typename MappedType>
Pair<Key, MappedType>* ft_map<Key, MappedType>::end_pointer_unlocked()
{
    return (this->_data + this->_size);
}

template <typename Key, typename MappedType>
const Pair<Key, MappedType>* ft_map<Key, MappedType>::end_pointer_unlocked() const
{
    return (this->_data + this->_size);
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::lock_internal(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);

    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::unlock_internal(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::unlock_internal");
    if (!lock_acquired || this->_mutex == ft_nullptr)
    {
        return (this->set_error(FT_ERR_SUCCESS));
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::lock_pair(const ft_map<Key, MappedType> &other,
    ft_bool *lock_this, ft_bool *lock_other) const
{
    if (lock_this)
        *lock_this = FT_FALSE;
    if (lock_other)
        *lock_other = FT_FALSE;
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    const ft_map<Key, MappedType> *first = this;
    const ft_map<Key, MappedType> *second = &other;
    if (first->_mutex > second->_mutex)
    {
        first = &other;
        second = this;
    }
    ft_bool first_locked = FT_FALSE;
    int32_t first_error = first->lock_internal(&first_locked);
    if (first_error != FT_ERR_SUCCESS)
        return (this->set_error(first_error));
    ft_bool second_locked = FT_FALSE;
    int32_t second_error = second->lock_internal(&second_locked);
    if (second_error != FT_ERR_SUCCESS)
    {
        (void)first->unlock_internal(first_locked);
        return (this->set_error(second_error));
    }
    if (lock_this)
    {
        if (first == this)
            *lock_this = first_locked;
        else
            *lock_this = second_locked;
    }
    if (lock_other)
    {
        if (first == &other)
            *lock_other = first_locked;
        else
            *lock_other = second_locked;
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::unlock_pair(const ft_map<Key, MappedType> &other,
    ft_bool lock_this, ft_bool lock_other) const
{
    if (this == &other)
        return ;
    (void)other.unlock_internal(lock_other);
    (void)this->unlock_internal(lock_this);
    return ;
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::insert(const Key& key, const MappedType& value)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::insert");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    ft_size_t index = this->find_index(key);

    if (index != this->_size)
    {
        if constexpr (ft_map<Key, MappedType>::template has_destroy<MappedType>::value
            && ft_map<Key, MappedType>::template has_initialize_copy<MappedType>::value)
        {
            int32_t destroy_error = this->_data[index].value.destroy();
            if (destroy_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(destroy_error);
                return ;
            }
            int32_t initialize_error = this->_data[index].value.initialize(value);
            if (initialize_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(initialize_error);
                return ;
            }
        }
        else if constexpr (std::is_copy_assignable<MappedType>::value)
            this->_data[index].value = value;
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    if (this->_size >= this->_capacity)
    {
        ft_size_t next_capacity;

        if (this->_capacity == 0)
            next_capacity = 1;
        else
        {
            next_capacity = this->_capacity * 2;
            if (next_capacity <= this->_capacity)
            {
                ft_size_t incremented_capacity = this->_capacity + 1;
                if (incremented_capacity == 0)
                {
                    (void)this->unlock_internal(lock_acquired);
                    return ;
                }
                next_capacity = incremented_capacity;
            }
        }
        if (!this->ensure_capacity(next_capacity))
        {
            (void)this->unlock_internal(lock_acquired);
            this->set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    if constexpr (ft_map<Key, MappedType>::template has_initialize_copy<MappedType>::value)
    {
        construct_at(&this->_data[this->_size]);
        if constexpr (ft_map<Key, MappedType>::template has_initialize_copy<Key>::value)
        {
            int32_t key_initialize_error = this->_data[this->_size].key.initialize(key);
            if (key_initialize_error != FT_ERR_SUCCESS)
            {
                destroy_at(&this->_data[this->_size]);
                (void)this->unlock_internal(lock_acquired);
                this->set_error(key_initialize_error);
                return ;
            }
        }
        else
            this->_data[this->_size].key = key;
        int32_t initialize_error = this->_data[this->_size].value.initialize(value);
        if (initialize_error != FT_ERR_SUCCESS)
        {
            if constexpr (ft_map<Key, MappedType>::template has_destroy<Key>::value)
                (void)this->_data[this->_size].key.destroy();
            destroy_at(&this->_data[this->_size]);
            (void)this->unlock_internal(lock_acquired);
            this->set_error(initialize_error);
            return ;
        }
    }
    else if constexpr (std::is_copy_constructible<MappedType>::value)
        construct_at(&this->_data[this->_size], key, value);
    else if constexpr (std::is_copy_assignable<MappedType>::value)
    {
        construct_at(&this->_data[this->_size]);
        if constexpr (ft_map<Key, MappedType>::template has_initialize_copy<Key>::value)
        {
            int32_t key_initialize_error = this->_data[this->_size].key.initialize(key);
            if (key_initialize_error != FT_ERR_SUCCESS)
            {
                destroy_at(&this->_data[this->_size]);
                (void)this->unlock_internal(lock_acquired);
                this->set_error(key_initialize_error);
                return ;
            }
        }
        else
            this->_data[this->_size].key = key;
        this->_data[this->_size].value = value;
    }
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::insert(const Key& key, MappedType&& value)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::insert_move");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    ft_size_t index = this->find_index(key);

    if (index != this->_size)
    {
        if constexpr (ft_map<Key, MappedType>::template has_destroy<MappedType>::value
            && ft_map<Key, MappedType>::template has_initialize_move<MappedType>::value)
        {
            int32_t destroy_error = this->_data[index].value.destroy();
            if (destroy_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(destroy_error);
                return ;
            }
            int32_t initialize_error = this->_data[index].value.initialize(ft_move(value));
            if (initialize_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(initialize_error);
                return ;
            }
        }
        else if constexpr (std::is_move_assignable<MappedType>::value)
            this->_data[index].value = ft_move(value);
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    if (this->_size >= this->_capacity)
    {
        ft_size_t next_capacity;

        if (this->_capacity == 0)
            next_capacity = 1;
        else
        {
            next_capacity = this->_capacity * 2;
            if (next_capacity <= this->_capacity)
            {
                ft_size_t incremented_capacity = this->_capacity + 1;
                if (incremented_capacity == 0)
                {
                    (void)this->unlock_internal(lock_acquired);
                    return ;
                }
                next_capacity = incremented_capacity;
            }
        }
        if (!this->ensure_capacity(next_capacity))
        {
            (void)this->unlock_internal(lock_acquired);
            this->set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    if constexpr (ft_map<Key, MappedType>::template has_initialize_move<MappedType>::value)
    {
        construct_at(&this->_data[this->_size]);
        if constexpr (ft_map<Key, MappedType>::template has_initialize_copy<Key>::value)
        {
            int32_t key_initialize_error = this->_data[this->_size].key.initialize(key);
            if (key_initialize_error != FT_ERR_SUCCESS)
            {
                destroy_at(&this->_data[this->_size]);
                (void)this->unlock_internal(lock_acquired);
                this->set_error(key_initialize_error);
                return ;
            }
        }
        else
            this->_data[this->_size].key = key;
        int32_t initialize_error = this->_data[this->_size].value.initialize(ft_move(value));
        if (initialize_error != FT_ERR_SUCCESS)
        {
            if constexpr (ft_map<Key, MappedType>::template has_destroy<Key>::value)
                (void)this->_data[this->_size].key.destroy();
            destroy_at(&this->_data[this->_size]);
            (void)this->unlock_internal(lock_acquired);
            this->set_error(initialize_error);
            return ;
        }
    }
    else if constexpr (std::is_move_constructible<MappedType>::value)
        construct_at(&this->_data[this->_size], key, ft_move(value));
    else if constexpr (std::is_move_assignable<MappedType>::value)
    {
        construct_at(&this->_data[this->_size]);
        if constexpr (ft_map<Key, MappedType>::template has_initialize_copy<Key>::value)
        {
            int32_t key_initialize_error = this->_data[this->_size].key.initialize(key);
            if (key_initialize_error != FT_ERR_SUCCESS)
            {
                destroy_at(&this->_data[this->_size]);
                (void)this->unlock_internal(lock_acquired);
                this->set_error(key_initialize_error);
                return ;
            }
        }
        else
            this->_data[this->_size].key = key;
        this->_data[this->_size].value = ft_move(value);
    }
    this->_size += 1;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
Pair<Key, MappedType>* ft_map<Key, MappedType>::find(const Key& key)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::find");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    Pair<Key, MappedType>* result;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->end_pointer_unlocked());
    }
    ft_size_t index = this->find_index(key);

    if (index == this->_size)
        result = this->end_pointer_unlocked();
    else
        result = &this->_data[index];
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
const Pair<Key, MappedType>* ft_map<Key, MappedType>::find(const Key& key) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::find_const");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    const Pair<Key, MappedType>* result;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->end_pointer_unlocked());
    }
    ft_size_t index = this->find_index(key);

    if (index == this->_size)
        result = this->end_pointer_unlocked();
    else
        result = &this->_data[index];
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::remove(const Key& key)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::remove");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    ft_size_t index = this->find_index(key);

    if (index == this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    ft_size_t move_index = index;

    while (move_index + 1 < this->_size)
    {
        this->_data[move_index].key = this->_data[move_index + 1].key;
        if constexpr (ft_map<Key, MappedType>::template has_destroy<MappedType>::value
            && ft_map<Key, MappedType>::template has_initialize_copy<MappedType>::value)
        {
            int32_t destroy_error = this->_data[move_index].value.destroy();
            if (destroy_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(destroy_error);
                return ;
            }
            int32_t initialize_error = this->_data[move_index].value.initialize(this->_data[move_index + 1].value);
            if (initialize_error != FT_ERR_SUCCESS)
            {
                (void)this->unlock_internal(lock_acquired);
                this->set_error(initialize_error);
                return ;
            }
        }
        else if constexpr (std::is_copy_assignable<MappedType>::value)
            this->_data[move_index].value = this->_data[move_index + 1].value;
        move_index++;
    }
    if (this->_size > 0)
    {
        destroy_at(&this->_data[this->_size - 1]);
        this->_size -= 1;
    }
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
ft_bool ft_map<Key, MappedType>::empty() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::empty");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    ft_bool    is_empty;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (is_empty);
}

template <typename Key, typename MappedType>
void ft_map<Key, MappedType>::clear()
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->destroy_all_unlocked();
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Key, typename MappedType>
ft_size_t ft_map<Key, MappedType>::size() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::size");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    ft_size_t current_size;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    current_size = this->_size;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (current_size);
}

template <typename Key, typename MappedType>
ft_size_t ft_map<Key, MappedType>::capacity() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::capacity");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    ft_size_t current_capacity;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    current_capacity = this->_capacity;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (current_capacity);
}

template <typename Key, typename MappedType>
Pair<Key, MappedType>* ft_map<Key, MappedType>::end()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::end");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    Pair<Key, MappedType>* result;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->end_pointer_unlocked());
    }
    result = this->end_pointer_unlocked();
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
const Pair<Key, MappedType>* ft_map<Key, MappedType>::end() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::end_const");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);
    const Pair<Key, MappedType>* result;

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->end_pointer_unlocked());
    }
    result = this->end_pointer_unlocked();
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
MappedType& ft_map<Key, MappedType>::at(const Key& key)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::at");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_data[0].value);
    }
    ft_size_t index = this->find_index(key);

    if (index == this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (this->_data[0].value);
    }
    MappedType& result = this->_data[index].value;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
const MappedType& ft_map<Key, MappedType>::at(const Key& key) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::at_const");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = this->lock_internal(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_data[0].value);
    }
    ft_size_t index = this->find_index(key);

    if (index == this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (this->_data[0].value);
    }
    const MappedType& result = this->_data[index].value;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::enable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    this->_mutex = new (std::nothrow) pt_recursive_mutex();
    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_NO_MEMORY));
    int32_t init_error = this->_mutex->initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        return (this->set_error(init_error));
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::disable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (this->set_error(destroy_error));
}

template <typename Key, typename MappedType>
ft_bool ft_map<Key, MappedType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::is_thread_safe");
    const ft_bool result = (this->_mutex != ft_nullptr);
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename Key, typename MappedType>
uint8_t ft_map<Key, MappedType>::is_initialised() const noexcept
{
    return (this->_initialised_state);
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    return (this->set_error(lock_result));
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::unlock");
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
}

template <typename Key, typename MappedType>
int8_t ft_map<Key, MappedType>::compare(const ft_map<Key, MappedType>& other) const
{
    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (0);
    }
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_map::compare");
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_map::compare", "source object is not initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    ft_bool lock_this = FT_FALSE;
    ft_bool lock_other = FT_FALSE;
    int32_t lock_error = this->lock_pair(other, &lock_this, &lock_other);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (-1);
    }
    if (this->_size != other._size)
    {
        this->unlock_pair(other, lock_this, lock_other);
        this->set_error(FT_ERR_SUCCESS);
        return (1);
    }
    ft_size_t index = 0;
    while (index < this->_size)
    {
        const Key &current_key = this->_data[index].key;
        ft_size_t other_index = other.find_index(current_key);
        if (other_index == other._size)
        {
            this->unlock_pair(other, lock_this, lock_other);
            this->set_error(FT_ERR_SUCCESS);
            return (1);
        }
        if (!(this->_data[index].value == other._data[other_index].value))
        {
            this->unlock_pair(other, lock_this, lock_other);
            this->set_error(FT_ERR_SUCCESS);
            return (1);
        }
        index++;
    }
    this->unlock_pair(other, lock_this, lock_other);
    this->set_error(FT_ERR_SUCCESS);
    return (0);
}


template <typename Key, typename MappedType>
thread_local int32_t ft_map<Key, MappedType>::_last_error = FT_ERR_SUCCESS;

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::set_error(int32_t error_code) const noexcept
{
    ft_map<Key, MappedType>::_last_error = error_code;
    return (error_code);
}

template <typename Key, typename MappedType>
int32_t ft_map<Key, MappedType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_map::get_error");
    return (ft_map<Key, MappedType>::_last_error);
}

template <typename Key, typename MappedType>
const char *ft_map<Key, MappedType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_map::get_error_str");
    return (ft_strerror(this->get_error()));
}

#endif

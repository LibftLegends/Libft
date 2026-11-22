#ifndef FT_VECTOR_HPP
#define FT_VECTOR_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../CMA/CMA.hpp"
#include "constructor.hpp"
#include "move.hpp"
#include <type_traits>
#include <cstdint>
#include <memory>
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"

template <typename Type, typename = void>
struct ft_is_complete
    : std::false_type
{
};

template <typename Type>
struct ft_is_complete<Type, std::void_t<decltype(sizeof(Type))> >
    : std::true_type
{
};

template <typename Type, typename Arg, typename = void>
struct ft_has_initialize_with_arg
    : std::false_type
{
};

template <typename Type, typename Arg>
struct ft_has_initialize_with_arg<Type, Arg,
    std::void_t<decltype(std::declval<Type &>().initialize(std::declval<Arg>()))> >
    : std::true_type
{
};

template <typename Type, ft_size_t InlineCapacity, ft_bool Enabled>
struct ft_vector_inline_storage
{
};

template <typename Type, ft_size_t InlineCapacity>
struct ft_vector_inline_storage<Type, InlineCapacity, FT_TRUE>
{
    alignas(Type) unsigned char buffer[sizeof(Type) * InlineCapacity];
};
template <typename ElementType>
class ft_vector
{
    private:
        static const ft_size_t SMALL_CAPACITY = 8;
        static const ft_bool SMALL_BUFFER_AVAILABLE = ft_is_complete<ElementType>::value;

        ft_vector_inline_storage<ElementType, SMALL_CAPACITY,
                SMALL_BUFFER_AVAILABLE> _inline_storage;
        ElementType    *_data;
        ft_size_t       _size;
        ft_size_t       _capacity;
        mutable pt_recursive_mutex *_mutex;
        uint8_t         _initialised_state;
        static thread_local int32_t _last_error;

        void    destroy_elements_unlocked(ft_size_t from, ft_size_t to);
        int32_t reserve_internal_unlocked(ft_size_t new_capacity);
        int32_t grow_internal_unlocked();
        ElementType    *small_data();
        const ElementType  *small_data() const;
        ft_bool using_small_buffer() const;
        void    reset_to_small_buffer();
    int32_t     lock_internal(ft_bool *lock_acquired) const;
    int32_t     unlock_internal(ft_bool lock_acquired) const;
        template <typename ArgType>
        int32_t construct_element_unlocked(ElementType *destination, ArgType &&value);
        static int32_t set_error(int32_t error_code) noexcept;

    protected:
        ElementType release_at(ft_size_t index);

    public:
        using iterator = ElementType*;
        using const_iterator = const ElementType*;

        ft_vector(ft_size_t initial_capacity = 0);
        ft_vector(const ft_vector<ElementType> &other) = delete;
        ft_vector(ft_vector<ElementType> &&other) = delete;
        ~ft_vector();

        ft_vector& operator=(const ft_vector&) = delete;
        ft_vector& operator=(ft_vector&& other) = delete;

        int32_t     initialize() noexcept;
        int32_t     initialize(const ft_vector<ElementType> &other);
        int32_t     move(ft_vector<ElementType> &other);
        int32_t     destroy() noexcept;

        int32_t     enable_thread_safety();
        int32_t     disable_thread_safety();
        int32_t     copy_from(const ft_vector<ElementType> &other);
        ft_bool is_thread_safe() const;
        uint8_t is_initialised() const noexcept;
        int32_t     lock(ft_bool *lock_acquired) const;
        void    unlock(ft_bool lock_acquired) const;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        ft_size_t size() const;
        ft_size_t capacity() const;
        ft_bool empty() const;

        int32_t push_back(const ElementType &value);
        int32_t push_back(ElementType &&value);
        void pop_back();

        ElementType& operator[](ft_size_t index);
        const ElementType& operator[](ft_size_t index) const;

        void clear();
        void reserve(ft_size_t new_capacity);
        void resize(ft_size_t new_size, const ElementType& value = ElementType());

        iterator insert(iterator pos, const ElementType& value);
        iterator erase(iterator pos);
        iterator begin();
        const_iterator begin() const;
        iterator end();
        const_iterator end() const;
};

template <typename ElementType>
thread_local int32_t ft_vector<ElementType>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType>
int32_t ft_vector<ElementType>::set_error(int32_t error_code) noexcept
{
    ft_vector<ElementType>::_last_error = error_code;
    return (error_code);
}

template <typename ElementType>
int32_t ft_vector<ElementType>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_vector::get_error");
    return (ft_vector<ElementType>::_last_error);
}

template <typename ElementType>
const char *ft_vector<ElementType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_vector::get_error_str");
    return (ft_strerror(this->get_error()));
}

template <typename ElementType>
template <typename ArgType>
int32_t ft_vector<ElementType>::construct_element_unlocked(
    ElementType *destination, ArgType &&value)
{
    if constexpr (std::is_constructible<ElementType, ArgType&&>::value)
    {
        construct_at(destination, std::forward<ArgType>(value));
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    else if constexpr (std::is_default_constructible<ElementType>::value
        && ft_has_initialize_with_arg<ElementType, ArgType&&>::value)
    {
        int32_t initialize_error;

        construct_default_at(destination);
        initialize_error = destination->initialize(std::forward<ArgType>(value));
        if (initialize_error != FT_ERR_SUCCESS)
        {
            ::destroy_at(destination);
            return (ft_vector<ElementType>::set_error(initialize_error));
        }
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    else if constexpr (std::is_default_constructible<ElementType>::value
        && std::is_assignable<ElementType &, ArgType&&>::value)
    {
        construct_default_at(destination);
        *destination = std::forward<ArgType>(value);
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    else
    {
        return (ft_vector<ElementType>::set_error(FT_ERR_UNSUPPORTED_TYPE));
    }
}

template <typename ElementType>
ft_vector<ElementType>::ft_vector(ft_size_t initial_capacity)
    : _data(ft_nullptr),
      _size(0),
      _capacity(0),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->reset_to_small_buffer();
    if (initial_capacity > SMALL_CAPACITY)
    {
        int32_t reserve_error;

        reserve_error = this->reserve_internal_unlocked(initial_capacity);
        if (reserve_error != FT_ERR_SUCCESS)
        {
            return ;
        }
    }
    return ;
}

template <typename ElementType>
ft_vector<ElementType>::~ft_vector()
{
    int32_t previous_error;

    previous_error = ft_vector<ElementType>::_last_error;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_data != ft_nullptr && this->using_small_buffer() == FT_FALSE)
        cma_free(this->_data);
    (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)ft_vector<ElementType>::set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_vector<ElementType>::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_vector::initialize",
            "called while object is already initialised");
        return (ft_vector<ElementType>::set_error(FT_ERR_INVALID_STATE));
    }
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    {
        ft_bool small_buffer_in_use = this->using_small_buffer();
        this->destroy_elements_unlocked(0, this->_size);
        if (this->_data != ft_nullptr && small_buffer_in_use == FT_FALSE)
            cma_free(this->_data);
        this->reset_to_small_buffer();
        this->_size = 0;
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_vector<ElementType>::set_error(first_error));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::initialize(const ft_vector<ElementType> &other)
{
    int32_t initialization_error = this->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
        return (ft_vector<ElementType>::set_error(initialization_error));
    return (ft_vector<ElementType>::set_error(this->copy_from(other)));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::move(ft_vector<ElementType> &other)
{
    if (this == &other)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_vector::move",
            "source object is uninitialised");
        return (ft_vector<ElementType>::set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (ft_vector<ElementType>::set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    int32_t initialization_error = this->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
        return (ft_vector<ElementType>::set_error(initialization_error));
    ft_bool other_lock_acquired = FT_FALSE;
    int32_t other_lock_error = other.lock_internal(&other_lock_acquired);
    if (other_lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_vector<ElementType>::set_error(other_lock_error));
    }
    if (other.using_small_buffer())
    {
        ft_size_t index = 0;
        while (index < other._size)
        {
            if (this->construct_element_unlocked(&this->_data[index],
                    other._data[index]) != FT_ERR_SUCCESS)
            {
                while (index > 0)
                {
                    index -= 1;
                    ::destroy_at(&this->_data[index]);
                }
                this->_size = 0;
                this->_initialised_state = FT_CLASS_STATE_DESTROYED;
                other.unlock_internal(other_lock_acquired);
                return (this->get_error());
            }
            ++index;
        }
        this->_size = other._size;
        other.destroy_elements_unlocked(0, other._size);
        other._size = 0;
    }
    else
    {
        if (this->_data != ft_nullptr && this->using_small_buffer() == FT_FALSE)
            cma_free(this->_data);
        this->_data = other._data;
        this->_size = other._size;
        this->_capacity = other._capacity;
        other._data = ft_nullptr;
        other._size = 0;
        other.reset_to_small_buffer();
    }
    other.unlock_internal(other_lock_acquired);
    pt_recursive_mutex *moved_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_mutex = moved_mutex;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::copy_from(const ft_vector<ElementType> &other)
{
    if (this == &other)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    ft_bool self_lock_acquired = FT_FALSE;
    int32_t self_lock_error = this->lock_internal(&self_lock_acquired);
    if (self_lock_error != FT_ERR_SUCCESS)
        return (ft_vector<ElementType>::set_error(self_lock_error));
    ft_bool other_lock_acquired = FT_FALSE;
    int32_t other_lock_error = other.lock_internal(&other_lock_acquired);
    if (other_lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(self_lock_acquired);
        return (ft_vector<ElementType>::set_error(other_lock_error));
    }
    this->destroy_elements_unlocked(0, this->_size);
    if (this->_data != ft_nullptr && this->using_small_buffer() == FT_FALSE)
        cma_free(this->_data);
    this->reset_to_small_buffer();
    this->_size = 0;
    int32_t reserve_error = this->reserve_internal_unlocked(other._capacity);
    if (reserve_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_lock_acquired);
        this->unlock_internal(self_lock_acquired);
        return (ft_vector<ElementType>::set_error(reserve_error));
    }
    ft_size_t index = 0;
    while (index < other._size)
    {
        if (this->construct_element_unlocked(&this->_data[index],
                other._data[index]) != FT_ERR_SUCCESS)
        {
            while (index > 0)
            {
                index -= 1;
                ::destroy_at(&this->_data[index]);
            }
            this->_size = 0;
            other.unlock_internal(other_lock_acquired);
            this->unlock_internal(self_lock_acquired);
            return (this->get_error());
        }
        ++index;
    }
    this->_size = other._size;
    other.unlock_internal(other_lock_acquired);
    this->unlock_internal(self_lock_acquired);
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}


template <typename ElementType>
int32_t ft_vector<ElementType>::enable_thread_safety()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_vector<ElementType>::set_error(FT_ERR_NO_MEMORY));
    int32_t initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (ft_vector<ElementType>::set_error(initialize_error));
    }
    this->_mutex = mutex_pointer;
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (ft_vector<ElementType>::set_error(FT_ERR_INVALID_STATE));
    if (this->_mutex == ft_nullptr)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    return (ft_vector<ElementType>::set_error(destroy_error));
}

template <typename ElementType>
ft_bool ft_vector<ElementType>::is_thread_safe() const
{
    ft_bool enabled;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::is_thread_safe");
    enabled = (this->_mutex != ft_nullptr);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (enabled);
}

template <typename ElementType>
uint8_t ft_vector<ElementType>::is_initialised() const noexcept
{
    return (this->_initialised_state);
}

template <typename ElementType>
int32_t ft_vector<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::lock");
    lock_error = this->lock_internal(lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (lock_error);
    }
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_vector<ElementType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::unlock");
    (void)this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ft_size_t ft_vector<ElementType>::size() const
{
    ft_bool lock_acquired;
    ft_size_t current_size;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (0);
    }
    current_size = this->_size;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (current_size);
}

template <typename ElementType>
ft_size_t ft_vector<ElementType>::capacity() const
{
    ft_bool lock_acquired;
    ft_size_t current_capacity;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::capacity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (0);
    }
    current_capacity = this->_capacity;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (current_capacity);
}

template <typename ElementType>
ft_bool ft_vector<ElementType>::empty() const
{
    ft_bool lock_acquired;
    ft_bool is_empty;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty = (this->_size == 0);
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (is_empty);
}

template <typename ElementType>
int32_t ft_vector<ElementType>::push_back(const ElementType &value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t reserve_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::push_back(copy)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (lock_error);
    }
    if (this->_size >= this->_capacity)
    {
        reserve_error = this->grow_internal_unlocked();
        if (reserve_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(reserve_error);
            return (reserve_error);
        }
    }
    if (this->construct_element_unlocked(&this->_data[this->_size], value)
        != FT_ERR_SUCCESS)
    {
        reserve_error = ft_vector<ElementType>::_last_error;
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(reserve_error);
        return (reserve_error);
    }
    this->_size += 1;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int32_t ft_vector<ElementType>::push_back(ElementType &&value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t reserve_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::push_back(move)");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (lock_error);
    }
    if (this->_size >= this->_capacity)
    {
        reserve_error = this->grow_internal_unlocked();
        if (reserve_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(reserve_error);
            return (reserve_error);
        }
    }
    if (this->construct_element_unlocked(&this->_data[this->_size], ft_move(value))
        != FT_ERR_SUCCESS)
    {
        reserve_error = ft_vector<ElementType>::_last_error;
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(reserve_error);
        return (reserve_error);
    }
    this->_size += 1;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_vector<ElementType>::pop_back()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_vector::pop_back");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return ;
    }
    if (this->_size > 0)
    {
        ::destroy_at(&this->_data[this->_size - 1]);
        this->_size -= 1;
    }
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ElementType& ft_vector<ElementType>::operator[](ft_size_t index)
{
    ft_bool lock_acquired;
    ElementType       *ref_pointer;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        static ElementType default_instance = ElementType();

        ft_vector<ElementType>::set_error(lock_error);
        return (default_instance);
    }
    if (index >= this->_size)
    {
        static ElementType default_instance = ElementType();

        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(FT_ERR_OUT_OF_RANGE);
        return (default_instance);
    }
    ref_pointer = &this->_data[index];
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (*ref_pointer);
}

template <typename ElementType>
const ElementType& ft_vector<ElementType>::operator[](ft_size_t index) const
{
    ft_bool lock_acquired;
    const ElementType *ref_pointer;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        static ElementType default_instance = ElementType();

        ft_vector<ElementType>::set_error(lock_error);
        return (default_instance);
    }
    if (index >= this->_size)
    {
        static ElementType default_instance = ElementType();

        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(FT_ERR_OUT_OF_RANGE);
        return (default_instance);
    }
    ref_pointer = &this->_data[index];
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (*ref_pointer);
}

template <typename ElementType>
void ft_vector<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return ;
    }
    this->destroy_elements_unlocked(0, this->_size);
    this->_size = 0;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_vector<ElementType>::reserve(ft_size_t new_capacity)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t reserve_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return ;
    }
    reserve_error = this->reserve_internal_unlocked(new_capacity);
    if (reserve_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(reserve_error);
        return ;
    }
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
void ft_vector<ElementType>::resize(ft_size_t new_size, const ElementType& value)
{
    ft_bool lock_acquired;
    ft_size_t index;
    int32_t lock_error;
    int32_t reserve_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return ;
    }
    if (new_size < this->_size)
    {
        this->destroy_elements_unlocked(new_size, this->_size);
    }
    else if (new_size > this->_size)
    {
        reserve_error = this->reserve_internal_unlocked(new_size);
        if (reserve_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(reserve_error);
            return ;
        }
        index = this->_size;
        while (index < new_size)
        {
            if (this->construct_element_unlocked(&this->_data[index], value)
                != FT_ERR_SUCCESS)
            {
                while (index > this->_size)
                {
                    index -= 1;
                    ::destroy_at(&this->_data[index]);
                }
                this->unlock_internal(lock_acquired);
                ft_vector<ElementType>::set_error(ft_vector<ElementType>::_last_error);
                return ;
            }
            ++index;
        }
    }
    this->_size = new_size;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
typename ft_vector<ElementType>::iterator ft_vector<ElementType>::insert(iterator pos, const ElementType& value)
{
    ft_bool lock_acquired;
    ft_size_t index;
    ft_size_t move_index;
    iterator   end_iterator;
    iterator   result_iterator;
    int32_t lock_error;
    int32_t reserve_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data + this->_size);
    }
    index = pos - this->_data;
    if (index > this->_size)
    {
        end_iterator = this->_data + this->_size;
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(FT_ERR_OUT_OF_RANGE);
        return (end_iterator);
    }
    if (this->_size >= this->_capacity)
    {
        reserve_error = this->grow_internal_unlocked();
        if (reserve_error != FT_ERR_SUCCESS)
        {
            end_iterator = this->_data + this->_size;
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(reserve_error);
            return (end_iterator);
        }
        pos = this->_data + index;
    }
    move_index = this->_size;
    while (move_index > index)
    {
        if (this->construct_element_unlocked(&this->_data[move_index],
                this->_data[move_index - 1]) != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(ft_vector<ElementType>::_last_error);
            return (this->_data + this->_size);
        }
        ::destroy_at(&this->_data[move_index - 1]);
        move_index -= 1;
    }
    if (this->construct_element_unlocked(&this->_data[index], value)
        != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(ft_vector<ElementType>::_last_error);
        return (this->_data + this->_size);
    }
    this->_size += 1;
    result_iterator = &this->_data[index];
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (result_iterator);
}

template <typename ElementType>
typename ft_vector<ElementType>::iterator ft_vector<ElementType>::erase(iterator pos)
{
    ft_bool lock_acquired;
    ft_size_t index;
    ft_size_t shift_index;
    iterator   end_iterator;
    iterator   result_iterator;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data + this->_size);
    }
    index = pos - this->_data;
    if (index >= this->_size)
    {
        end_iterator = this->_data + this->_size;
        this->unlock_internal(lock_acquired);
        ft_vector<ElementType>::set_error(FT_ERR_OUT_OF_RANGE);
        return (end_iterator);
    }
    ::destroy_at(&this->_data[index]);
    shift_index = index;
    while (shift_index < this->_size - 1)
    {
        if (this->construct_element_unlocked(&this->_data[shift_index],
                ft_move(this->_data[shift_index + 1])) != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            ft_vector<ElementType>::set_error(ft_vector<ElementType>::_last_error);
            return (this->_data + this->_size);
        }
        ::destroy_at(&this->_data[shift_index + 1]);
        shift_index += 1;
    }
    this->_size -= 1;
    if (index >= this->_size)
        result_iterator = this->_data + this->_size;
    else
        result_iterator = &this->_data[index];
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (result_iterator);
}

template <typename ElementType>
typename ft_vector<ElementType>::iterator ft_vector<ElementType>::begin()
{
    ft_bool lock_acquired;
    iterator iterator_result;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data);
    }
    iterator_result = this->_data;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (iterator_result);
}

template <typename ElementType>
typename ft_vector<ElementType>::const_iterator ft_vector<ElementType>::begin() const
{
    ft_bool lock_acquired;
    const_iterator  iterator_result;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data);
    }
    iterator_result = this->_data;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (iterator_result);
}

template <typename ElementType>
typename ft_vector<ElementType>::iterator ft_vector<ElementType>::end()
{
    ft_bool lock_acquired;
    iterator iterator_result;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data + this->_size);
    }
    iterator_result = this->_data + this->_size;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (iterator_result);
}

template <typename ElementType>
typename ft_vector<ElementType>::const_iterator ft_vector<ElementType>::end() const
{
    ft_bool lock_acquired;
    const_iterator  iterator_result;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_vector<ElementType>::set_error(lock_error);
        return (this->_data + this->_size);
    }
    iterator_result = this->_data + this->_size;
    this->unlock_internal(lock_acquired);
    ft_vector<ElementType>::set_error(FT_ERR_SUCCESS);
    return (iterator_result);
}


template <typename ElementType>
void ft_vector<ElementType>::destroy_elements_unlocked(ft_size_t from, ft_size_t to)
{
    ft_size_t index = from;

    while (index < to)
    {
        ::destroy_at(&this->_data[index]);
        ++index;
    }
    return ;
}

template <typename ElementType>
ElementType* ft_vector<ElementType>::small_data()
{
    if constexpr (SMALL_BUFFER_AVAILABLE != FT_FALSE)
        return (reinterpret_cast<ElementType*>(this->_inline_storage.buffer));
    return (ft_nullptr);
}

template <typename ElementType>
const ElementType* ft_vector<ElementType>::small_data() const
{
    if constexpr (SMALL_BUFFER_AVAILABLE != FT_FALSE)
        return (reinterpret_cast<const ElementType*>(this->_inline_storage.buffer));
    return (ft_nullptr);
}

template <typename ElementType>
ft_bool ft_vector<ElementType>::using_small_buffer() const
{
    if constexpr (SMALL_BUFFER_AVAILABLE != FT_FALSE)
    {
        const ElementType* small_pointer = this->small_data();
        return (this->_data == small_pointer);
    }
    return (FT_FALSE);
}

template <typename ElementType>
void ft_vector<ElementType>::reset_to_small_buffer()
{
    if constexpr (SMALL_BUFFER_AVAILABLE != FT_FALSE)
    {
        this->_data = this->small_data();
        this->_capacity = SMALL_CAPACITY;
    }
    else
    {
        this->_data = ft_nullptr;
        this->_capacity = 0;
    }
    return ;
}

template <typename ElementType>
int32_t ft_vector<ElementType>::reserve_internal_unlocked(ft_size_t new_capacity)
{
    if (new_capacity <= this->_capacity)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    if (new_capacity > FT_SYSTEM_SIZE_MAX / sizeof(ElementType))
        return (ft_vector<ElementType>::set_error(FT_ERR_NO_MEMORY));
    ElementType* new_data = static_cast<ElementType*>(cma_malloc(new_capacity * sizeof(ElementType)));
    if (new_data == ft_nullptr)
    {
        return (ft_vector<ElementType>::set_error(FT_ERR_NO_MEMORY));
    }
    ft_bool had_small_buffer = this->using_small_buffer();
    ElementType* old_data = this->_data;
    ft_size_t current_size = this->_size;
    if constexpr (std::is_trivially_move_constructible_v<ElementType>
        && std::is_trivially_destructible_v<ElementType>)
    {
        if (current_size != 0U)
            std::uninitialized_move_n(old_data, current_size, new_data);
    }
    else
    {
        ft_size_t index = 0U;
        while (index < current_size)
        {
            const int32_t construct_error = this->construct_element_unlocked(
                &new_data[index], ft_move(old_data[index]));
            if (construct_error != FT_ERR_SUCCESS)
            {
                while (index > 0)
                {
                    index -= 1;
                    ::destroy_at(&new_data[index]);
                }
                cma_free(new_data);
                return (ft_vector<ElementType>::set_error(construct_error));
            }
            ++index;
        }
        ft_size_t destroy_index = 0U;
        while (destroy_index < current_size)
        {
            ::destroy_at(&old_data[destroy_index]);
            ++destroy_index;
        }
    }
    if (had_small_buffer == FT_FALSE && old_data != ft_nullptr)
        cma_free(old_data);
    this->_data = new_data;
    this->_capacity = new_capacity;
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::grow_internal_unlocked()
{
    constexpr ft_size_t MAXIMUM_CAPACITY = FT_SYSTEM_SIZE_MAX / sizeof(ElementType);
    if (this->_capacity >= MAXIMUM_CAPACITY)
        return (ft_vector<ElementType>::set_error(FT_ERR_NO_MEMORY));
    if (this->_capacity == 0U)
        return (this->reserve_internal_unlocked(1U));
    if (this->_capacity > MAXIMUM_CAPACITY / 2U)
        return (this->reserve_internal_unlocked(MAXIMUM_CAPACITY));
    return (this->reserve_internal_unlocked(this->_capacity * 2U));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::lock_internal(ft_bool *lock_acquired) const
{
    if (lock_acquired)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
    {
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    }
    int32_t result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (result != FT_ERR_SUCCESS)
        return (ft_vector<ElementType>::set_error(result));
    if (lock_acquired)
        *lock_acquired = FT_TRUE;
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_vector<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (!lock_acquired || this->_mutex == ft_nullptr)
        return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (ft_vector<ElementType>::set_error(FT_ERR_SUCCESS));
}


template <typename ElementType>
ElementType ft_vector<ElementType>::release_at(ft_size_t index)
{
    ft_bool lock_acquired;
    ElementType detached;
    ft_size_t   shift_index;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (ElementType());
    }
    if (index >= this->_size)
    {
        this->unlock_internal(lock_acquired);
        return (ElementType());
    }
    detached = ft_move(this->_data[index]);
    shift_index = index;
    while (shift_index < this->_size - 1)
    {
        this->_data[shift_index] = ft_move(this->_data[shift_index + 1]);
        shift_index += 1;
    }
    this->_size -= 1;
    this->unlock_internal(lock_acquired);
    return (detached);
}

#endif

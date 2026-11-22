#ifndef FT_VARIANT_HPP
#define FT_VARIANT_HPP

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
#include <type_traits>
#include <utility>
#include <cstdint>
#include <new>

template <typename TypeToFind, typename... Types>
struct variant_index;

template <typename TypeToFind, typename... Types>
struct variant_index<TypeToFind, TypeToFind, Types...>
    : std::integral_constant<ft_size_t, 0>
{
};

template <typename TypeToFind, typename CurrentType, typename... Types>
struct variant_index<TypeToFind, CurrentType, Types...>
    : std::integral_constant<ft_size_t, 1 + variant_index<TypeToFind, Types...>::value>
{
};

template <ft_size_t IndexValue, typename... Types>
struct variant_destroyer;

template <ft_size_t IndexValue>
struct variant_destroyer<IndexValue>
{
    static void destroy(ft_size_t, void*)
    {
        return ;
    }
};

template <ft_size_t IndexValue, typename CurrentType, typename... Types>
struct variant_destroyer<IndexValue, CurrentType, Types...>
{
    static void destroy(ft_size_t stored_index, void* storage_pointer)
    {
        if (stored_index == IndexValue)
            ::destroy_at(reinterpret_cast<CurrentType*>(storage_pointer));
        else
            variant_destroyer<IndexValue + 1, Types...>::destroy(stored_index,
                storage_pointer);
        return ;
    }
};

template <ft_size_t IndexValue, typename... Types>
struct variant_visitor;

template <ft_size_t IndexValue>
struct variant_visitor<IndexValue>
{
    template <typename VisitorType>
    static void apply(ft_size_t, void*, VisitorType&&)
    {
        return ;
    }
};

template <ft_size_t IndexValue, typename CurrentType, typename... Types>
struct variant_visitor<IndexValue, CurrentType, Types...>
{
    template <typename VisitorType>
    static void apply(ft_size_t stored_index, void* storage_pointer,
        VisitorType&& visitor)
    {
        if (stored_index == IndexValue)
            visitor(*reinterpret_cast<CurrentType*>(storage_pointer));
        else
            variant_visitor<IndexValue + 1, Types...>::apply(stored_index,
                storage_pointer, std::forward<VisitorType>(visitor));
        return ;
    }
};

template <typename... Types>
class ft_variant
{
    private:
        using storage_type = typename std::aligned_union<0, Types...>::type;

        storage_type               *_data;
        ft_size_t                   _index;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void destroy_unlocked();

    public:
        static const ft_size_t npos = static_cast<ft_size_t>(-1);

        ft_variant();
        ft_variant(const ft_variant &other) = delete;
        ft_variant(ft_variant &&other) = delete;

        template <typename TypeToStore>
        explicit ft_variant(const TypeToStore& value);

        template <typename TypeToStore>
        explicit ft_variant(TypeToStore&& value);

        ~ft_variant();
        ft_variant &operator=(const ft_variant &other) = delete;
        ft_variant &operator=(ft_variant &&other) = delete;

        int32_t initialize();

        template <typename TypeToStore>
        int32_t initialize(TypeToStore&& value);

        int32_t destroy();
        int32_t move(ft_variant &other);

        template <typename TypeToStore>
        void emplace(TypeToStore&& value);

        template <typename TypeToCheck>
        ft_bool holds_alternative() const;

        template <typename TypeToGet>
        class value_proxy
        {
            private:
                TypeToGet *_pointer;
                int32_t    _error;

            public:
                value_proxy(TypeToGet *pointer, int32_t error);
                operator TypeToGet&() const;
                TypeToGet *operator->() const;
                int32_t get_error() const;
        };

        template <typename TypeToGet>
        value_proxy<TypeToGet> get();

        template <typename TypeToGet>
        value_proxy<const TypeToGet> get() const;

        template <typename VisitorType>
        void visit(VisitorType&& visitor);

        void reset();

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename... Types>
thread_local int32_t ft_variant<Types...>::_last_error = FT_ERR_SUCCESS;

template <typename... Types>
int32_t ft_variant<Types...>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename... Types>
int32_t ft_variant<Types...>::lock_internal(ft_bool *lock_acquired) const
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

template <typename... Types>
int32_t ft_variant<Types...>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename... Types>
void ft_variant<Types...>::destroy_unlocked()
{
    if (this->_data == ft_nullptr)
        return ;
    if (this->_index == npos)
        return ;
    variant_destroyer<0, Types...>::destroy(this->_index, this->_data);
    this->_index = npos;
    return ;
}

template <typename... Types>
ft_variant<Types...>::ft_variant()
    : _data(ft_nullptr)
    , _index(npos)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename... Types>
template <typename TypeToStore>
ft_variant<Types...>::ft_variant(const TypeToStore& value)
    : _data(ft_nullptr)
    , _index(npos)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(value);
    (void)set_error(previous_error);
    return ;
}

template <typename... Types>
template <typename TypeToStore>
ft_variant<Types...>::ft_variant(TypeToStore&& value)
    : _data(ft_nullptr)
    , _index(npos)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(ft_move(value));
    (void)set_error(previous_error);
    return ;
}

template <typename... Types>
ft_variant<Types...>::~ft_variant()
{
    uint32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)set_error(previous_error);
    return ;
}

template <typename... Types>
int32_t ft_variant<Types...>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_variant::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = static_cast<storage_type*>(cma_malloc(sizeof(storage_type)));
    if (this->_data == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    this->_index = npos;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename... Types>
template <typename TypeToStore>
int32_t ft_variant<Types...>::initialize(TypeToStore&& value)
{
    int32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (set_error(initialize_error));
    this->emplace<TypeToStore>(std::forward<TypeToStore>(value));
    if (get_error() != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(get_error()));
    }
    return (set_error(FT_ERR_SUCCESS));
}

template <typename... Types>
int32_t ft_variant<Types...>::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    this->destroy_unlocked();
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    (void)this->unlock_internal(lock_acquired);
    disable_error = FT_ERR_SUCCESS;
    if (this->_mutex != ft_nullptr)
        disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        return (set_error(disable_error));
    this->_index = npos;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename... Types>
int32_t ft_variant<Types...>::move(ft_variant<Types...> &other)
{
    int32_t source_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_variant::move",
            "source object is not initialised");
    }
    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_result;

        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_data = ft_nullptr;
        this->_index = npos;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(other._last_error));
    }
    source_error = other._last_error;
    this->_data = other._data;
    this->_index = other._index;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._index = npos;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(source_error));
}

template <typename... Types>
template <typename TypeToStore>
void ft_variant<Types...>::emplace(TypeToStore&& value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::emplace");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_data == ft_nullptr)
    {
        this->_data = static_cast<storage_type*>(cma_malloc(sizeof(storage_type)));
        if (this->_data == ft_nullptr)
        {
            (void)this->unlock_internal(lock_acquired);
            set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    this->destroy_unlocked();
    construct_at(reinterpret_cast<std::decay_t<TypeToStore>*>(this->_data),
        std::forward<TypeToStore>(value));
    this->_index = variant_index<std::decay_t<TypeToStore>, Types...>::value;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename... Types>
template <typename TypeToCheck>
ft_bool ft_variant<Types...>::holds_alternative() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool has_type;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::holds_alternative");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_FALSE);
    }
    has_type = (this->_index == variant_index<TypeToCheck, Types...>::value);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (has_type);
}

template <typename... Types>
template <typename TypeToGet>
ft_variant<Types...>::value_proxy<TypeToGet>::value_proxy(TypeToGet *pointer,
    int32_t error)
    : _pointer(pointer), _error(error)
{
    return ;
}

template <typename... Types>
template <typename TypeToGet>
ft_variant<Types...>::value_proxy<TypeToGet>::operator TypeToGet&() const
{
    if (this->_pointer == ft_nullptr)
    {
        static TypeToGet fallback = TypeToGet();
        return (fallback);
    }
    return (*this->_pointer);
}

template <typename... Types>
template <typename TypeToGet>
TypeToGet *ft_variant<Types...>::value_proxy<TypeToGet>::operator->() const
{
    return (this->_pointer);
}

template <typename... Types>
template <typename TypeToGet>
int32_t ft_variant<Types...>::value_proxy<TypeToGet>::get_error() const
{
    return (this->_error);
}

template <typename... Types>
template <typename TypeToGet>
typename ft_variant<Types...>::template value_proxy<TypeToGet>
ft_variant<Types...>::get()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    TypeToGet *value_pointer;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::get");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (value_proxy<TypeToGet>(ft_nullptr,
            set_error(lock_error)));
    if (this->_index != variant_index<TypeToGet, Types...>::value)
    {
        (void)this->unlock_internal(lock_acquired);
        return (value_proxy<TypeToGet>(ft_nullptr,
            set_error(FT_ERR_INVALID_OPERATION)));
    }
    value_pointer = reinterpret_cast<TypeToGet*>(this->_data);
    (void)this->unlock_internal(lock_acquired);
    return (value_proxy<TypeToGet>(value_pointer,
        set_error(FT_ERR_SUCCESS)));
}

template <typename... Types>
template <typename TypeToGet>
typename ft_variant<Types...>::template value_proxy<const TypeToGet>
ft_variant<Types...>::get() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const TypeToGet *value_pointer;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::get const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (value_proxy<const TypeToGet>(ft_nullptr,
            set_error(lock_error)));
    if (this->_index != variant_index<TypeToGet, Types...>::value)
    {
        (void)this->unlock_internal(lock_acquired);
        return (value_proxy<const TypeToGet>(ft_nullptr,
            set_error(FT_ERR_INVALID_OPERATION)));
    }
    value_pointer = reinterpret_cast<const TypeToGet*>(this->_data);
    (void)this->unlock_internal(lock_acquired);
    return (value_proxy<const TypeToGet>(value_pointer,
        set_error(FT_ERR_SUCCESS)));
}

template <typename... Types>
template <typename VisitorType>
void ft_variant<Types...>::visit(VisitorType&& visitor)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::visit");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    if (this->_index == npos)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_INVALID_OPERATION);
        return ;
    }
    variant_visitor<0, Types...>::apply(this->_index, this->_data,
        std::forward<VisitorType>(visitor));
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename... Types>
void ft_variant<Types...>::reset()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::reset");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->destroy_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename... Types>
int32_t ft_variant<Types...>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::enable_thread_safety");
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

template <typename... Types>
int32_t ft_variant<Types...>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::disable_thread_safety");
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

template <typename... Types>
ft_bool ft_variant<Types...>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename... Types>
int32_t ft_variant<Types...>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_variant::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename... Types>
void ft_variant<Types...>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename... Types>
int32_t ft_variant<Types...>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_variant::get_error");
    return (_last_error);
}

template <typename... Types>
const char *ft_variant<Types...>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_variant::get_error_str");
    return (ft_strerror(_last_error));
}

#endif

#ifndef FT_OPTIONAL_HPP
#define FT_OPTIONAL_HPP

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
#include <type_traits>

template <typename ElementType>
class ft_optional
{
    private:
        ElementType                *_value;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                    _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        static ElementType &fallback_reference() noexcept;
        void destroy_value_unlocked();

    public:
        class value_proxy
        {
            private:
                ElementType *_element_pointer;
                int32_t     _error;

            public:
                value_proxy(ElementType *element_pointer, int32_t error) noexcept;
                operator ElementType&() const noexcept;
                ElementType *operator->() const noexcept;
                int32_t get_error() const noexcept;
        };

        class const_value_proxy
        {
            private:
                const ElementType *_element_pointer;
                int32_t           _error;

            public:
                const_value_proxy(const ElementType *element_pointer,
                    int32_t error) noexcept;
                operator const ElementType&() const noexcept;
                const ElementType *operator->() const noexcept;
                int32_t get_error() const noexcept;
        };

        ft_optional();
        ft_optional(const ft_optional &other) = delete;
        ft_optional(ft_optional &&other) = delete;
        ft_optional(const ElementType& value);
        ft_optional(ElementType&& value);
        ~ft_optional();
        ft_optional &operator=(const ft_optional &other) = delete;
        ft_optional &operator=(ft_optional &&other) = delete;

        int32_t initialize();
        int32_t initialize(const ElementType& value);
        int32_t initialize(ElementType&& value);
        int32_t destroy();
        int32_t move(ft_optional &other);

        ft_bool has_value() const;
        ElementType& value();
        const ElementType& value() const;

        value_proxy operator*() noexcept;
        const_value_proxy operator*() const noexcept;
        ElementType *operator->() noexcept;
        const ElementType *operator->() const noexcept;
        operator ft_bool() const noexcept;

        void reset();
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ElementType>
int32_t ft_optional<ElementType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
int32_t ft_optional<ElementType>::lock_internal(ft_bool *lock_acquired) const
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
int32_t ft_optional<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
ElementType &ft_optional<ElementType>::fallback_reference() noexcept
{
    if constexpr (std::is_default_constructible_v<ElementType>)
    {
        static ElementType fallback = ElementType();
        return (fallback);
    }
    else
    {
        alignas(ElementType) static unsigned char storage[sizeof(ElementType)] = {0};
        return (*reinterpret_cast<ElementType*>(storage));
    }
}

template <typename ElementType>
void ft_optional<ElementType>::destroy_value_unlocked()
{
    if (this->_value == ft_nullptr)
        return ;
    destroy_at(this->_value);
    cma_free(this->_value);
    this->_value = ft_nullptr;
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::value_proxy::value_proxy(
    ElementType *element_pointer, int32_t error) noexcept
    : _element_pointer(element_pointer), _error(error)
{
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::value_proxy::operator ElementType&() const noexcept
{
    if (this->_element_pointer == ft_nullptr)
        return (ft_optional<ElementType>::fallback_reference());
    return (*this->_element_pointer);
}

template <typename ElementType>
ElementType *ft_optional<ElementType>::value_proxy::operator->() const noexcept
{
    return (this->_element_pointer);
}

template <typename ElementType>
int32_t ft_optional<ElementType>::value_proxy::get_error() const noexcept
{
    return (this->_error);
}

template <typename ElementType>
ft_optional<ElementType>::const_value_proxy::const_value_proxy(
    const ElementType *element_pointer, int32_t error) noexcept
    : _element_pointer(element_pointer), _error(error)
{
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::const_value_proxy::operator const ElementType&() const noexcept
{
    if (this->_element_pointer == ft_nullptr)
        return (ft_optional<ElementType>::fallback_reference());
    return (*this->_element_pointer);
}

template <typename ElementType>
const ElementType *ft_optional<ElementType>::const_value_proxy::operator->() const noexcept
{
    return (this->_element_pointer);
}

template <typename ElementType>
int32_t ft_optional<ElementType>::const_value_proxy::get_error() const noexcept
{
    return (this->_error);
}

template <typename ElementType>
ft_optional<ElementType>::ft_optional()
    : _value(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::ft_optional(const ElementType& value)
    : _value(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(value);
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::ft_optional(ElementType&& value)
    : _value(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(ft_move(value));
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
ft_optional<ElementType>::~ft_optional()
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

template <typename ElementType>
int32_t ft_optional<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_optional::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_value = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_optional<ElementType>::initialize(const ElementType& value)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_optional::initialize(copy)", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_value = static_cast<ElementType*>(cma_malloc(sizeof(ElementType)));
    if (this->_value == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    construct_at(this->_value, value);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_optional<ElementType>::initialize(ElementType&& value)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_optional::initialize(move)", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_value = static_cast<ElementType*>(cma_malloc(sizeof(ElementType)));
    if (this->_value == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    construct_at(this->_value, ft_move(value));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_optional<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->destroy_value_unlocked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_optional<ElementType>::move(ft_optional<ElementType> &other)
{
    int32_t destroy_result;
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType *moved_value;
    pt_recursive_mutex *moved_mutex;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_optional::move",
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
    lock_acquired = FT_FALSE;
    lock_error = other.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(lock_error));
    }
    moved_value = other._value;
    moved_mutex = other._mutex;
    other._value = ft_nullptr;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(moved_mutex);
    this->_value = moved_value;
    this->_mutex = moved_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(other._last_error));
}

template <typename ElementType>
ft_bool ft_optional<ElementType>::has_value() const
{
    ft_bool has_stored_value;
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_optional::has_value");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_FALSE);
    }
    has_stored_value = (this->_value != ft_nullptr);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (has_stored_value);
}

template <typename ElementType>
ElementType& ft_optional<ElementType>::value()
{
    const auto get_fallback = []() -> ElementType &
    {
        static ElementType fallback = ElementType();
        return (fallback);
    };
    ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_optional::value");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (get_fallback());
    }
    if (this->_value == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (get_fallback());
    }
    value_pointer = this->_value;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
const ElementType& ft_optional<ElementType>::value() const
{
    const auto get_fallback = []() -> const ElementType &
    {
        static ElementType fallback = ElementType();
        return (fallback);
    };
    const ElementType *value_pointer;
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_optional::value const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (get_fallback());
    }
    if (this->_value == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (get_fallback());
    }
    value_pointer = this->_value;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*value_pointer);
}

template <typename ElementType>
typename ft_optional<ElementType>::value_proxy ft_optional<ElementType>::operator*() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (value_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_value == ft_nullptr)
        return (value_proxy(ft_nullptr, set_error(FT_ERR_EMPTY)));
    return (value_proxy(this->_value, set_error(FT_ERR_SUCCESS)));
}

template <typename ElementType>
typename ft_optional<ElementType>::const_value_proxy ft_optional<ElementType>::operator*()
    const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (const_value_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_value == ft_nullptr)
        return (const_value_proxy(ft_nullptr, set_error(FT_ERR_EMPTY)));
    return (const_value_proxy(this->_value, set_error(FT_ERR_SUCCESS)));
}

template <typename ElementType>
ElementType *ft_optional<ElementType>::operator->() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    if (this->_value == ft_nullptr)
    {
        set_error(FT_ERR_EMPTY);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_value);
}

template <typename ElementType>
const ElementType *ft_optional<ElementType>::operator->() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    if (this->_value == ft_nullptr)
    {
        set_error(FT_ERR_EMPTY);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_value);
}

template <typename ElementType>
ft_optional<ElementType>::operator ft_bool() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_value != ft_nullptr);
}

template <typename ElementType>
void ft_optional<ElementType>::reset()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_optional::reset");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->destroy_value_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_optional<ElementType>::enable_thread_safety() noexcept
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_INVALID_STATE));
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
int32_t ft_optional<ElementType>::disable_thread_safety() noexcept
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
int32_t ft_optional<ElementType>::get_error() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "ft_optional::get_error");
    return (_last_error);
}

template <typename ElementType>
const char *ft_optional<ElementType>::get_error_str() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "ft_optional::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename ElementType>
thread_local int32_t ft_optional<ElementType>::_last_error = FT_ERR_SUCCESS;

#endif

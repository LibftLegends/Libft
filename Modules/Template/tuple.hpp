#ifndef FT_TUPLE_HPP
#define FT_TUPLE_HPP

#include "constructor.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include <tuple>
#include <utility>
#include <new>
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"

template <typename... Types>
class ft_tuple
{
    private:
        using tuple_t = std::tuple<Types...>;

        tuple_t                     *_data;
        mutable pt_recursive_mutex  *_mutex;
        uint8_t                     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code);
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void destroy_locked();

        template <typename ValueType>
        static ValueType &fallback_reference();

    public:
        ft_tuple();
        ft_tuple(const ft_tuple &other) = delete;
        ft_tuple(ft_tuple &&other) = delete;

        template <typename... Args>
        explicit ft_tuple(Args&&... args);

        ~ft_tuple();

        ft_tuple& operator=(const ft_tuple&) = delete;
        ft_tuple& operator=(ft_tuple&& other) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(ft_tuple<Types...> &other);

        template <ft_size_t I>
        typename std::tuple_element<I, tuple_t>::type& get();

        template <ft_size_t I>
        const typename std::tuple_element<I, tuple_t>::type& get() const;

        template <typename T>
        T& get();

        template <typename T>
        const T& get() const;

        void reset();

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const;
        const char *get_error_str() const;
};

template <typename... Types>
thread_local int32_t ft_tuple<Types...>::_last_error = FT_ERR_SUCCESS;

template <typename... Types>
int32_t ft_tuple<Types...>::set_error(int32_t error_code)
{
    _last_error = error_code;
    return (error_code);
}

template <typename... Types>
int32_t ft_tuple<Types...>::lock_internal(ft_bool *lock_acquired) const
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

template <typename... Types>
int32_t ft_tuple<Types...>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename... Types>
void ft_tuple<Types...>::destroy_locked()
{
    if (this->_data != ft_nullptr)
    {
        ::destroy_at(this->_data);
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    return ;
}

template <typename... Types>
template <typename ValueType>
ValueType &ft_tuple<Types...>::fallback_reference()
{
    static ValueType default_value = ValueType();
    return (default_value);
}

template <typename... Types>
ft_tuple<Types...>::ft_tuple()
    : _data(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename... Types>
template <typename... Args>
ft_tuple<Types...>::ft_tuple(Args&&... args)
    : _data(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->_data = static_cast<tuple_t *>(cma_malloc(sizeof(tuple_t)));
    if (this->_data == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return ;
    }
    ::construct_at(this->_data, std::forward<Args>(args)...);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return ;
}

template <typename... Types>
ft_tuple<Types...>::~ft_tuple()
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

template <typename... Types>
int32_t ft_tuple<Types...>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_tuple::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = static_cast<tuple_t *>(cma_malloc(sizeof(tuple_t)));
    if (this->_data == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    ::construct_at(this->_data);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename... Types>
int32_t ft_tuple<Types...>::destroy()
{
    int32_t first_error;
    int32_t disable_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_result;
    this->destroy_locked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename... Types>
int32_t ft_tuple<Types...>::move(ft_tuple<Types...> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_tuple::move",
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
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename... Types>
template <ft_size_t I>
typename std::tuple_element<I, typename ft_tuple<Types...>::tuple_t>::type&
ft_tuple<Types...>::get()
{
    using elem_t = typename std::tuple_element<I, tuple_t>::type;
    ft_bool lock_acquired;
    int32_t lock_result;
    elem_t *reference;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::get");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (fallback_reference<elem_t>());
    if (this->_data == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_OPERATION);
        (void)this->unlock_internal(lock_acquired);
        return (fallback_reference<elem_t>());
    }
    reference = &std::get<I>(*this->_data);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*reference);
}

template <typename... Types>
template <ft_size_t I>
const typename std::tuple_element<I, typename ft_tuple<Types...>::tuple_t>::type&
ft_tuple<Types...>::get() const
{
    using elem_t = typename std::tuple_element<I, tuple_t>::type;
    ft_bool lock_acquired;
    int32_t lock_result;
    const elem_t *reference;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::get const");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (fallback_reference<elem_t>());
    if (this->_data == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_OPERATION);
        (void)this->unlock_internal(lock_acquired);
        return (fallback_reference<elem_t>());
    }
    reference = &std::get<I>(*this->_data);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*reference);
}
template <typename... Types>
template <typename T>
T& ft_tuple<Types...>::get()
{
    ft_bool lock_acquired;
    int32_t lock_result;
    T *reference;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::get type");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (fallback_reference<T>());
    if (this->_data == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_OPERATION);
        (void)this->unlock_internal(lock_acquired);
        return (fallback_reference<T>());
    }
    reference = &std::get<T>(*this->_data);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*reference);
}

template <typename... Types>
template <typename T>
const T& ft_tuple<Types...>::get() const
{
    ft_bool lock_acquired;
    int32_t lock_result;
    const T *reference;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "ft_tuple::get type const");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (fallback_reference<T>());
    if (this->_data == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_OPERATION);
        (void)this->unlock_internal(lock_acquired);
        return (fallback_reference<T>());
    }
    reference = &std::get<T>(*this->_data);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (*reference);
}

template <typename... Types>
void ft_tuple<Types...>::reset()
{
    ft_bool lock_acquired;
    int32_t lock_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::reset");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    this->destroy_locked();
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename... Types>
int32_t ft_tuple<Types...>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_tuple::enable_thread_safety");
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
int32_t ft_tuple<Types...>::disable_thread_safety()
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

template <typename... Types>
ft_bool ft_tuple<Types...>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_tuple::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename... Types>
int32_t ft_tuple<Types...>::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::lock");
    return (set_error(this->lock_internal(lock_acquired)));
}

template <typename... Types>
void ft_tuple<Types...>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_tuple::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename... Types>
int32_t ft_tuple<Types...>::get_error() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_tuple::get_error");
    return (_last_error);
}

template <typename... Types>
const char *ft_tuple<Types...>::get_error_str() const
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_tuple::get_error_str");
    return (ft_strerror(_last_error));
}

#endif

#ifndef SHARED_PTR_HPP
#define SHARED_PTR_HPP

#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <atomic>
#include <cstdint>
#include <new>

template <typename ManagedType>
class ft_sharedptr
{
    private:
        ManagedType                *_managed_pointer;
        std::atomic<ft_size_t>     *_reference_count;
        ft_size_t                  _array_size;
        ft_bool                    _is_array_type;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                    _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        void destroy_storage() noexcept;

    public:
        class reference_proxy
        {
            private:
                ManagedType *_pointer;
                int32_t     _error;

            public:
                reference_proxy(ManagedType *pointer, int32_t error) noexcept;
                ~reference_proxy();
                operator ManagedType&() const noexcept;
                ManagedType *operator->() const noexcept;
                int32_t get_error() const noexcept;
        };

        class const_reference_proxy
        {
            private:
                const ManagedType *_pointer;
                int32_t           _error;

            public:
                const_reference_proxy(const ManagedType *pointer,
                    int32_t error) noexcept;
                ~const_reference_proxy();
                operator const ManagedType&() const noexcept;
                const ManagedType *operator->() const noexcept;
                int32_t get_error() const noexcept;
        };

        ft_sharedptr() noexcept;
        ft_sharedptr(ManagedType *pointer, ft_bool array_type = FT_FALSE,
            ft_size_t array_size = 1) noexcept;
        explicit ft_sharedptr(ft_size_t size) noexcept;
        ft_sharedptr(const ft_sharedptr &other) noexcept = delete;
        ft_sharedptr(ft_sharedptr &&other) noexcept = delete;
        ft_sharedptr &operator=(const ft_sharedptr &other) noexcept;
        ft_sharedptr &operator=(ft_sharedptr &&other) noexcept;
        ~ft_sharedptr();

        int32_t initialize() noexcept;
        int32_t initialize(ManagedType *pointer, ft_bool array_type = FT_FALSE,
            ft_size_t array_size = 1) noexcept;
        int32_t initialize(ft_size_t size) noexcept;
        int32_t initialize(const ft_sharedptr &other) noexcept;
        int32_t initialize(ft_sharedptr &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_sharedptr<ManagedType> &other) noexcept;

        reference_proxy operator*() noexcept;
        const_reference_proxy operator*() const noexcept;
        ManagedType *operator->() noexcept;
        const ManagedType *operator->() const noexcept;
        reference_proxy operator[](ft_size_t index) noexcept;
        const_reference_proxy operator[](ft_size_t index) const noexcept;

        ManagedType *get() noexcept;
        const ManagedType *get() const noexcept;
        int32_t use_count() const noexcept;
        ft_bool unique() const noexcept;

        void reset(ManagedType *pointer = ft_nullptr, ft_size_t size = 1,
            ft_bool array_type = FT_FALSE) noexcept;
        void swap(ft_sharedptr &other) noexcept;

        operator ft_bool() const noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::lock_internal(ft_bool *lock_acquired) const noexcept
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

template <typename ManagedType>
void ft_sharedptr<ManagedType>::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return ;
    if (this->_mutex == ft_nullptr)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

template <typename ManagedType>
void ft_sharedptr<ManagedType>::destroy_storage() noexcept
{
    ft_size_t reference_count;

    if (this->_managed_pointer == ft_nullptr)
    {
        this->_reference_count = ft_nullptr;
        return ;
    }
    if (this->_reference_count == ft_nullptr)
    {
        if (this->_is_array_type)
            delete[] this->_managed_pointer;
        else
            delete this->_managed_pointer;
        this->_managed_pointer = ft_nullptr;
        this->_array_size = 0;
        this->_is_array_type = FT_FALSE;
        return ;
    }
    reference_count = this->_reference_count->fetch_sub(1,
        std::memory_order_acq_rel);
    if (reference_count > 1)
    {
        this->_managed_pointer = ft_nullptr;
        this->_reference_count = ft_nullptr;
        this->_array_size = 0;
        this->_is_array_type = FT_FALSE;
        return ;
    }
    if (this->_is_array_type)
        delete[] this->_managed_pointer;
    else
        delete this->_managed_pointer;
    delete this->_reference_count;
    this->_managed_pointer = ft_nullptr;
    this->_reference_count = ft_nullptr;
    this->_array_size = 0;
    this->_is_array_type = FT_FALSE;
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::reference_proxy::reference_proxy(ManagedType *pointer,
    int32_t error) noexcept : _pointer(pointer), _error(error)
{
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::reference_proxy::~reference_proxy()
{
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::reference_proxy::operator ManagedType&() const noexcept
{
    static ManagedType fallback = ManagedType();

    if (this->_pointer == ft_nullptr)
        return (fallback);
    return (*this->_pointer);
}

template <typename ManagedType>
ManagedType *ft_sharedptr<ManagedType>::reference_proxy::operator->() const noexcept
{
    return (this->_pointer);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::reference_proxy::get_error() const noexcept
{
    return (this->_error);
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::const_reference_proxy::const_reference_proxy(
    const ManagedType *pointer, int32_t error) noexcept : _pointer(pointer), _error(error)
{
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::const_reference_proxy::~const_reference_proxy()
{
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::const_reference_proxy::operator const ManagedType&() const noexcept
{
    static ManagedType fallback = ManagedType();

    if (this->_pointer == ft_nullptr)
        return (fallback);
    return (*this->_pointer);
}

template <typename ManagedType>
const ManagedType *ft_sharedptr<ManagedType>::const_reference_proxy::operator->() const noexcept
{
    return (this->_pointer);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::const_reference_proxy::get_error() const noexcept
{
    return (this->_error);
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::ft_sharedptr() noexcept
    : _managed_pointer(ft_nullptr), _reference_count(ft_nullptr),
      _array_size(0), _is_array_type(FT_FALSE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::ft_sharedptr(ManagedType *pointer, ft_bool array_type,
    ft_size_t array_size) noexcept
    : _managed_pointer(ft_nullptr), _reference_count(ft_nullptr),
      _array_size(0), _is_array_type(FT_FALSE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(pointer, array_type, array_size);
    (void)set_error(previous_error);
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::ft_sharedptr(ft_size_t size) noexcept
    : _managed_pointer(ft_nullptr), _reference_count(ft_nullptr),
      _array_size(0), _is_array_type(FT_FALSE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(size);
    (void)set_error(previous_error);
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType> &ft_sharedptr<ManagedType>::operator=(
    const ft_sharedptr &other) noexcept
{
    if (this == &other)
        return (*this);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        (void)this->initialize();
    this->destroy_storage();
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (*this);
    this->_managed_pointer = other._managed_pointer;
    this->_reference_count = other._reference_count;
    this->_array_size = other._array_size;
    this->_is_array_type = other._is_array_type;
    if (this->_reference_count != ft_nullptr)
        (void)this->_reference_count->fetch_add(1,
            std::memory_order_relaxed);
    set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename ManagedType>
ft_sharedptr<ManagedType> &ft_sharedptr<ManagedType>::operator=(
    ft_sharedptr &&other) noexcept
{
    if (this == &other)
        return (*this);
    (void)this->move(other);
    return (*this);
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::~ft_sharedptr()
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

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_INVALID_STATE));
    this->_managed_pointer = ft_nullptr;
    this->_reference_count = ft_nullptr;
    this->_array_size = 0;
    this->_is_array_type = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::initialize(ManagedType *pointer,
    ft_bool array_type, ft_size_t array_size) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_INVALID_STATE));
    this->_managed_pointer = pointer;
    this->_reference_count = ft_nullptr;
    if (pointer != ft_nullptr)
    {
        this->_reference_count = new (std::nothrow)
            std::atomic<ft_size_t>(1);
        if (this->_reference_count == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
    }
    if (array_type)
        this->_array_size = array_size;
    else if (pointer == ft_nullptr)
        this->_array_size = 0;
    else
        this->_array_size = 1;
    this->_is_array_type = array_type;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::initialize(ft_size_t size) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_INVALID_STATE));
    this->_managed_pointer = ft_nullptr;
    this->_reference_count = ft_nullptr;
    this->_array_size = 0;
    this->_is_array_type = FT_TRUE;
    if (size > 0)
    {
        this->_managed_pointer = new (std::nothrow) ManagedType[size];
        if (this->_managed_pointer == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
    }
    this->_array_size = size;
    if (this->_managed_pointer != ft_nullptr)
    {
        this->_reference_count = new (std::nothrow)
            std::atomic<ft_size_t>(1);
        if (this->_reference_count == ft_nullptr)
        {
            delete[] this->_managed_pointer;
            this->_managed_pointer = ft_nullptr;
            this->_array_size = 0;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_NO_MEMORY));
        }
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::initialize(const ft_sharedptr &other) noexcept
{
    int32_t destroy_result;
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_sharedptr::initialize(copy)",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
        destroy_result = this->disable_thread_safety();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_managed_pointer = ft_nullptr;
        this->_reference_count = ft_nullptr;
        this->_array_size = 0;
        this->_is_array_type = FT_FALSE;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    new_mutex = ft_nullptr;
    if (other._mutex != ft_nullptr)
    {
        new_mutex = new (std::nothrow) pt_recursive_mutex();
        if (new_mutex == ft_nullptr)
            return (set_error(FT_ERR_NO_MEMORY));
        initialize_result = new_mutex->initialize();
        if (initialize_result != FT_ERR_SUCCESS)
        {
            delete new_mutex;
            return (set_error(initialize_result));
        }
    }
    this->_managed_pointer = other._managed_pointer;
    this->_reference_count = other._reference_count;
    this->_array_size = other._array_size;
    this->_is_array_type = other._is_array_type;
    this->_mutex = new_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (this->_reference_count != ft_nullptr)
        (void)this->_reference_count->fetch_add(1,
            std::memory_order_relaxed);
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::initialize(ft_sharedptr &&other) noexcept
{
    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    return (this->move(other));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::destroy() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (lock_result);
    this->destroy_storage();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->unlock_internal(lock_acquired);
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::move(ft_sharedptr<ManagedType> &other) noexcept
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_sharedptr::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
        destroy_result = this->disable_thread_safety();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_managed_pointer = ft_nullptr;
        this->_reference_count = ft_nullptr;
        this->_array_size = 1;
        this->_is_array_type = FT_FALSE;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_managed_pointer = other._managed_pointer;
    this->_reference_count = other._reference_count;
    this->_array_size = other._array_size;
    this->_is_array_type = other._is_array_type;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._managed_pointer = ft_nullptr;
    other._reference_count = ft_nullptr;
    other._array_size = 1;
    other._is_array_type = FT_FALSE;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ManagedType>
typename ft_sharedptr<ManagedType>::reference_proxy ft_sharedptr<ManagedType>::operator*() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_managed_pointer == ft_nullptr)
        return (reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_POINTER)));
    return (reference_proxy(this->_managed_pointer, set_error(FT_ERR_SUCCESS)));
}

template <typename ManagedType>
typename ft_sharedptr<ManagedType>::const_reference_proxy ft_sharedptr<ManagedType>::operator*() const
    noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (const_reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_managed_pointer == ft_nullptr)
        return (const_reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_POINTER)));
    return (const_reference_proxy(this->_managed_pointer, set_error(FT_ERR_SUCCESS)));
}

template <typename ManagedType>
ManagedType *ft_sharedptr<ManagedType>::operator->() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    if (this->_managed_pointer == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_POINTER);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer);
}

template <typename ManagedType>
const ManagedType *ft_sharedptr<ManagedType>::operator->() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    if (this->_managed_pointer == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_POINTER);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer);
}

template <typename ManagedType>
typename ft_sharedptr<ManagedType>::reference_proxy ft_sharedptr<ManagedType>::operator[](
    ft_size_t index) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_is_array_type == FT_FALSE || this->_managed_pointer == ft_nullptr)
        return (reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_OPERATION)));
    if (index >= this->_array_size)
        return (reference_proxy(ft_nullptr, set_error(FT_ERR_OUT_OF_RANGE)));
    return (reference_proxy(&this->_managed_pointer[index], set_error(FT_ERR_SUCCESS)));
}

template <typename ManagedType>
typename ft_sharedptr<ManagedType>::const_reference_proxy ft_sharedptr<ManagedType>::operator[](
    ft_size_t index) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (const_reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_STATE)));
    if (this->_is_array_type == FT_FALSE || this->_managed_pointer == ft_nullptr)
        return (const_reference_proxy(ft_nullptr, set_error(FT_ERR_INVALID_OPERATION)));
    if (index >= this->_array_size)
        return (const_reference_proxy(ft_nullptr, set_error(FT_ERR_OUT_OF_RANGE)));
    return (const_reference_proxy(&this->_managed_pointer[index], set_error(FT_ERR_SUCCESS)));
}

template <typename ManagedType>
ManagedType *ft_sharedptr<ManagedType>::get() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer);
}

template <typename ManagedType>
const ManagedType *ft_sharedptr<ManagedType>::get() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::use_count() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (0);
    }
    if (this->_managed_pointer == ft_nullptr)
    {
        set_error(FT_ERR_SUCCESS);
        return (0);
    }
    if (this->_reference_count == ft_nullptr)
        return (1);
    set_error(FT_ERR_SUCCESS);
    return (static_cast<int32_t>(this->_reference_count->load(
        std::memory_order_acquire)));
}

template <typename ManagedType>
ft_bool ft_sharedptr<ManagedType>::unique() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer != ft_nullptr);
}

template <typename ManagedType>
void ft_sharedptr<ManagedType>::reset(ManagedType *pointer, ft_size_t size,
    ft_bool array_type) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    this->destroy_storage();
    this->_managed_pointer = pointer;
    this->_reference_count = ft_nullptr;
    if (pointer != ft_nullptr)
    {
        this->_reference_count = new (std::nothrow)
            std::atomic<ft_size_t>(1);
        if (this->_reference_count == ft_nullptr)
        {
            this->_managed_pointer = ft_nullptr;
            this->_array_size = 0;
            this->_is_array_type = FT_FALSE;
            set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    if (array_type)
        this->_array_size = size;
    else if (pointer == ft_nullptr)
        this->_array_size = 0;
    else
        this->_array_size = 1;
    this->_is_array_type = array_type;
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ManagedType>
void ft_sharedptr<ManagedType>::swap(ft_sharedptr &other) noexcept
{
    ManagedType *pointer_value;
    ft_size_t array_size_value;
    ft_bool is_array_type_value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    pointer_value = this->_managed_pointer;
    ft_size_t *reference_count_value;
    array_size_value = this->_array_size;
    is_array_type_value = this->_is_array_type;
    reference_count_value = this->_reference_count;
    this->_managed_pointer = other._managed_pointer;
    this->_reference_count = other._reference_count;
    this->_array_size = other._array_size;
    this->_is_array_type = other._is_array_type;
    other._managed_pointer = pointer_value;
    other._reference_count = reference_count_value;
    other._array_size = array_size_value;
    other._is_array_type = is_array_type_value;
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ManagedType>
ft_sharedptr<ManagedType>::operator ft_bool() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        set_error(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_managed_pointer != ft_nullptr);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::enable_thread_safety() noexcept
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

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

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

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

template <typename ManagedType>
ft_bool ft_sharedptr<ManagedType>::is_thread_safe() const noexcept
{
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::lock(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_result;

    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename ManagedType>
void ft_sharedptr<ManagedType>::unlock(ft_bool lock_acquired) const noexcept
{
    this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ManagedType>
int32_t ft_sharedptr<ManagedType>::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sharedptr::get_error");
    return (_last_error);
}

template <typename ManagedType>
const char *ft_sharedptr<ManagedType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_sharedptr::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename ManagedType>
thread_local int32_t ft_sharedptr<ManagedType>::_last_error = FT_ERR_SUCCESS;

template <typename LeftType, typename RightType>
ft_bool operator==(const ft_sharedptr<LeftType> &left,
    const ft_sharedptr<RightType> &right)
{
    return (left.get() == right.get());
}

template <typename LeftType, typename RightType>
ft_bool operator!=(const ft_sharedptr<LeftType> &left,
    const ft_sharedptr<RightType> &right)
{
    return (left.get() != right.get());
}

#endif

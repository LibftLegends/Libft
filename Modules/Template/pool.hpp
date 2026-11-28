#ifndef POOL_HPP
#define POOL_HPP

#include <utility>
#include "vector.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"

template<typename T>
class Pool
{
    private:
        ft_vector<T*>               _slots;
        ft_vector<ft_size_t>        _free_indices;
        mutable pt_recursive_mutex  *_mutex;
        uint8_t                     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        void release(ft_size_t index) noexcept;
        int32_t resize_unlocked(ft_size_t new_size);

    public:
        class Object
        {
            private:
                Pool<T>                  *_pool;
                ft_size_t                _index;
                T                        *_pointer;
                mutable pt_recursive_mutex *_mutex;
                uint8_t                  _initialised_state;

                static thread_local int32_t _last_error;

                static int32_t set_error(int32_t error_code) noexcept;
                int32_t lock_internal(ft_bool *lock_acquired) const;
                int32_t unlock_internal(ft_bool lock_acquired) const;

            public:
                Object() noexcept;
                Object(Pool<T>* pool, ft_size_t index, T* pointer) noexcept;
                ~Object() noexcept;

                Object(const Object&) = delete;
                Object& operator=(const Object&) = delete;

                Object(Object&& other) noexcept;
                Object& operator=(Object&& other) noexcept;

                T* operator->() const noexcept;
                operator ft_bool() const noexcept;

                int32_t enable_thread_safety();
                int32_t disable_thread_safety();
                ft_bool is_thread_safe() const;
                int32_t lock(ft_bool *lock_acquired) const;
                void unlock(ft_bool lock_acquired) const;

                int32_t get_error() const noexcept;
                const char *get_error_str() const noexcept;
        };

        Pool();
        ~Pool();

        Pool(Pool&& other) = delete;
        Pool& operator=(Pool&& other) = delete;
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;

        int32_t initialize();
        int32_t destroy();
        int32_t move(Pool<T> &other);

        void resize(ft_size_t new_size);

        template<typename... Args>
        Object acquire(Args&&... args);

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template<typename T>
int32_t Pool<T>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template<typename T>
int32_t Pool<T>::lock_internal(ft_bool *lock_acquired) const
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

template<typename T>
int32_t Pool<T>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template<typename T>
void Pool<T>::release(ft_size_t index) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    T *pointer_to_release;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    pointer_to_release = ft_nullptr;
    if (index < this->_slots.size())
    {
        pointer_to_release = this->_slots[index];
        if (pointer_to_release != ft_nullptr)
        {
            this->_slots[index] = ft_nullptr;
            this->_free_indices.push_back(index);
        }
    }
    (void)this->unlock_internal(lock_acquired);
    if (pointer_to_release != ft_nullptr)
        cma_free(pointer_to_release);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template<typename T>
int32_t Pool<T>::Object::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template<typename T>
int32_t Pool<T>::Object::lock_internal(ft_bool *lock_acquired) const
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

template<typename T>
int32_t Pool<T>::Object::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template<typename T>
Pool<T>::Object::Object() noexcept
    : _pool(ft_nullptr), _index(0), _pointer(ft_nullptr),
      _mutex(ft_nullptr), _initialised_state(2)
{
    return ;
}

template<typename T>
Pool<T>::Object::Object(Pool<T>* pool, ft_size_t index, T* pointer) noexcept
    : _pool(pool), _index(index), _pointer(pointer), _mutex(ft_nullptr),
      _initialised_state(2)
{
    return ;
}

template<typename T>
Pool<T>::Object::~Object() noexcept
{
    uint32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state != 2)
    {
        (void)set_error(previous_error);
        return ;
    }
    if (this->_pointer != ft_nullptr)
    {
        this->_pointer->~T();
        if (this->_pool != ft_nullptr)
            this->_pool->release(this->_index);
    }
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    this->_pool = ft_nullptr;
    this->_pointer = ft_nullptr;
    this->_index = 0;
    this->_initialised_state = 1;
    (void)set_error(previous_error);
    return ;
}

template<typename T>
Pool<T>::Object::Object(Object&& other) noexcept
    : _pool(other._pool), _index(other._index), _pointer(other._pointer),
      _mutex(other._mutex), _initialised_state(other._initialised_state)
{
    other._pool = ft_nullptr;
    other._index = 0;
    other._pointer = ft_nullptr;
    other._mutex = ft_nullptr;
    other._initialised_state = 1;
    return ;
}

template<typename T>
typename Pool<T>::Object& Pool<T>::Object::operator=(Object&& other) noexcept
{
    if (this == &other)
        return (*this);
    if (this->_pointer != ft_nullptr)
    {
        this->_pointer->~T();
        if (this->_pool != ft_nullptr)
            this->_pool->release(this->_index);
    }
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    this->_pool = other._pool;
    this->_index = other._index;
    this->_pointer = other._pointer;
    this->_mutex = other._mutex;
    this->_initialised_state = other._initialised_state;
    other._pool = ft_nullptr;
    other._index = 0;
    other._pointer = ft_nullptr;
    other._mutex = ft_nullptr;
    other._initialised_state = 1;
    set_error(FT_ERR_SUCCESS);
    return (*this);
}

template<typename T>
T* Pool<T>::Object::operator->() const noexcept
{
    if (this->_pointer == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_HANDLE);
        return (ft_nullptr);
    }
    set_error(FT_ERR_SUCCESS);
    return (this->_pointer);
}

template<typename T>
Pool<T>::Object::operator ft_bool() const noexcept
{
    if (this->_pointer == ft_nullptr)
    {
        set_error(FT_ERR_INVALID_HANDLE);
        return (FT_FALSE);
    }
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template<typename T>
int32_t Pool<T>::Object::enable_thread_safety()
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

template<typename T>
int32_t Pool<T>::Object::disable_thread_safety()
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

template<typename T>
ft_bool Pool<T>::Object::is_thread_safe() const
{
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template<typename T>
int32_t Pool<T>::Object::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template<typename T>
void Pool<T>::Object::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template<typename T>
int32_t Pool<T>::Object::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "Pool::Object::get_error");
    return (_last_error);
}

template<typename T>
const char *Pool<T>::Object::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "Pool::Object::get_error_str");
    return (ft_strerror(_last_error));
}

template<typename T>
Pool<T>::Pool()
    : _slots(), _free_indices(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template<typename T>
Pool<T>::~Pool()
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

template<typename T>
int32_t Pool<T>::initialize()
{
    int32_t slots_initialize_error;
    int32_t free_indices_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "Pool::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        (void)this->_slots.destroy();
        (void)this->_free_indices.destroy();
    }
    slots_initialize_error = this->_slots.initialize();
    if (slots_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(static_cast<uint32_t>(slots_initialize_error)));
    }
    free_indices_initialize_error = this->_free_indices.initialize();
    if (free_indices_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_slots.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(static_cast<uint32_t>(free_indices_initialize_error)));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template<typename T>
int32_t Pool<T>::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t slots_destroy_error;
    int32_t free_indices_destroy_error;
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
    ft_size_t index;

    index = 0;
    while (index < this->_slots.size())
    {
        if (this->_slots[index] != ft_nullptr)
        {
            this->_slots[index]->~T();
            cma_free(this->_slots[index]);
            this->_slots[index] = ft_nullptr;
        }
        ++index;
    }
    slots_destroy_error = this->_slots.destroy();
    if (slots_destroy_error != FT_ERR_SUCCESS)
        first_error = slots_destroy_error;
    free_indices_destroy_error = this->_free_indices.destroy();
    if (first_error == FT_ERR_SUCCESS
        && free_indices_destroy_error != FT_ERR_SUCCESS)
        first_error = free_indices_destroy_error;
    (void)this->unlock_internal(lock_acquired);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template<typename T>
int32_t Pool<T>::move(Pool<T> &other)
{
    int32_t destroy_result;
    int32_t slots_move_result;
    int32_t free_indices_move_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "Pool::move",
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
        (void)this->_slots.move(other._slots);
        (void)this->_free_indices.move(other._free_indices);
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    slots_move_result = this->_slots.move(other._slots);
    if (slots_move_result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(slots_move_result));
    }
    free_indices_move_result = this->_free_indices.move(other._free_indices);
    if (free_indices_move_result != FT_ERR_SUCCESS)
    {
        (void)this->_slots.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(free_indices_move_result));
    }
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template<typename T>
int32_t Pool<T>::resize_unlocked(ft_size_t new_size)
{
    const ft_size_t old_size = this->_slots.size();
    if (new_size == old_size)
        return (FT_ERR_SUCCESS);
    if (new_size < old_size)
    {
        ft_size_t index = new_size;
        while (index < old_size)
        {
            if (this->_slots[index] != ft_nullptr)
                return (FT_ERR_INVALID_OPERATION);
            ++index;
        }
        const ft_size_t free_count = this->_free_indices.size();
        ft_size_t retained_count = 0U;
        index = 0U;
        while (index < free_count)
        {
            const ft_size_t free_index = this->_free_indices[index++];
            if (free_index < new_size)
                this->_free_indices[retained_count++] = free_index;
        }
        this->_free_indices.resize(retained_count);
        this->_slots.resize(new_size, ft_nullptr);
        return (FT_ERR_SUCCESS);
    }
    constexpr ft_size_t MAXIMUM_SLOTS = FT_SYSTEM_SIZE_MAX / sizeof(T *);
    constexpr ft_size_t MAXIMUM_INDICES = FT_SYSTEM_SIZE_MAX / sizeof(ft_size_t);
    if (new_size > MAXIMUM_SLOTS || new_size > MAXIMUM_INDICES)
        return (FT_ERR_NO_MEMORY);
    const ft_size_t current_capacity = this->_slots.capacity();
    ft_size_t capacity = new_size;
    if (current_capacity < new_size && current_capacity <= MAXIMUM_SLOTS / 2U
        && current_capacity <= MAXIMUM_INDICES / 2U && current_capacity * 2U > new_size)
        capacity = current_capacity * 2U;
    this->_free_indices.reserve(capacity);
    int32_t error = this->_free_indices.get_error();
    if (error != FT_ERR_SUCCESS)
        return (error);
    this->_slots.reserve(capacity);
    error = this->_slots.get_error();
    if (error != FT_ERR_SUCCESS)
        return (error);
    this->_slots.resize(new_size, ft_nullptr);
    error = this->_slots.get_error();
    if (error != FT_ERR_SUCCESS)
        return (error);
    ft_size_t index = old_size;
    while (index < new_size)
    {
        this->_free_indices.push_back(index);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

template<typename T>
void Pool<T>::resize(ft_size_t new_size)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "Pool::resize");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    const int32_t resize_error = this->resize_unlocked(new_size);
    (void)this->unlock_internal(lock_acquired);
    set_error(resize_error);
    return ;
}

template<typename T>
template<typename... Args>
typename Pool<T>::Object Pool<T>::acquire(Args&&... args)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t free_count;
    ft_size_t last_index;
    ft_size_t storage_index;
    T *constructed_pointer;
    Object result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "Pool::acquire");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (result);
    }
    free_count = this->_free_indices.size();
    if (free_count == 0)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_EMPTY);
        return (result);
    }
    last_index = free_count - 1;
    storage_index = this->_free_indices[last_index];
    constructed_pointer = static_cast<T*>(cma_malloc(sizeof(T)));
    if (constructed_pointer == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (result);
    }
    this->_free_indices.pop_back();
    construct_at(constructed_pointer, std::forward<Args>(args)...);
    this->_slots[storage_index] = constructed_pointer;
    result = Object(this, storage_index, constructed_pointer);
    if (this->_mutex != ft_nullptr)
        (void)result.enable_thread_safety();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (result);
}

template<typename T>
int32_t Pool<T>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "Pool::enable_thread_safety");
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

template<typename T>
int32_t Pool<T>::disable_thread_safety()
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

template<typename T>
ft_bool Pool<T>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "Pool::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template<typename T>
int32_t Pool<T>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised(this->_initialised_state, "Pool::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template<typename T>
void Pool<T>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template<typename T>
int32_t Pool<T>::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "Pool::get_error");
    return (_last_error);
}

template<typename T>
const char *Pool<T>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "Pool::get_error_str");
    return (ft_strerror(_last_error));
}

template<typename T>
thread_local int32_t Pool<T>::_last_error = FT_ERR_SUCCESS;

template<typename T>
thread_local int32_t Pool<T>::Object::_last_error = FT_ERR_SUCCESS;

#endif

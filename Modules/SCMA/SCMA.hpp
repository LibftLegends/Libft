#ifndef SCMA_HPP
# define SCMA_HPP

#include <cstdint>
#include <type_traits>
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

static inline void scma_secure_bzero(void *memory_pointer, ft_size_t size)
{
    volatile unsigned char *pointer;

    if (memory_pointer == nullptr || size == 0)
        return ;
    pointer = static_cast<volatile unsigned char *>(memory_pointer);
    while (size > 0)
    {
        *pointer = 0;
        pointer++;
        size--;
    }
    return ;
}


struct scma_handle
{
    ft_size_t    index;
    ft_size_t    generation;
};

scma_handle    scma_invalid_handle(void);

int32_t    scma_initialize(ft_size_t initial_capacity);
void    scma_shutdown(void);
int32_t     scma_is_initialised(void);

scma_handle    scma_allocate(ft_size_t size);
int32_t    scma_free(scma_handle handle);
int32_t    scma_resize(scma_handle handle, ft_size_t new_size);
ft_size_t    scma_get_size(scma_handle handle);
int32_t     scma_handle_is_valid(scma_handle handle);

int32_t    scma_write(scma_handle handle, ft_size_t offset,
            const void *source, ft_size_t size);
int32_t    scma_read(scma_handle handle, ft_size_t offset,
            void *destination, ft_size_t size);

struct scma_write_request
{
    scma_handle handle;
    ft_size_t offset;
    const void *source;
    ft_size_t size;
};

struct scma_read_request
{
    scma_handle handle;
    ft_size_t offset;
    void *destination;
    ft_size_t size;
};

int32_t    scma_write_batch(const scma_write_request *requests,
            ft_size_t request_count);
int32_t    scma_read_batch(const scma_read_request *requests,
            ft_size_t request_count);

struct scma_stats
{
    ft_size_t    block_count;
    ft_size_t    used_size;
    ft_size_t    heap_capacity;
};

int32_t     scma_get_stats(scma_stats *out_stats);

void    scma_debug_dump(void);

#ifdef LIBFT_TEST_BUILD
struct scma_leak_entry
{
    scma_handle handle;
    ft_size_t offset;
    ft_size_t size;
    ft_size_t generation;
};

struct scma_leak_summary
{
    ft_size_t live_block_count;
    ft_size_t live_bytes;
    ft_size_t ignored_block_count;
    ft_size_t ignored_bytes;
};

int32_t scma_get_leak_summary(scma_leak_summary *out_summary);
int32_t scma_get_leak_entries(scma_leak_entry *entries, ft_size_t capacity,
    ft_size_t *entry_count);
int32_t scma_report_leaks(void);
int32_t scma_untrack_leak(scma_handle handle);
int32_t scma_track_leak(scma_handle handle);
#endif

pt_recursive_mutex    *scma_runtime_mutex(void);
int32_t     scma_enable_thread_safety(void);
int32_t     scma_disable_thread_safety(void);
int32_t     scma_enable_thread_safety_timed(uint64_t timeout_ms);
int32_t     scma_disable_thread_safety_timed(uint64_t timeout_ms);
int32_t     scma_try_set_thread_safety(ft_bool enable);
ft_bool    scma_is_thread_safe_enabled(void);
int32_t     scma_mutex_lock(void);
int32_t     scma_mutex_unlock(void);
int32_t     scma_mutex_close(void);
ft_size_t    scma_mutex_lock_count(void);

template <typename TValue>
class scma_handle_accessor_element_proxy;

template <typename TValue>
class scma_handle_accessor_const_element_proxy;

template <typename TValue>
class scma_handle_accessor
{
 #ifdef LIBFT_TEST_BUILD
    public:
 #else
    private:
 #endif
        scma_handle _handle;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        int32_t     set_error(int32_t error_code) const;

    public:
        scma_handle_accessor(void);
        scma_handle_accessor(scma_handle handle) = delete;
        scma_handle_accessor(const scma_handle_accessor &other) = delete;
        scma_handle_accessor(scma_handle_accessor &&other) = delete;
        ~scma_handle_accessor(void);
        scma_handle_accessor &operator=(const scma_handle_accessor &other) = delete;
        scma_handle_accessor &operator=(scma_handle_accessor &&other) = delete;

        int32_t    initialize(void);
        int32_t    initialize(scma_handle handle);
        int32_t     move(scma_handle_accessor &other);
        int32_t     destroy(void);
        int32_t     enable_thread_safety(void);
        int32_t     disable_thread_safety(void);
        ft_bool        is_thread_safe(void) const;
        int32_t     is_initialised(void) const;
        int32_t     bind(scma_handle handle);
        int32_t     is_bound(void) const;
        scma_handle    get_handle(void) const;

        scma_handle_accessor_element_proxy<TValue>          operator*(void);
        scma_handle_accessor_const_element_proxy<TValue>    operator*(void) const;
        scma_handle_accessor_element_proxy<TValue>          operator->(void);
        scma_handle_accessor_const_element_proxy<TValue>    operator->(void) const;
        scma_handle_accessor_element_proxy<TValue>
                                            operator[](ft_size_t element_index);
        scma_handle_accessor_const_element_proxy<TValue>
                                            operator[](ft_size_t element_index) const;

        int32_t     read_struct(TValue &destination) const;
        int32_t     write_struct(const TValue &source) const;
        int32_t     read_at(TValue &destination, ft_size_t element_index) const;
        int32_t     write_at(const TValue &source, ft_size_t element_index) const;
        ft_size_t    get_count(void) const;
        int32_t     get_error(void) const;
        const char  *get_error_str(void) const;

};

template <typename TValue>
thread_local int32_t scma_handle_accessor<TValue>::_last_error = FT_ERR_SUCCESS;

template <typename TValue>
inline scma_handle_accessor<TValue>::scma_handle_accessor(void)
{
    this->_handle.index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    this->_handle.generation = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

template <typename TValue>
inline scma_handle_accessor<TValue>::~scma_handle_accessor(void)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t previous_error;

        previous_error = scma_handle_accessor<TValue>::_last_error;
        (void)this->destroy();
        scma_handle_accessor<TValue>::_last_error = previous_error;
    }
    return ;
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::initialize(void)
{
    int32_t lock_result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "scma_handle_accessor::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    lock_result = scma_mutex_lock();
    if (lock_result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    this->_handle.index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    this->_handle.generation = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    (void)scma_mutex_unlock();
    return (FT_ERR_SUCCESS);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::initialize(scma_handle handle)
{
    int32_t initialization_error;

    initialization_error = this->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
        return (initialization_error);
    if (this->bind(handle))
        return (FT_ERR_SUCCESS);
    int32_t bind_error = this->get_error();
    (void)this->destroy();
    this->set_error(bind_error);
    return (bind_error);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::move(
        scma_handle_accessor &other)
{
    int32_t destroy_result;
    int32_t source_error;

    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "scma_handle_accessor::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    source_error = other.get_error();
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_result);
            return (destroy_result);
        }
    }
    this->_handle = scma_invalid_handle();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    this->_handle = other._handle;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._handle = scma_invalid_handle();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::destroy(void)
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    scma_secure_bzero(&this->_handle, sizeof(this->_handle));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    (void)scma_mutex_unlock();
    return (this->get_error());
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::enable_thread_safety(void)
{
    int32_t operation_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::enable_thread_safety");
    operation_error = scma_enable_thread_safety();
    this->set_error(operation_error);
    return (operation_error);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::disable_thread_safety(void)
{
    int32_t operation_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::disable_thread_safety");
    operation_error = scma_disable_thread_safety();
    this->set_error(operation_error);
    return (operation_error);
}

template <typename TValue>
inline ft_bool    scma_handle_accessor<TValue>::is_thread_safe(void) const
{
    ft_bool is_enabled;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::is_thread_safe");
    is_enabled = scma_is_thread_safe_enabled();
    this->set_error(FT_ERR_SUCCESS);
    return (is_enabled);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::is_initialised(void) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::is_initialised");
    this->set_error(FT_ERR_SUCCESS);
    return (1);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::bind(scma_handle handle)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::bind");
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (0);
    }
    if (!scma_handle_is_valid(handle))
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        (void)scma_mutex_unlock();
        return (0);
    }
    this->_handle = handle;
    this->set_error(FT_ERR_SUCCESS);
    (void)scma_mutex_unlock();
    return (1);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::is_bound(void) const
{
    int32_t is_bound_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::is_bound");
    is_bound_result = 0;
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (0);
    }
    if (this->_handle.index == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        is_bound_result = 0;
    else if (this->_handle.generation == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        is_bound_result = 0;
    else
        is_bound_result = 1;
    (void)scma_mutex_unlock();
    this->set_error(FT_ERR_SUCCESS);
    return (is_bound_result);
}

template <typename TValue>
inline scma_handle    scma_handle_accessor<TValue>::get_handle(void) const
{
    scma_handle handle;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "scma_handle_accessor::get_handle");
    handle.index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    handle.generation = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (handle);
    }
    handle = this->_handle;
    (void)scma_mutex_unlock();
    this->set_error(FT_ERR_SUCCESS);
    return (handle);
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>    scma_handle_accessor<TValue>::operator*(void)
{
    return (scma_handle_accessor_element_proxy<TValue>(this, 0));
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>    scma_handle_accessor<TValue>::operator*(void) const
{
    return (scma_handle_accessor_const_element_proxy<TValue>(this, 0));
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>    scma_handle_accessor<TValue>::operator->(void)
{
    return (scma_handle_accessor_element_proxy<TValue>(this, 0));
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>    scma_handle_accessor<TValue>::operator->(void) const
{
    return (scma_handle_accessor_const_element_proxy<TValue>(this, 0));
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>    scma_handle_accessor<TValue>::operator[](ft_size_t element_index)
{
    return (scma_handle_accessor_element_proxy<TValue>(this, element_index));
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>    scma_handle_accessor<TValue>::operator[](ft_size_t element_index) const
{
    return (scma_handle_accessor_const_element_proxy<TValue>(this, element_index));
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::read_struct(TValue &destination) const
{
    int32_t read_result;
    ft_size_t required_size;
    ft_size_t host_size;
    ft_size_t block_size;

    read_result = 0;
    if (!this->is_initialised())
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (!std::is_trivially_copyable<TValue>::value)
    {
        this->set_error(FT_ERR_UNSUPPORTED_TYPE);
        goto cleanup;
    }
    if (!this->is_bound())
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    if (!scma_handle_is_valid(this->_handle))
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    host_size = sizeof(TValue);
    if (host_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    required_size = host_size;
    block_size = scma_get_size(this->_handle);
    if (block_size < required_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    if (!scma_read(this->_handle, 0, &destination, required_size))
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        goto cleanup;
    }
    this->set_error(FT_ERR_SUCCESS);
    read_result = 1;

cleanup:
    (void)scma_mutex_unlock();
    return (read_result);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::write_struct(const TValue &source) const
{
    int32_t write_result;
    ft_size_t required_size;
    ft_size_t host_size;
    ft_size_t block_size;

    write_result = 0;
    if (!this->is_initialised())
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (0);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (0);
    }
    if (!std::is_trivially_copyable<TValue>::value)
    {
        this->set_error(FT_ERR_UNSUPPORTED_TYPE);
        goto cleanup;
    }
    if (!this->is_bound())
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    host_size = sizeof(TValue);
    if (host_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    required_size = host_size;
    block_size = scma_get_size(this->_handle);
    if (block_size < required_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    if (scma_write(this->_handle, 0, &source, required_size) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        goto cleanup;
    }
    this->set_error(FT_ERR_SUCCESS);
    write_result = FT_ERR_SUCCESS;

cleanup:
    (void)scma_mutex_unlock();
    return (write_result);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::read_at
                (TValue &destination, ft_size_t element_index) const
{
    int32_t read_result;
    ft_size_t offset;
    ft_size_t element_size;
    ft_size_t required_size;
    ft_size_t host_size;
    ft_size_t block_size;

    read_result = FT_ERR_INVALID_STATE;
    if (!this->is_initialised())
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (!std::is_trivially_copyable<TValue>::value)
    {
        this->set_error(FT_ERR_UNSUPPORTED_TYPE);
        goto cleanup;
    }
    if (!this->is_bound())
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    host_size = sizeof(TValue);
    if (host_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    element_size = host_size;
    if (element_size == 0 || element_index
            > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) / element_size)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        goto cleanup;
    }
    offset = element_index * element_size;
    if (offset == 0 && element_index != 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (offset > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - element_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    required_size = offset + element_size;
    block_size = scma_get_size(this->_handle);
    if (block_size < required_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    if (scma_read(this->_handle, offset, &destination, element_size) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        goto cleanup;
    }
    this->set_error(FT_ERR_SUCCESS);
    read_result = FT_ERR_SUCCESS;

cleanup:
    (void)scma_mutex_unlock();
    return (read_result);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::write_at
                    (const TValue &source, ft_size_t element_index) const
{
    int32_t write_result;
    ft_size_t offset;
    ft_size_t element_size;
    ft_size_t required_size;
    ft_size_t host_size;
    ft_size_t block_size;

    write_result = FT_ERR_INVALID_STATE;
    if (!this->is_initialised())
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (!std::is_trivially_copyable<TValue>::value)
    {
        this->set_error(FT_ERR_UNSUPPORTED_TYPE);
        goto cleanup;
    }
    if (!this->is_bound())
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    host_size = sizeof(TValue);
    if (host_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    element_size = host_size;
    if (element_size == 0 || element_index
            > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) / element_size)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        goto cleanup;
    }
    offset = element_index * element_size;
    if (offset == 0 && element_index != 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (offset > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - element_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    required_size = offset + element_size;
    block_size = scma_get_size(this->_handle);
    if (block_size < required_size)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    if (scma_write(this->_handle, offset, &source, element_size) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        goto cleanup;
    }
    this->set_error(FT_ERR_SUCCESS);
    write_result = FT_ERR_SUCCESS;

cleanup:
    (void)scma_mutex_unlock();
    return (write_result);
}

template <typename TValue>
inline ft_size_t    scma_handle_accessor<TValue>::get_count(void) const
{
    ft_size_t block_size;
    ft_size_t element_size;
    ft_size_t element_count;
    ft_size_t host_size;

    element_count = 0;
    if (!this->is_initialised())
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (0);
    }
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED);
        return (0);
    }
    if (!std::is_trivially_copyable<TValue>::value)
    {
        this->set_error(FT_ERR_UNSUPPORTED_TYPE);
        goto cleanup;
    }
    if (!this->is_bound())
    {
        this->set_error(FT_ERR_INVALID_HANDLE);
        goto cleanup;
    }
    host_size = sizeof(TValue);
    if (host_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        goto cleanup;
    }
    element_size = host_size;
    block_size = scma_get_size(this->_handle);
    if (element_size == 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        goto cleanup;
    }
    this->set_error(FT_ERR_SUCCESS);
    element_count = block_size / element_size;

cleanup:
    (void)scma_mutex_unlock();
    return (element_count);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::set_error(int32_t error_code) const
{
    scma_handle_accessor<TValue>::_last_error = error_code;
    return (error_code);
}

template <typename TValue>
inline int32_t    scma_handle_accessor<TValue>::get_error(void) const
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "scma_handle_accessor::get_error",
            "called while object is uninitialised");
    }
    return (scma_handle_accessor<TValue>::_last_error);
}

template <typename TValue>
inline const char *scma_handle_accessor<TValue>::get_error_str(void) const
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "scma_handle_accessor::get_error_str",
            "called while object is uninitialised");
    }
    return (ft_strerror(scma_handle_accessor<TValue>::_last_error));
}

template <typename TValue>
class scma_handle_accessor_element_proxy
{
 #ifdef LIBFT_TEST_BUILD
    public:
 #else
    private:
 #endif
        scma_handle_accessor<TValue> *_parent;
        ft_size_t _index;
        TValue _value;
        int32_t _should_write_back;
        mutable uint32_t _error;
        int32_t _is_valid;
        int32_t set_error(int32_t error_code);

    public:
        scma_handle_accessor_element_proxy(void);
        scma_handle_accessor_element_proxy(scma_handle_accessor<TValue> *parent, ft_size_t element_index);
        scma_handle_accessor_element_proxy(const scma_handle_accessor_element_proxy &other);
        scma_handle_accessor_element_proxy    &operator=(const scma_handle_accessor_element_proxy &other) = delete;
        scma_handle_accessor_element_proxy(scma_handle_accessor_element_proxy &&other);
        scma_handle_accessor_element_proxy    &operator=(scma_handle_accessor_element_proxy &&other) = delete;
        ~scma_handle_accessor_element_proxy(void);

        TValue    *operator->(void);
        TValue    &operator*(void);
        /* Required for proxy write-back semantics: *proxy = value. */
        scma_handle_accessor_element_proxy    &operator=(const TValue &source);
        operator TValue(void) const;
        int32_t get_error(void) const;
        const char *get_error_str(void) const;
        int32_t is_valid(void) const;

};

template <typename TValue>
class scma_handle_accessor_const_element_proxy
{
 #ifdef LIBFT_TEST_BUILD
    public:
 #else
    private:
 #endif
        const scma_handle_accessor<TValue> *_parent;
        ft_size_t _index;
        mutable TValue _value;
        mutable uint32_t _error;
        int32_t _is_valid;
        int32_t set_error(int32_t error_code) const;

    public:
        scma_handle_accessor_const_element_proxy(void);
        scma_handle_accessor_const_element_proxy(const scma_handle_accessor<TValue> *parent, ft_size_t element_index);
        scma_handle_accessor_const_element_proxy(const scma_handle_accessor_const_element_proxy &other);
        scma_handle_accessor_const_element_proxy    &operator=(const scma_handle_accessor_const_element_proxy &other) = delete;
        scma_handle_accessor_const_element_proxy(scma_handle_accessor_const_element_proxy &&other);
        scma_handle_accessor_const_element_proxy    &operator=(scma_handle_accessor_const_element_proxy &&other) = delete;
        ~scma_handle_accessor_const_element_proxy(void);

        const TValue    *operator->(void) const;
        const TValue    &operator*(void) const;
        operator TValue(void) const;
        int32_t get_error(void) const;
        const char *get_error_str(void) const;
        int32_t is_valid(void) const;
};

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::scma_handle_accessor_element_proxy(void)
{
    this->_parent = ft_nullptr;
    this->_index = 0;
    this->_value = TValue();
    this->_should_write_back = 0;
    this->_error = FT_ERR_INVALID_STATE;
    this->_is_valid = 0;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::scma_handle_accessor_element_proxy(scma_handle_accessor<TValue> *parent, ft_size_t element_index)
{
    this->_parent = parent;
    this->_index = element_index;
    this->_should_write_back = 0;
    this->_error = FT_ERR_INVALID_STATE;
    this->_is_valid = 0;
    this->_value = TValue();
    if (!this->_parent)
    {
        this->set_error(FT_ERR_INVALID_POINTER);
        return ;
    }
    if (this->_parent->read_at(this->_value, this->_index) != FT_ERR_SUCCESS)
    {
        this->set_error(this->_parent->get_error());
        this->_parent = ft_nullptr;
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->_is_valid = 1;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::scma_handle_accessor_element_proxy(
        const scma_handle_accessor_element_proxy &other)
{
    this->_parent = other._parent;
    this->_index = other._index;
    this->_value = other._value;
    this->_should_write_back = 0;
    this->_error = other._error;
    this->_is_valid = other._is_valid;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::scma_handle_accessor_element_proxy(scma_handle_accessor_element_proxy &&other)
{
    this->_parent = other._parent;
    this->_index = other._index;
    this->_value = other._value;
    this->_should_write_back = other._should_write_back;
    this->_error = other._error;
    this->_is_valid = other._is_valid;
    other._parent = ft_nullptr;
    other._should_write_back = 0;
    other._error = FT_ERR_INVALID_STATE;
    other._is_valid = 0;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::~scma_handle_accessor_element_proxy(void)
{
    if (!this->_parent)
        return ;
    if (!this->_should_write_back)
        return ;
    if (this->_parent->write_at(this->_value, this->_index) != FT_ERR_SUCCESS)
    {
        this->set_error(this->_parent->get_error());
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename TValue>
inline TValue    *scma_handle_accessor_element_proxy<TValue>::operator->(void)
{
    static TValue error_value = TValue();

    if (!this->_is_valid)
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (&error_value);
    }
    this->_should_write_back = 1;
    this->set_error(FT_ERR_SUCCESS);
    return (&this->_value);
}

template <typename TValue>
inline TValue    &scma_handle_accessor_element_proxy<TValue>::operator*(void)
{
    static TValue error_value = TValue();

    if (!this->_is_valid)
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (error_value);
    }
    this->_should_write_back = 1;
    this->set_error(FT_ERR_SUCCESS);
    return (this->_value);
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>    &scma_handle_accessor_element_proxy<TValue>::operator=(const TValue &source)
{
    if (!this->_parent)
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (*this);
    }
    if (!this->_is_valid)
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (*this);
    }
    this->_value = source;
    if (!this->_parent->write_at(this->_value, this->_index))
    {
        this->set_error(this->_parent->get_error());
        return (*this);
    }
    this->_should_write_back = 0;
    this->set_error(FT_ERR_SUCCESS);
    return (*this);
}

template <typename TValue>
inline scma_handle_accessor_element_proxy<TValue>::operator TValue(void) const
{
    return (this->_value);
}

template <typename TValue>
inline int32_t scma_handle_accessor_element_proxy<TValue>::get_error(void) const
{
    return (this->_error);
}

template <typename TValue>
inline int32_t scma_handle_accessor_element_proxy<TValue>::set_error(int32_t error_code)
{
    this->_error = error_code;
    return (error_code);
}

template <typename TValue>
inline const char *scma_handle_accessor_element_proxy<TValue>::get_error_str(void) const
{
    return (ft_strerror(this->_error));
}

template <typename TValue>
inline int32_t scma_handle_accessor_element_proxy<TValue>::is_valid(void) const
{
    return (this->_is_valid);
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::scma_handle_accessor_const_element_proxy(void)
{
    this->_parent = ft_nullptr;
    this->_index = 0;
    this->_value = TValue();
    this->_error = FT_ERR_INVALID_STATE;
    this->_is_valid = 0;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::scma_handle_accessor_const_element_proxy(const scma_handle_accessor<TValue> *parent, ft_size_t element_index)
{
    this->_parent = parent;
    this->_index = element_index;
    this->_error = FT_ERR_INVALID_STATE;
    this->_is_valid = 0;
    this->_value = TValue();
    if (!this->_parent)
    {
        this->set_error(FT_ERR_INVALID_POINTER);
        return ;
    }
    if (this->_parent->read_at(this->_value, this->_index) != FT_ERR_SUCCESS)
    {
        this->set_error(this->_parent->get_error());
        this->_parent = ft_nullptr;
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->_is_valid = 1;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::scma_handle_accessor_const_element_proxy(
        const scma_handle_accessor_const_element_proxy &other)
{
    this->_parent = other._parent;
    this->_index = other._index;
    this->_value = other._value;
    this->_error = other._error;
    this->_is_valid = other._is_valid;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::scma_handle_accessor_const_element_proxy(scma_handle_accessor_const_element_proxy &&other)
{
    this->_parent = other._parent;
    this->_index = other._index;
    this->_value = other._value;
    this->_error = other._error;
    this->_is_valid = other._is_valid;
    other._parent = ft_nullptr;
    other._error = FT_ERR_INVALID_STATE;
    other._is_valid = 0;
    return ;
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::~scma_handle_accessor_const_element_proxy(void)
{
    return ;
}

template <typename TValue>
inline const TValue    *scma_handle_accessor_const_element_proxy<TValue>::operator->(void) const
{
    static TValue error_value = TValue();

    if (!this->_is_valid)
    {
        this->_error = FT_ERR_INVALID_STATE;
        return (&error_value);
    }
    this->_error = FT_ERR_SUCCESS;
    return (&this->_value);
}

template <typename TValue>
inline const TValue    &scma_handle_accessor_const_element_proxy<TValue>::operator*(void) const
{
    static TValue error_value = TValue();

    if (!this->_is_valid)
    {
        this->_error = FT_ERR_INVALID_STATE;
        return (error_value);
    }
    this->_error = FT_ERR_SUCCESS;
    return (this->_value);
}

template <typename TValue>
inline scma_handle_accessor_const_element_proxy<TValue>::operator TValue(void) const
{
    if (!this->_is_valid)
        this->_error = FT_ERR_INVALID_STATE;
    else
        this->_error = FT_ERR_SUCCESS;
    return (this->_value);
}

template <typename TValue>
inline int32_t scma_handle_accessor_const_element_proxy<TValue>::get_error(void) const
{
    return (this->_error);
}

template <typename TValue>
inline int32_t scma_handle_accessor_const_element_proxy<TValue>::set_error(int32_t error_code) const
{
    this->_error = error_code;
    return (error_code);
}

template <typename TValue>
inline const char *scma_handle_accessor_const_element_proxy<TValue>::get_error_str(void) const
{
    return (ft_strerror(this->_error));
}

template <typename TValue>
inline int32_t scma_handle_accessor_const_element_proxy<TValue>::is_valid(void) const
{
    if (!this->_is_valid)
        this->_error = FT_ERR_INVALID_STATE;
    else
        this->_error = FT_ERR_SUCCESS;
    return (this->_is_valid);
}

#endif

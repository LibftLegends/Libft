#include "class_stringbuf.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_stringbuf::_last_error = FT_ERR_SUCCESS;

int32_t ft_stringbuf::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

ft_stringbuf::ft_stringbuf() noexcept
    : _storage(), _position(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_stringbuf::~ft_stringbuf() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return ;
    }
    (void)this->destroy();
    return ;
}

int32_t ft_stringbuf::initialize(const ft_string &string) noexcept
{
    int32_t assign_error;
    int32_t storage_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_stringbuf::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_position = 0;
    storage_initialize_error = this->_storage.initialize();
    if (storage_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(storage_initialize_error));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    assign_error = this->_storage.assign(string);
    if (assign_error != FT_ERR_SUCCESS)
    {
        (void)this->_storage.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(assign_error));
    }
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_stringbuf::destroy() noexcept
{
    int32_t storage_destroy_error;
    int32_t mutex_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    mutex_destroy_error = this->disable_thread_safety();
    storage_destroy_error = this->_storage.destroy();
    this->_position = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (mutex_destroy_error != FT_ERR_SUCCESS)
        return (set_error(mutex_destroy_error));
    return (set_error(storage_destroy_error));
}

int32_t ft_stringbuf::move(ft_stringbuf &other) noexcept
{
    if (&other == this)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_stringbuf::move",
            "called with uninitialised source object");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_position = 0;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    pt_recursive_mutex *source_mutex = other._mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(source_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    const int32_t storage_move_error = this->_storage.move(other._storage);
    if (storage_move_error != FT_ERR_SUCCESS)
    {
        (void)this->_storage.destroy();
        (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);
        return (set_error(storage_move_error));
    }
    this->_position = other._position;
    this->_mutex = source_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other._storage.destroy();
    other._position = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);
    return (set_error(FT_ERR_SUCCESS));
}

int64_t ft_stringbuf::read(char *buffer, ft_size_t count) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stringbuf::read");
    if (buffer == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    if (count == 0)
    {
        (void)set_error(FT_ERR_SUCCESS);
        return (0);
    }
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    const ft_size_t remaining = this->_storage.size() - this->_position;
    if (count > remaining)
        count = remaining;
    if (count != 0)
        (void)ft_memcpy(buffer, this->_storage.c_str() + this->_position, count);
    this->_position += count;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    (void)set_error(FT_ERR_SUCCESS);
    return (static_cast<int64_t>(count));
}

ft_bool ft_stringbuf::is_valid() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stringbuf::is_valid");
    (void)set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

int32_t ft_stringbuf::get_string(ft_string &value) const noexcept
{
    int32_t lock_error;
    int32_t assign_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_stringbuf::get_string");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    assign_error = value.assign(this->_storage.c_str() + this->_position,
            this->_storage.size() - this->_position);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (assign_error != FT_ERR_SUCCESS)
        return (set_error(assign_error));
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_stringbuf::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_stringbuf::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (set_error(initialize_error));
    }
    this->_mutex = mutex_pointer;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_stringbuf::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_stringbuf::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

ft_bool ft_stringbuf::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_stringbuf::is_thread_safe");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_stringbuf::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_stringbuf::get_error");
    return (_last_error);
}

const char *ft_stringbuf::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_stringbuf::get_error_str");
    return (ft_strerror(_last_error));
}

#include "class_istream.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_istream::_last_error = FT_ERR_SUCCESS;

int32_t ft_istream::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

ft_istream::ft_istream() noexcept
    : _gcount(0), _is_valid(FT_TRUE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_istream::~ft_istream() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        (void)this->destroy();
        return ;
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return ;
}

int32_t ft_istream::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_istream::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_gcount = 0;
    this->_is_valid = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_istream::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    destroy_error = this->disable_thread_safety();
    this->_gcount = 0;
    this->_is_valid = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(destroy_error));
}

int32_t ft_istream::move(ft_istream &other) noexcept
{
    if (&other == this)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_istream::move",
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
        this->_gcount = 0;
        this->_is_valid = FT_TRUE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    pt_recursive_mutex *source_mutex = other._mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(source_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    this->_gcount = other._gcount;
    this->_is_valid = other._is_valid;
    this->_mutex = source_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._gcount = 0;
    other._is_valid = FT_TRUE;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_istream::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_istream::enable_thread_safety");
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

int32_t ft_istream::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_istream::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

ft_bool ft_istream::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_istream::is_thread_safe");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_istream::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_istream::get_error");
    return (_last_error);
}

const char *ft_istream::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_istream::get_error_str");
    return (ft_strerror(_last_error));
}

ssize_t ft_istream::read(char *buffer, ft_size_t count) noexcept
{
    ssize_t bytes_read;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_istream::read");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    if (buffer == ft_nullptr && count > 0)
    {
        this->_is_valid = FT_FALSE;
        this->_gcount = 0;
        (void)set_error(FT_ERR_INVALID_ARGUMENT);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    bytes_read = this->do_read(buffer, count);
    if (bytes_read < 0)
    {
        this->_is_valid = FT_FALSE;
        this->_gcount = 0;
        (void)set_error(FT_ERR_IO);
    }
    else
    {
        this->_is_valid = FT_TRUE;
        this->_gcount = static_cast<ft_size_t>(bytes_read);
        (void)set_error(FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (bytes_read);
}

ft_size_t ft_istream::gcount() const noexcept
{
    ft_size_t count_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_istream::gcount");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (0);
    }
    count_value = this->_gcount;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    (void)set_error(FT_ERR_SUCCESS);
    return (count_value);
}

ft_bool ft_istream::is_valid() const noexcept
{
    ft_bool result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_istream::is_valid");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    result = this->_is_valid;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    (void)set_error(FT_ERR_SUCCESS);
    return (result);
}

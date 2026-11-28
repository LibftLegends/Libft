#include "class_ofstream.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include <fcntl.h>
#include <new>
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_ofstream::_last_error = FT_ERR_SUCCESS;

int32_t ft_ofstream::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

ft_ofstream::ft_ofstream() noexcept
    : _file(), _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_ofstream::~ft_ofstream() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t ft_ofstream::initialize() noexcept
{
    int32_t file_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_ofstream::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    file_initialize_error = this->_file.initialize();
    if (file_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(file_initialize_error));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_ofstream::destroy() noexcept
{
    int32_t destroy_error;
    int32_t file_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    destroy_error = this->disable_thread_safety();
    file_destroy_error = this->_file.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (destroy_error != FT_ERR_SUCCESS)
        return (set_error(destroy_error));
    return (set_error(file_destroy_error));
}

int32_t ft_ofstream::move(ft_ofstream &other) noexcept
{
    if (&other == this)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_ofstream::move",
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
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    pt_recursive_mutex *source_mutex = other._mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(source_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    const int32_t file_move_error = this->_file.move(other._file);
    if (file_move_error == FT_ERR_SUCCESS)
    {
        this->_mutex = source_mutex;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
        other._mutex = ft_nullptr;
        other._initialised_state = FT_CLASS_STATE_DESTROYED;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);
    return (set_error(file_move_error));
}

int32_t ft_ofstream::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_ofstream::enable_thread_safety");
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

int32_t ft_ofstream::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_ofstream::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

ft_bool ft_ofstream::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_ofstream::is_thread_safe");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_ofstream::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_ofstream::get_error");
    return (_last_error);
}

const char *ft_ofstream::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_ofstream::get_error_str");
    return (ft_strerror(_last_error));
}

int32_t ft_ofstream::open(const char *filename) noexcept
{
    int32_t open_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_ofstream::open");
    if (filename == ft_nullptr)
        return (set_error(FT_ERR_INVALID_ARGUMENT));
    open_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (open_error != FT_ERR_SUCCESS)
        return (set_error(open_error));
    open_error = this->_file.open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (open_error != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_INVALID_HANDLE));
    return (set_error(FT_ERR_SUCCESS));
}

ssize_t ft_ofstream::write(const char *string) noexcept
{
    ssize_t bytes_written;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_ofstream::write");
    if (string == ft_nullptr)
    {
        (void)set_error(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    bytes_written = this->_file.write(string);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (bytes_written < 0)
    {
        (void)set_error(FT_ERR_IO);
        return (-1);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (bytes_written);
}

int32_t ft_ofstream::close() noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_ofstream::close");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    this->_file.close();
    const int32_t close_error = this->_file.get_error();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (set_error(close_error));
}

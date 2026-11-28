#include "class_fd_istream.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t ft_fd_istream::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_fd_istream::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_fd_istream::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_fd_istream::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_fd_istream::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_fd_istream::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

ft_fd_istream::ft_fd_istream() noexcept
    : ft_istream(), _file_descriptor(-1), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_fd_istream::~ft_fd_istream() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return ;
    }
    (void)this->destroy();
    return ;
}

int32_t ft_fd_istream::initialize() noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_fd_istream::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = ft_istream::initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    this->_file_descriptor = -1;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_fd_istream::destroy() noexcept
{
    int32_t disable_error;
    int32_t base_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)ft_istream::destroy();
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_file_descriptor = -1;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    base_destroy_error = ft_istream::destroy();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (base_destroy_error);
}

int32_t ft_fd_istream::move(ft_fd_istream &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_fd_istream::move",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_file_descriptor = -1;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    pt_recursive_mutex *descriptor_mutex = other._mutex;
    pt_recursive_mutex *base_mutex = ft_nullptr;
    if (descriptor_mutex != ft_nullptr)
        base_mutex = other.ft_istream::_mutex;
    // Match read's lock order. The base move can reacquire its recursive mutex.
    const int32_t base_lock_error = pt_recursive_mutex_lock_if_not_null(base_mutex);
    if (base_lock_error != FT_ERR_SUCCESS)
        return (base_lock_error);
    const int32_t descriptor_lock_error = pt_recursive_mutex_lock_if_not_null(descriptor_mutex);
    if (descriptor_lock_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(base_mutex);
        return (descriptor_lock_error);
    }
    const int32_t base_move_error = ft_istream::move(other);
    if (base_move_error == FT_ERR_SUCCESS)
    {
        this->_file_descriptor = other._file_descriptor;
        this->_mutex = descriptor_mutex;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
        other._file_descriptor = -1;
        other._mutex = ft_nullptr;
        other._initialised_state = FT_CLASS_STATE_DESTROYED;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(descriptor_mutex);
    (void)pt_recursive_mutex_unlock_if_not_null(base_mutex);
    return (base_move_error);
}

void ft_fd_istream::set_file_descriptor(int32_t file_descriptor) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_fd_istream::set_file_descriptor");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return ;
    this->_file_descriptor = file_descriptor;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_fd_istream::get_file_descriptor() const noexcept
{
    int32_t file_descriptor;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_fd_istream::get_file_descriptor");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (-1);
    file_descriptor = this->_file_descriptor;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (file_descriptor);
}

ssize_t ft_fd_istream::do_read(char *buffer, ft_size_t count)
{
    int32_t file_descriptor;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_fd_istream::do_read");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (-1);
    file_descriptor = this->_file_descriptor;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (static_cast<ssize_t>(su_read(file_descriptor, buffer, count)));
}

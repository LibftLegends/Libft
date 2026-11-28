#include "class_file.hpp"
#include "class_file_stream.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Printf/printf_internal.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
#include <new>
#include <unistd.h>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_file::_last_error = FT_ERR_SUCCESS;

int32_t ft_file::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

int32_t ft_file::enable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    int32_t mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_file::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();

    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

ft_bool ft_file::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file::is_thread_safe");
    (void)set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

ft_file::ft_file() noexcept
    : _file_descriptor(-1), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t ft_file::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_file::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t close_error = this->close_descriptor();
    if (close_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(close_error));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_file::destroy() noexcept
{
    int32_t thread_safety_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(this->close_descriptor()));
    }
    thread_safety_error = this->disable_thread_safety();
    const int32_t close_error = this->close_descriptor();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (thread_safety_error != FT_ERR_SUCCESS)
        return (set_error(thread_safety_error));
    return (set_error(close_error));
}

int32_t ft_file::move(ft_file &other) noexcept
{
    if (&other == this)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_file::move",
            "called with uninitialised source object");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED
        || this->_file_descriptor >= 0)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_file_descriptor = -1;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    pt_recursive_mutex *source_mutex = other._mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(source_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    this->_file_descriptor = other._file_descriptor;
    this->_mutex = source_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._file_descriptor = -1;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);
    return (set_error(FT_ERR_SUCCESS));
}

ft_file::~ft_file() noexcept
{
    (void)this->destroy();
    return ;
}

void    ft_file::close() noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(lock_error);
        return ;
    }
    const int32_t close_error = this->close_descriptor();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    (void)set_error(close_error);
    return ;
}

int32_t ft_file::close_descriptor() noexcept
{
    const int32_t file_descriptor = this->_file_descriptor;
    if (file_descriptor < 0)
        return (FT_ERR_SUCCESS);
    this->_file_descriptor = -1;
    return (su_close(file_descriptor));
}

int32_t ft_file::replace_descriptor(int32_t new_descriptor) noexcept
{
    if (new_descriptor < 0)
        return (cmp_map_system_error_to_ft(errno));
    if (new_descriptor == this->_file_descriptor)
        return (FT_ERR_SUCCESS);
    const int32_t close_error = this->close_descriptor();
    if (close_error != FT_ERR_SUCCESS)
    {
        (void)su_close(new_descriptor);
        return (close_error);
    }
    this->_file_descriptor = new_descriptor;
    return (FT_ERR_SUCCESS);
}

int32_t ft_file::open(const char* filename, int32_t flags, mode_t mode) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_INVALID_STATE));
    if (DEBUG == 1)
        pf_printf("Opening %s\n", filename);
    const int32_t final_error = this->replace_descriptor(su_open(filename, flags, mode));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (set_error(final_error));
}

int32_t ft_file::open(const char* filename, int32_t flags) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_INVALID_STATE));
    const int32_t final_error = this->replace_descriptor(su_open(filename, flags));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (set_error(final_error));
}

int32_t ft_file::get_file_descriptor() const
{
    int32_t lock_error;
    int32_t final_error;
    int32_t file_descriptor;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    final_error = FT_ERR_SUCCESS;
    file_descriptor = -1;
    if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else
        file_descriptor = this->_file_descriptor;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS)
    {
        (void)set_error(final_error);
        return (-1);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (file_descriptor);
}

ssize_t ft_file::read(char *buffer, int32_t count) noexcept
{
    int32_t lock_error;
    int32_t final_error;
    ssize_t bytes_read;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    final_error = FT_ERR_SUCCESS;
    bytes_read = -1;
    if (buffer == ft_nullptr || count <= 0)
        final_error = FT_ERR_INVALID_ARGUMENT;
    else if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else
    {
        bytes_read = su_read(this->_file_descriptor, buffer, static_cast<ft_size_t>(count));
        if (bytes_read == -1)
            final_error = cmp_map_system_error_to_ft(errno);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS)
    {
        (void)set_error(final_error);
        return (-1);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (bytes_read);
}

ssize_t ft_file::write(const char *string) noexcept
{
    int32_t lock_error;
    int32_t final_error;
    ssize_t result;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    final_error = FT_ERR_SUCCESS;
    result = -1;
    if (string == ft_nullptr)
        final_error = FT_ERR_INVALID_ARGUMENT;
    else if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else
    {
        result = su_write(this->_file_descriptor, string, ft_strlen_size_t(string));
        if (result == -1)
            final_error = cmp_map_system_error_to_ft(errno);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS)
    {
        (void)set_error(final_error);
        return (-1);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (result);
}

ssize_t ft_file::write_buffer(const char *buffer, ft_size_t length) noexcept
{
    int32_t lock_error;
    int32_t final_error;
    ssize_t result;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (-1);
    }
    final_error = FT_ERR_SUCCESS;
    result = -1;
    if (buffer == ft_nullptr && length != 0)
        final_error = FT_ERR_INVALID_ARGUMENT;
    else if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else if (length == 0)
        result = 0;
    else
    {
        result = su_write(this->_file_descriptor, buffer, length);
        if (result == -1)
            final_error = cmp_map_system_error_to_ft(errno);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS)
    {
        (void)set_error(final_error);
        return (-1);
    }
    (void)set_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t ft_file::seek(off_t offset, int32_t whence) noexcept
{
    int32_t lock_error;
    int32_t final_error;
    off_t result;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_INVALID_STATE));
    final_error = FT_ERR_SUCCESS;
    if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else
    {
        result = su_lseek(this->_file_descriptor, offset, whence);
        if (result == -1)
            final_error = cmp_map_system_error_to_ft(errno);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS)
        return (set_error(final_error));
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_file::printf(const char *format, ...)
{
    int32_t lock_error;
    int32_t final_error;
    int32_t printed_chars;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)set_error(FT_ERR_INVALID_STATE);
        return (0);
    }
    final_error = FT_ERR_SUCCESS;
    printed_chars = 0;
    if (format == ft_nullptr)
        final_error = FT_ERR_INVALID_ARGUMENT;
    else if (this->_file_descriptor < 0)
        final_error = FT_ERR_INVALID_HANDLE;
    else
    {
        va_list args;

        va_start(args, format);
        printed_chars = pf_printf_fd_v(this->_file_descriptor, format, args);
        va_end(args);
        if (printed_chars < 0)
            final_error = FT_ERR_IO;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (final_error != FT_ERR_SUCCESS && printed_chars >= 0)
        printed_chars = 0;
    (void)set_error(final_error);
    return (printed_chars);
}

int32_t ft_file::copy_to_with_buffer(const char *destination_path, ft_size_t buffer_size) noexcept
{
    if (destination_path == ft_nullptr)
        return (set_error(FT_ERR_INVALID_ARGUMENT));
    if (this->_file_descriptor < 0)
        return (set_error(FT_ERR_INVALID_HANDLE));
    ft_file destination_file;
    int32_t destination_flags;
    int32_t copy_result;
    uint32_t destination_initialize_error;

    destination_initialize_error = destination_file.initialize();
    if (destination_initialize_error != FT_ERR_SUCCESS)
        return (set_error(destination_initialize_error));
    destination_flags = O_WRONLY | O_CREAT | O_TRUNC;
    if (destination_file.open(destination_path, destination_flags, 0644) != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_IO));
    copy_result = ft_file_stream_copy(*this, destination_file, buffer_size);
    if (copy_result != FT_ERR_SUCCESS)
        return (set_error(copy_result));
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_file::copy_to(const char *destination_path) noexcept
{
    return (this->copy_to_with_buffer(destination_path, ft_file_default_buffer_size()));
}

int32_t ft_file::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_file::get_error");
    return (_last_error);
}

const char *ft_file::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_file::get_error_str");
    return (ft_strerror(_last_error));
}

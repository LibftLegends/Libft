#include "gnl_stream.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

gnl_stream::gnl_stream() noexcept
    : _read_callback(ft_nullptr)
    , _user_data(ft_nullptr)
    , _file_descriptor(-1)
    , _file_handle(ft_nullptr)
    , _close_on_reset(FT_FALSE)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _mutex(ft_nullptr)
{
    return ;
}

void gnl_stream::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

gnl_stream::~gnl_stream() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t gnl_stream::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "gnl_stream::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_read_callback = ft_nullptr;
    this->_user_data = ft_nullptr;
    this->_file_descriptor = -1;
    this->_file_handle = ft_nullptr;
    this->_close_on_reset = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t gnl_stream::move(gnl_stream &other) noexcept
{
    int32_t destroy_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        other.abort_lifecycle_error("gnl_stream::move",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->_read_callback = ft_nullptr;
            this->_user_data = ft_nullptr;
            this->_file_descriptor = -1;
            this->_file_handle = ft_nullptr;
            this->_close_on_reset = FT_FALSE;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (static_cast<uint32_t>(destroy_error));
        }
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_read_callback = ft_nullptr;
        this->_user_data = ft_nullptr;
        this->_file_descriptor = -1;
        this->_file_handle = ft_nullptr;
        this->_close_on_reset = FT_FALSE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        if (this->initialize() != FT_ERR_SUCCESS)
        {
            this->_read_callback = ft_nullptr;
            this->_user_data = ft_nullptr;
            this->_file_descriptor = -1;
            this->_file_handle = ft_nullptr;
            this->_close_on_reset = FT_FALSE;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (FT_ERR_INITIALIZATION_FAILED);
        }
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_read_callback = ft_nullptr;
        this->_user_data = ft_nullptr;
        this->_file_descriptor = -1;
        this->_file_handle = ft_nullptr;
        this->_close_on_reset = FT_FALSE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (static_cast<uint32_t>(lock_error));
    }
    this->_read_callback = other._read_callback;
    this->_user_data = other._user_data;
    this->_file_descriptor = other._file_descriptor;
    this->_file_handle = other._file_handle;
    this->_close_on_reset = other._close_on_reset;
    other.reset_source(other._file_handle);
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t gnl_stream::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    if (this->is_thread_safe() == FT_TRUE)
    {
        disable_error = this->disable_thread_safety();
        if (disable_error != FT_ERR_SUCCESS)
            first_error = disable_error;
    }
    this->reset();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (first_error != FT_ERR_SUCCESS)
        return (first_error);
    return (FT_ERR_SUCCESS);
}

int32_t gnl_stream::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t gnl_stream::disable_thread_safety(void) noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool gnl_stream::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

void gnl_stream::reset_source(FILE *retained_file) noexcept
{
    if (this->_close_on_reset && this->_file_handle != ft_nullptr
        && this->_file_handle != retained_file)
        fclose(this->_file_handle);
    this->_file_descriptor = -1;
    this->_file_handle = ft_nullptr;
    this->_close_on_reset = FT_FALSE;
    this->_user_data = ft_nullptr;
    this->_read_callback = ft_nullptr;
    return ;
}

int32_t gnl_stream::init_from_fd(int32_t file_descriptor) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::init_from_fd");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (file_descriptor < 0 || (this->_close_on_reset
        && this->_file_handle != ft_nullptr
        && file_descriptor == fileno(this->_file_handle)))
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->reset_source(ft_nullptr);
    this->_file_descriptor = file_descriptor;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t gnl_stream::init_from_file(FILE *file_handle, ft_bool close_on_reset) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::init_from_file");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (file_handle == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->reset_source(file_handle);
    this->_file_handle = file_handle;
    this->_close_on_reset = close_on_reset;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t gnl_stream::init_from_callback(int64_t (*callback)(void *user_data,
    char *buffer, ft_size_t max_size) noexcept, void *user_data) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::init_from_callback");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (callback == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->reset_source(ft_nullptr);
    this->_read_callback = callback;
    this->_user_data = user_data;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

void gnl_stream::reset() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::reset");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return ;
    this->reset_source(ft_nullptr);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int64_t gnl_stream::read(char *buffer, ft_size_t max_size) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "gnl_stream::read");
    if (buffer == ft_nullptr || max_size == 0U || max_size > SIZE_MAX
        || max_size > static_cast<ft_size_t>(INT64_MAX))
        return (-1);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (-1);
    int64_t read_result = -1;
    if (this->_read_callback != ft_nullptr)
        read_result = this->_read_callback(this->_user_data, buffer, max_size);
    else if (this->_file_descriptor >= 0)
    {
        if (cmp_read(this->_file_descriptor, buffer, max_size, &read_result)
            != FT_ERR_SUCCESS)
            read_result = -1;
    }
    else if (this->_file_handle != ft_nullptr)
    {
        const ft_size_t bytes_read = fread(buffer, 1U,
            FT_UINT64_TO_NATIVE_SIZE_CAST(max_size), this->_file_handle);
        if (bytes_read != 0U || !ferror(this->_file_handle))
            read_result = static_cast<int64_t>(bytes_read);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (read_result < 0 || static_cast<ft_size_t>(read_result) > max_size)
        return (-1);
    return (read_result);
}

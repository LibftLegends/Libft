#include "socket_handle.hpp"
#include "networking.hpp"
#include "../Errno/errno_internal.hpp"
#include <mutex>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <windows.h>
#else
# include <fcntl.h>
# include <unistd.h>
# include <cerrno>
#endif

static int32_t socket_handle_duplicate(int32_t source_descriptor, int32_t &descriptor)
{
#ifdef _WIN32
    const int32_t startup_error = ft_socket_runtime_acquire();
    if (startup_error != FT_ERR_SUCCESS)
        return (startup_error);
    WSAPROTOCOL_INFOW protocol_info;
    if (WSADuplicateSocketW(static_cast<SOCKET>(source_descriptor),
            GetCurrentProcessId(), &protocol_info) != 0)
    {
        ft_socket_runtime_release();
        return (FT_ERR_SOCKET_CREATION_FAILED);
    }
    const SOCKET duplicate = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
        FROM_PROTOCOL_INFO, &protocol_info, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (duplicate == INVALID_SOCKET)
    {
        ft_socket_runtime_release();
        return (FT_ERR_SOCKET_CREATION_FAILED);
    }
    if (duplicate > static_cast<SOCKET>(FT_INT32_MAX))
    {
        closesocket(duplicate);
        ft_socket_runtime_release();
        return (FT_ERR_OUT_OF_RANGE);
    }
    descriptor = static_cast<int32_t>(duplicate);
#else
    do
    {
        descriptor = fcntl(source_descriptor, F_DUPFD_CLOEXEC, 0);
    }
    while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0)
        return (FT_ERR_SOCKET_CREATION_FAILED);
#endif
    return (FT_ERR_SUCCESS);
}

#ifdef _WIN32
static std::mutex &ft_socket_runtime_mutex()
{
    static std::mutex runtime_mutex;
    return (runtime_mutex);
}

static int32_t &ft_socket_runtime_reference_count()
{
    static int32_t reference_count = 0;
    return (reference_count);
}

static ft_bool &ft_socket_runtime_initialised()
{
    static ft_bool initialised = FT_FALSE;
    return (initialised);
}
#endif

ft_socket_handle::ft_socket_handle() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _socket_fd(-1)
{
    return ;
}

ft_socket_handle::~ft_socket_handle() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_socket_handle::move(ft_socket_handle &other) noexcept
{
    return (this->initialize(static_cast<ft_socket_handle &&>(other)));
}

int32_t ft_socket_handle::initialize(const ft_socket_handle &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_socket_handle::initialize(const ft_socket_handle &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (other._socket_fd >= 0)
        return (this->initialize_duplicate(other._socket_fd));
    return (this->initialize());
}

int32_t ft_socket_handle::initialize_duplicate(int32_t descriptor) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_socket_handle::initialize_duplicate", "already initialised");
    int32_t error = FT_ERR_INVALID_HANDLE;
    if (descriptor >= 0)
        error = socket_handle_duplicate(descriptor, this->_socket_fd);
    if (error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket_handle::initialize(ft_socket_handle &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_socket_handle::initialize(ft_socket_handle &&)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_socket_fd = other._socket_fd;
    this->_initialised_state = other._initialised_state;
    other._socket_fd = -1;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket_handle::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_socket_handle::initialize", "initialize called on initialised instance");
    this->_socket_fd = -1;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket_handle::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const ft_bool closed = this->close();
    this->_socket_fd = -1;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (!closed)
        return (FT_ERR_SOCKET_CLOSE_FAILED);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_socket_handle::reset(int32_t socket_fd)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket_handle::reset");
    if (socket_fd == this->_socket_fd)
        return (FT_TRUE);
    if (!this->close())
        return (FT_FALSE);
    if (socket_fd >= 0)
        this->_socket_fd = socket_fd;
    return (FT_TRUE);
}

ft_bool ft_socket_handle::close()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket_handle::close");
    if (this->_socket_fd < 0)
        return (FT_TRUE);
    if (nw_close(this->_socket_fd) != 0)
        return (FT_FALSE);
    this->_socket_fd = -1;
    return (FT_TRUE);
}

ft_bool ft_socket_handle::is_valid() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket_handle::is_valid");
    if (this->_socket_fd >= 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_socket_handle::get() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket_handle::get");
    return (this->_socket_fd);
}

int32_t ft_socket_runtime_acquire()
{
#ifdef _WIN32
    std::mutex &runtime_mutex = ft_socket_runtime_mutex();
    int32_t &reference_count = ft_socket_runtime_reference_count();

    runtime_mutex.lock();
    if (reference_count == 0)
    {
        WSADATA data;

        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            runtime_mutex.unlock();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        ft_socket_runtime_initialised() = FT_TRUE;
    }
    if (reference_count < 0)
    {
        reference_count = 0;
    }
    reference_count++;
    runtime_mutex.unlock();
#endif
    (void)(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void ft_socket_runtime_release()
{
#ifdef _WIN32
    std::mutex &runtime_mutex = ft_socket_runtime_mutex();
    int32_t &reference_count = ft_socket_runtime_reference_count();
    ft_bool &initialised = ft_socket_runtime_initialised();

    runtime_mutex.lock();
    if (reference_count > 0)
    {
        reference_count--;
        if (reference_count == 0 && initialised)
        {
            if (WSACleanup() != 0)
            {
                (void)(FT_ERR_INTERNAL);
            }
            else
            {
                (void)(FT_ERR_SUCCESS);
            }
            initialised = FT_FALSE;
            runtime_mutex.unlock();
            return ;
        }
    }
    runtime_mutex.unlock();
#endif
    (void)(FT_ERR_SUCCESS);
    return ;
}

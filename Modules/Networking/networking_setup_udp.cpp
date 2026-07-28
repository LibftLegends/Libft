#include "udp_socket.hpp"
#include "networking.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdio>
#include <cstring>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

#ifdef _WIN32
static int32_t setsockopt_reuse(int32_t file_descriptor, int32_t option_value)
{
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char *>(&option_value), sizeof(option_value)) == SOCKET_ERROR)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#else
static int32_t setsockopt_reuse(int32_t file_descriptor, int32_t option_value)
{
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR,
            &option_value, sizeof(option_value)) < 0)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#endif

#ifdef _WIN32
static int32_t set_timeout_recv(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char *>(&timeout_milliseconds), sizeof(timeout_milliseconds)) == SOCKET_ERROR)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#else
static int32_t set_timeout_recv(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    struct timeval timeout_value;

    timeout_value.tv_sec = timeout_milliseconds / 1000;
    timeout_value.tv_usec = (timeout_milliseconds % 1000) * 1000;
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
            &timeout_value, sizeof(timeout_value)) < 0)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#endif

#ifdef _WIN32
static int32_t set_timeout_send(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char *>(&timeout_milliseconds), sizeof(timeout_milliseconds)) == SOCKET_ERROR)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#else
static int32_t set_timeout_send(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    struct timeval timeout_value;

    timeout_value.tv_sec = timeout_milliseconds / 1000;
    timeout_value.tv_usec = (timeout_milliseconds % 1000) * 1000;
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_SNDTIMEO,
            &timeout_value, sizeof(timeout_value)) < 0)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}
#endif

udp_socket::udp_socket() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _address(), _socket_fd(-1), _mutex(ft_nullptr)
{
    ft_bzero(&this->_address, sizeof(this->_address));
    return ;
}

udp_socket::~udp_socket() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t udp_socket::move(udp_socket &other) noexcept
{
    return (this->initialize(static_cast<udp_socket &&>(other)));
}

int32_t udp_socket::enable_thread_safety() noexcept
{
    int32_t mutex_error;
    pt_recursive_mutex *mutex_pointer;

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

int32_t udp_socket::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

ft_bool udp_socket::is_thread_safe() const noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t udp_socket::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "udp_socket::initialize", "initialize called on initialised instance");
    ft_bzero(&this->_address, sizeof(this->_address));
    this->_socket_fd = -1;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::initialize(const udp_socket &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "udp_socket::initialize(const udp_socket &)", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    this->_address = other._address;
    this->_socket_fd = -1;
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::initialize(udp_socket &&other) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::create_socket(const SocketConfig &config)
{
    this->_socket_fd = nw_socket(config._address_family, SOCK_DGRAM, config._protocol);
    if (this->_socket_fd < 0)
        return (FT_ERR_SOCKET_CREATION_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::set_non_blocking(const SocketConfig &config)
{
    if (config._non_blocking == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (nw_set_nonblocking(this->_socket_fd) != 0)
        return (FT_ERR_CONFIGURATION);
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::set_timeouts(const SocketConfig &config)
{
    if (config._recv_timeout > 0)
    {
        if (set_timeout_recv(this->_socket_fd, config._recv_timeout) != FT_ERR_SUCCESS)
            return (FT_ERR_CONFIGURATION);
    }
    if (config._send_timeout > 0)
    {
        if (set_timeout_send(this->_socket_fd, config._send_timeout) != FT_ERR_SUCCESS)
            return (FT_ERR_CONFIGURATION);
    }
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::configure_address(const SocketConfig &config)
{
    struct addrinfo hints;
    struct addrinfo *address_info;
    char port_string[16];
    const char *host_value;

    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = config._address_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = config._protocol;
    if (config._type == SocketType::SERVER)
        hints.ai_flags = AI_PASSIVE;
    std::snprintf(port_string, sizeof(port_string), "%u", config._port);
    if (config._ip[0] == '\0')
        host_value = ft_nullptr;
    else
        host_value = config._ip;
    if (getaddrinfo(host_value, port_string, &hints, &address_info) != 0)
        return (FT_ERR_SOCKET_RESOLVE_FAILED);
    ft_memcpy(&this->_address, address_info->ai_addr,
        static_cast<ft_size_t>(address_info->ai_addrlen));
    freeaddrinfo(address_info);
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::bind_socket(const SocketConfig &config)
{
    int32_t reuse_option;
    socklen_t address_length;

    reuse_option = 1;
    if (config._type != SocketType::SERVER)
        return (FT_ERR_SUCCESS);
    if (setsockopt_reuse(this->_socket_fd, reuse_option) != FT_ERR_SUCCESS)
        return (FT_ERR_CONFIGURATION);
    if (config._address_family == AF_INET)
        address_length = sizeof(struct sockaddr_in);
    else if (config._address_family == AF_INET6)
        address_length = sizeof(struct sockaddr_in6);
    else
        return (FT_ERR_CONFIGURATION);
    if (nw_bind(this->_socket_fd, reinterpret_cast<const struct sockaddr *>(&this->_address),
            address_length) < 0)
        return (FT_ERR_SOCKET_BIND_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::connect_socket(const SocketConfig &config)
{
    socklen_t address_length;

    if (config._type != SocketType::CLIENT)
        return (FT_ERR_SUCCESS);
    if (config._address_family == AF_INET)
        address_length = sizeof(struct sockaddr_in);
    else if (config._address_family == AF_INET6)
        address_length = sizeof(struct sockaddr_in6);
    else
        return (FT_ERR_CONFIGURATION);
    if (nw_connect(this->_socket_fd, reinterpret_cast<const struct sockaddr *>(&this->_address),
            address_length) < 0)
        return (FT_ERR_SOCKET_CONNECT_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t udp_socket::initialize(const SocketConfig &config)
{
    int32_t lock_error;
    int32_t step_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::initialize(config)");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_socket_fd >= 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_ALREADY_INITIALISED);
    }
    step_error = this->create_socket(config);
    if (step_error == FT_ERR_SUCCESS)
        step_error = this->set_non_blocking(config);
    if (step_error == FT_ERR_SUCCESS)
        step_error = this->set_timeouts(config);
    if (step_error == FT_ERR_SUCCESS)
        step_error = this->configure_address(config);
    if (step_error == FT_ERR_SUCCESS)
        step_error = this->bind_socket(config);
    if (step_error == FT_ERR_SUCCESS)
        step_error = this->connect_socket(config);
    if (step_error != FT_ERR_SUCCESS)
    {
        if (this->_socket_fd >= 0)
        {
            (void)nw_close(this->_socket_fd);
            this->_socket_fd = -1;
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (step_error);
}

int32_t udp_socket::destroy() noexcept
{
    int32_t lock_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
    {
        if (this->_socket_fd >= 0)
        {
            (void)nw_close(this->_socket_fd);
            this->_socket_fd = -1;
        }
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    }
    (void)this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ssize_t udp_socket::send_to(const void *data, ft_size_t size, int32_t flags,
    const struct sockaddr *destination_address, socklen_t address_length)
{
    int32_t lock_error;
    ssize_t send_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::send_to");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    if (this->_socket_fd < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    send_result = nw_sendto(this->_socket_fd, data, size, flags, destination_address, address_length);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (send_result);
}

ssize_t udp_socket::receive_from(void *buffer, ft_size_t size, int32_t flags,
    struct sockaddr *source_address, socklen_t *address_length)
{
    int32_t lock_error;
    ssize_t receive_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::receive_from");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    if (this->_socket_fd < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    receive_result = nw_recvfrom(this->_socket_fd, buffer, size, flags, source_address, address_length);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (receive_result);
}

ft_bool udp_socket::close_socket()
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::close_socket");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_socket_fd < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_TRUE);
    }
    if (nw_close(this->_socket_fd) != 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    this->_socket_fd = -1;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_TRUE);
}

int32_t udp_socket::get_fd() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::get_fd");
    return (this->_socket_fd);
}

const struct sockaddr_storage &udp_socket::get_address() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "udp_socket::get_address");
    return (this->_address);
}

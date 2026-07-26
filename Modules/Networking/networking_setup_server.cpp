#include "networking.hpp"
#include "socket_class.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstdio>
#include <fcntl.h>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <io.h>
#else
# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/in.h>
# include <unistd.h>
# include <sys/socket.h>
#endif

#if !defined(IPV6_ADD_MEMBERSHIP) && defined(IPV6_JOIN_GROUP)
# define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

#ifdef _WIN32
static inline int32_t setsockopt_reuse(int32_t file_descriptor, int32_t option_value)
{
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char *>(&option_value), sizeof(option_value)));
}

static inline int32_t set_nonblocking_platform(int32_t file_descriptor)
{
    u_long mode;

    mode = 1;
    return (ioctlsocket(static_cast<SOCKET>(file_descriptor), FIONBIO, &mode));
}

static inline int32_t set_timeout_recv(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char *>(&timeout_milliseconds), sizeof(timeout_milliseconds)));
}

static inline int32_t set_timeout_send(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char *>(&timeout_milliseconds), sizeof(timeout_milliseconds)));
}
#else
static inline int32_t setsockopt_reuse(int32_t file_descriptor, int32_t option_value)
{
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR,
            &option_value, sizeof(option_value)));
}

static inline int32_t set_nonblocking_platform(int32_t file_descriptor)
{
    int32_t flags;

    flags = fcntl(file_descriptor, F_GETFL, 0);
    if (flags == -1)
        return (-1);
    return (fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK));
}

static inline int32_t set_timeout_recv(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    struct timeval timeout_value;

    timeout_value.tv_sec = timeout_milliseconds / 1000;
    timeout_value.tv_usec = (timeout_milliseconds % 1000) * 1000;
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
            &timeout_value, sizeof(timeout_value)));
}

static inline int32_t set_timeout_send(int32_t file_descriptor, int32_t timeout_milliseconds)
{
    struct timeval timeout_value;

    timeout_value.tv_sec = timeout_milliseconds / 1000;
    timeout_value.tv_usec = (timeout_milliseconds % 1000) * 1000;
    return (setsockopt(file_descriptor, SOL_SOCKET, SO_SNDTIMEO,
            &timeout_value, sizeof(timeout_value)));
}
#endif

int32_t ft_socket::create_socket(const SocketConfig &config)
{
    this->_socket_file_descriptor = nw_socket(config._address_family, SOCK_STREAM, config._protocol);
    if (this->_socket_file_descriptor < 0)
        return (FT_ERR_SOCKET_CREATION_FAILED);
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::set_reuse_address(const SocketConfig &config)
{
    int32_t option_value;

    if (config._reuse_address == FT_FALSE)
        return (FT_ERR_SUCCESS);
    option_value = 1;
    if (setsockopt_reuse(this->_socket_file_descriptor, option_value) < 0)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_CONFIGURATION);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::set_timeouts(const SocketConfig &config)
{
    if (config._recv_timeout > 0)
    {
        if (set_timeout_recv(this->_socket_file_descriptor, config._recv_timeout) < 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
    }
    if (config._send_timeout > 0)
    {
        if (set_timeout_send(this->_socket_file_descriptor, config._send_timeout) < 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::set_non_blocking(const SocketConfig &config)
{
    if (config._non_blocking == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (set_nonblocking_platform(this->_socket_file_descriptor) < 0)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_CONFIGURATION);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::configure_address(const SocketConfig &config)
{
    ft_string host_copy;
    char port_string[16];
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *current;
    int32_t resolver_status;

    ft_memset(&this->_address, 0, sizeof(this->_address));
    if (host_copy.initialize(config._ip) != FT_ERR_SUCCESS)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_NO_MEMORY);
    }
    if (host_copy.get_error() != FT_ERR_SUCCESS)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_NO_MEMORY);
    }
    if (config._address_family == AF_INET)
    {
        struct sockaddr_in *address_ipv4;

        address_ipv4 = reinterpret_cast<struct sockaddr_in *>(&this->_address);
        address_ipv4->sin_family = AF_INET;
        address_ipv4->sin_port = htons(config._port);
        if (host_copy.empty() != FT_FALSE)
        {
            address_ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
            return (FT_ERR_SUCCESS);
        }
        if (nw_inet_pton(AF_INET, host_copy.c_str(), &address_ipv4->sin_addr) > 0)
            return (FT_ERR_SUCCESS);
    }
    else if (config._address_family == AF_INET6)
    {
        struct sockaddr_in6 *address_ipv6;

        address_ipv6 = reinterpret_cast<struct sockaddr_in6 *>(&this->_address);
        address_ipv6->sin6_family = AF_INET6;
        address_ipv6->sin6_port = htons(config._port);
        if (host_copy.empty() != FT_FALSE)
        {
            address_ipv6->sin6_addr = in6addr_any;
            return (FT_ERR_SUCCESS);
        }
        if (nw_inet_pton(AF_INET6, host_copy.c_str(), &address_ipv6->sin6_addr) > 0)
            return (FT_ERR_SUCCESS);
    }
    else
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_CONFIGURATION);
    }

    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = config._address_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = config._protocol;
    std::snprintf(port_string, sizeof(port_string), "%u", config._port);
    results = ft_nullptr;
    resolver_status = getaddrinfo(host_copy.c_str(), port_string, &hints, &results);
    if (resolver_status != 0)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_SOCKET_RESOLVE_FAILED);
    }
    current = results;
    while (current != ft_nullptr)
    {
        if (current->ai_addrlen <= sizeof(this->_address))
        {
            ft_memcpy(&this->_address, current->ai_addr,
                static_cast<ft_size_t>(current->ai_addrlen));
            freeaddrinfo(results);
            return (FT_ERR_SUCCESS);
        }
        current = current->ai_next;
    }
    freeaddrinfo(results);
    (void)nw_close(this->_socket_file_descriptor);
    this->_socket_file_descriptor = -1;
    return (FT_ERR_SOCKET_RESOLVE_FAILED);
}

int32_t ft_socket::bind_socket(const SocketConfig &config)
{
    socklen_t address_length;

    if (config._address_family == AF_INET)
        address_length = sizeof(struct sockaddr_in);
    else if (config._address_family == AF_INET6)
        address_length = sizeof(struct sockaddr_in6);
    else
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_CONFIGURATION);
    }
    if (nw_bind(this->_socket_file_descriptor,
            reinterpret_cast<const struct sockaddr *>(&this->_address), address_length) < 0)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_SOCKET_BIND_FAILED);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::listen_socket(const SocketConfig &config)
{
    if (nw_listen(this->_socket_file_descriptor, config._backlog) < 0)
    {
        (void)nw_close(this->_socket_file_descriptor);
        this->_socket_file_descriptor = -1;
        return (FT_ERR_SOCKET_LISTEN_FAILED);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::join_multicast_group(const SocketConfig &config)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::join_multicast_group");
    if (config._multicast_group[0] == '\0')
        return (FT_ERR_SUCCESS);
    if (config._address_family == AF_INET)
    {
        struct ip_mreq request_ipv4;

        ft_bzero(&request_ipv4, sizeof(request_ipv4));
        if (nw_inet_pton(AF_INET, config._multicast_group, &request_ipv4.imr_multiaddr) <= 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
        if (config._multicast_interface[0] == '\0')
            request_ipv4.imr_interface.s_addr = htonl(INADDR_ANY);
        else if (nw_inet_pton(AF_INET, config._multicast_interface, &request_ipv4.imr_interface) <= 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
        if (setsockopt(this->_socket_file_descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                reinterpret_cast<const char *>(&request_ipv4), sizeof(request_ipv4)) < 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
        return (FT_ERR_SUCCESS);
    }
    if (config._address_family == AF_INET6)
    {
        struct ipv6_mreq request_ipv6;

        ft_bzero(&request_ipv6, sizeof(request_ipv6));
        if (nw_inet_pton(AF_INET6, config._multicast_group, &request_ipv6.ipv6mr_multiaddr) <= 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
        request_ipv6.ipv6mr_interface = 0;
        if (setsockopt(this->_socket_file_descriptor, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                reinterpret_cast<const char *>(&request_ipv6), sizeof(request_ipv6)) < 0)
        {
            (void)nw_close(this->_socket_file_descriptor);
            this->_socket_file_descriptor = -1;
            return (FT_ERR_CONFIGURATION);
        }
        return (FT_ERR_SUCCESS);
    }
    (void)nw_close(this->_socket_file_descriptor);
    this->_socket_file_descriptor = -1;
    return (FT_ERR_CONFIGURATION);
}

int32_t ft_socket::setup_server(const SocketConfig &config)
{
    int32_t setup_error;

    setup_error = this->create_socket(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->set_reuse_address(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->set_non_blocking(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->set_timeouts(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->configure_address(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->bind_socket(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->listen_socket(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    setup_error = this->join_multicast_group(config);
    if (setup_error != FT_ERR_SUCCESS)
    {
        return (setup_error);
    }
    return (FT_ERR_SUCCESS);
}

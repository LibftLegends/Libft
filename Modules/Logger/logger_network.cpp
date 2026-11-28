#include "logger_internal.hpp"
#include "../Networking/networking.hpp"
#include "../Basic/basic.hpp"
#include <new>

int32_t logger_configure_remote(const char *host, uint16_t port, ft_bool use_tcp)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (host == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (nw_inet_pton(AF_INET, host, &address.sin_addr) != 1)
        return (FT_ERR_INVALID_IP_FORMAT);

    s_network_sink *sink = new (std::nothrow) s_network_sink;
    if (sink == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    sink->send_function = nw_send;
    sink->port = port;
    sink->use_tcp = use_tcp;
    int32_t error = sink->host.initialize(host);
    if (error == FT_ERR_SUCCESS)
        error = network_sink_prepare_thread_safety(sink);
    if (error != FT_ERR_SUCCESS)
    {
        delete sink;
        return (error);
    }
    int32_t socket_type = SOCK_DGRAM;
    if (use_tcp != FT_FALSE)
        socket_type = SOCK_STREAM;
    sink->socket_fd = nw_socket(AF_INET, socket_type, 0);
    error = FT_ERR_SOCKET_CREATION_FAILED;
    if (sink->socket_fd >= 0)
    {
        error = FT_ERR_SOCKET_CONNECT_FAILED;
        if (nw_connect(sink->socket_fd, reinterpret_cast<const sockaddr *>(&address),
                sizeof(address)) == 0)
            error = ft_log_add_sink(ft_network_sink, sink);
        if (error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        (void)nw_close(sink->socket_fd);
    }
    network_sink_teardown_thread_safety(sink);
    delete sink;
    return (error);
}

int32_t ft_log_set_remote_sink(const char *host, uint16_t port, ft_bool use_tcp)
{
    if (logger_configure_remote(host, port, use_tcp) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

int32_t ft_network_sink(const char *message, void *user_data)
{
    auto *sink = static_cast<s_network_sink *>(user_data);
    if (sink == ft_nullptr || message == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    if (network_sink_lock(sink, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);

    int32_t error = FT_ERR_SUCCESS;
    if (sink->socket_fd >= 0)
    {
        t_network_send_function send_function = sink->send_function;
        if (send_function == ft_nullptr)
            send_function = nw_send;
        ft_size_t remaining = ft_strlen(message);
        while (remaining != 0U)
        {
            const ssize_t sent = send_function(sink->socket_fd, message, remaining, 0);
            if (sent <= 0 || static_cast<ft_size_t>(sent) > remaining)
            {
                (void)nw_close(sink->socket_fd);
                sink->socket_fd = -1;
                sink->send_function = ft_nullptr;
                error = FT_ERR_SOCKET_SEND_FAILED;
                break ;
            }
            message += sent;
            remaining -= static_cast<ft_size_t>(sent);
        }
    }
    network_sink_unlock(sink, lock_acquired);
    return (error);
}

#include <stdlib.h>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <io.h>
#else
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#include "socket_class.hpp"
#include "socket_handle.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Observability/observability.hpp"

#ifdef _WIN32
static inline int32_t bind_platform(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    if (bind(static_cast<SOCKET>(socket_file_descriptor), address_value, address_length) == SOCKET_ERROR)
        return (-1);
    return (0);
}

static inline int32_t listen_platform(int32_t socket_file_descriptor, int32_t backlog_value)
{
    if (listen(static_cast<SOCKET>(socket_file_descriptor), backlog_value) == SOCKET_ERROR)
        return (-1);
    return (0);
}

static inline int32_t accept_platform(int32_t socket_file_descriptor, struct sockaddr *address_value, socklen_t *address_length)
{
    SOCKET new_file_descriptor;

    new_file_descriptor = accept(static_cast<SOCKET>(socket_file_descriptor), address_value, address_length);
    if (new_file_descriptor == INVALID_SOCKET)
        return (-1);
    return (static_cast<int32_t>(new_file_descriptor));
}

static inline int32_t socket_platform(int32_t domain_value, int32_t type_value, int32_t protocol_value)
{
    SOCKET socket_file_descriptor;

    if (ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
        return (-1);
    socket_file_descriptor = socket(domain_value, type_value, protocol_value);
    if (socket_file_descriptor == INVALID_SOCKET)
    {
        ft_socket_runtime_release();
        return (-1);
    }
    return (static_cast<int32_t>(socket_file_descriptor));
}

static inline int32_t connect_platform(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    if (connect(static_cast<SOCKET>(socket_file_descriptor), address_value, address_length) == SOCKET_ERROR)
        return (-1);
    return (0);
}

static inline ssize_t send_platform(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags)
{
    int32_t send_result;

    send_result = ::send(static_cast<SOCKET>(socket_file_descriptor), static_cast<const char *>(buffer_value), static_cast<int32_t>(buffer_length), send_flags);
    if (send_result == SOCKET_ERROR)
        return (-1);
    return (send_result);
}

static inline ssize_t recv_platform(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags)
{
    int32_t receive_result;

    receive_result = ::recv(static_cast<SOCKET>(socket_file_descriptor), static_cast<char *>(buffer_value), static_cast<int32_t>(buffer_length), receive_flags);
    if (receive_result == SOCKET_ERROR)
        return (-1);
    return (receive_result);
}

static inline int32_t shutdown_platform(int32_t socket_file_descriptor, int32_t shutdown_mode)
{
    if (::shutdown(static_cast<SOCKET>(socket_file_descriptor), shutdown_mode) == SOCKET_ERROR)
        return (-1);
    return (0);
}

static inline ssize_t sendto_platform(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags,
    const struct sockaddr *destination_address, socklen_t address_length)
{
    int32_t send_result;

    send_result = ::sendto(static_cast<SOCKET>(socket_file_descriptor), static_cast<const char *>(buffer_value),
            static_cast<int32_t>(buffer_length), send_flags, destination_address, address_length);
    if (send_result == SOCKET_ERROR)
        return (-1);
    return (send_result);
}

static inline ssize_t recvfrom_platform(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags,
    struct sockaddr *source_address, socklen_t *address_length)
{
    int32_t receive_result;

    receive_result = ::recvfrom(static_cast<SOCKET>(socket_file_descriptor), static_cast<char *>(buffer_value),
            static_cast<int32_t>(buffer_length), receive_flags, source_address, address_length);
    if (receive_result == SOCKET_ERROR)
        return (-1);
    return (receive_result);
}
#else
static inline int32_t bind_platform(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    if (bind(socket_file_descriptor, address_value, address_length) == -1)
        return (-1);
    return (0);
}

static inline int32_t listen_platform(int32_t socket_file_descriptor, int32_t backlog_value)
{
    if (listen(socket_file_descriptor, backlog_value) == -1)
        return (-1);
    return (0);
}

static inline int32_t accept_platform(int32_t socket_file_descriptor, struct sockaddr *address_value, socklen_t *address_length)
{
    int32_t new_file_descriptor;

    new_file_descriptor = accept(socket_file_descriptor, address_value, address_length);
    if (new_file_descriptor == -1)
        return (-1);
    return (new_file_descriptor);
}

static inline int32_t socket_platform(int32_t domain_value, int32_t type_value, int32_t protocol_value)
{
    int32_t socket_file_descriptor;

    socket_file_descriptor = socket(domain_value, type_value, protocol_value);
    if (socket_file_descriptor == -1)
        return (-1);
    return (socket_file_descriptor);
}

static inline int32_t connect_platform(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    if (connect(socket_file_descriptor, address_value, address_length) == -1)
        return (-1);
    return (0);
}

static inline ssize_t send_platform(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags)
{
    ssize_t send_result;

    send_result = ::send(socket_file_descriptor, buffer_value, buffer_length, send_flags);
    if (send_result == -1)
        return (-1);
    return (send_result);
}

static inline ssize_t recv_platform(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags)
{
    ssize_t receive_result;

    receive_result = ::recv(socket_file_descriptor, buffer_value, buffer_length, receive_flags);
    if (receive_result == -1)
        return (-1);
    return (receive_result);
}

static inline int32_t shutdown_platform(int32_t socket_file_descriptor, int32_t shutdown_mode)
{
    if (::shutdown(socket_file_descriptor, shutdown_mode) == -1)
        return (-1);
    return (0);
}

static inline ssize_t sendto_platform(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags,
    const struct sockaddr *destination_address, socklen_t address_length)
{
    ssize_t send_result;

    send_result = ::sendto(socket_file_descriptor, buffer_value, buffer_length, send_flags, destination_address, address_length);
    if (send_result == -1)
        return (-1);
    return (send_result);
}

static inline ssize_t recvfrom_platform(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags,
    struct sockaddr *source_address, socklen_t *address_length)
{
    ssize_t receive_result;

    receive_result = ::recvfrom(socket_file_descriptor, buffer_value, buffer_length, receive_flags, source_address, address_length);
    if (receive_result == -1)
        return (-1);
    return (receive_result);
}
#endif

static void networking_observability_emit(const char *operation,
    ft_observability_trace_phase phase, int32_t error_code,
    ft_size_t bytes_read, ft_size_t bytes_written)
{
    ft_observability_trace_event event;
    ft_bool success;

    event.module = FT_OBSERVABILITY_MODULE_NETWORKING;
    event.phase = phase;
    event.operation = operation;
    event.resource = ft_nullptr;
    event.error_code = error_code;
    event.bytes_read = bytes_read;
    event.bytes_written = bytes_written;
    (void)observability_trace_emit(&event);
    if (phase == FT_OBSERVABILITY_TRACE_FINISH)
    {
        success = FT_FALSE;
        if (error_code == FT_ERR_SUCCESS)
            success = FT_TRUE;
        (void)observability_record_operation(FT_OBSERVABILITY_MODULE_NETWORKING,
            success, bytes_read, bytes_written);
    }
    return ;
}

int32_t nw_bind(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    return (bind_platform(socket_file_descriptor, address_value, address_length));
}

int32_t nw_listen(int32_t socket_file_descriptor, int32_t backlog_value)
{
    return (listen_platform(socket_file_descriptor, backlog_value));
}

int32_t nw_accept(int32_t socket_file_descriptor, struct sockaddr *address_value, socklen_t *address_length)
{
    int32_t accepted_file_descriptor;

    networking_observability_emit("accept", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    accepted_file_descriptor = accept_platform(socket_file_descriptor, address_value, address_length);
    if (accepted_file_descriptor < 0)
    {
        networking_observability_emit("accept", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_ACCEPT_FAILED, 0, 0);
        return (-1);
    }
#ifdef _WIN32
    if (ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
    {
        nw_close(accepted_file_descriptor);
        return (-1);
    }
#endif
    networking_observability_emit("accept", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, 0, 0);
    return (accepted_file_descriptor);
}

#ifdef LIBFT_TEST_BUILD
static t_nw_socket_hook g_nw_socket_hook = socket_platform;

void nw_set_socket_hook(t_nw_socket_hook hook)
{
    if (hook != ft_nullptr)
    {
        g_nw_socket_hook = hook;
        return ;
    }
    g_nw_socket_hook = socket_platform;
    return ;
}
#endif

int32_t nw_socket(int32_t domain_value, int32_t type_value, int32_t protocol_value)
{
    int32_t socket_file_descriptor;

    networking_observability_emit("socket", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
#ifdef LIBFT_TEST_BUILD
    socket_file_descriptor = g_nw_socket_hook(domain_value, type_value,
            protocol_value);
#else
    socket_file_descriptor = socket_platform(domain_value, type_value,
            protocol_value);
#endif
    if (socket_file_descriptor < 0)
    {
        networking_observability_emit("socket", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_CREATION_FAILED, 0, 0);
        return (socket_file_descriptor);
    }
    networking_observability_emit("socket", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, 0, 0);
    return (socket_file_descriptor);
}

int32_t nw_connect(int32_t socket_file_descriptor, const struct sockaddr *address_value, socklen_t address_length)
{
    int32_t connect_result;

    networking_observability_emit("connect", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    connect_result = connect_platform(socket_file_descriptor, address_value, address_length);
    if (connect_result != 0)
    {
        networking_observability_emit("connect", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_CONNECT_FAILED, 0, 0);
        return (connect_result);
    }
    networking_observability_emit("connect", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, 0, 0);
    return (connect_result);
}

ssize_t nw_send(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags)
{
    ssize_t send_result;
    ft_size_t bytes_written;

    networking_observability_emit("send", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    send_result = send_platform(socket_file_descriptor, buffer_value, buffer_length, send_flags);
    if (send_result < 0)
    {
        networking_observability_emit("send", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_SEND_FAILED, 0, 0);
        return (send_result);
    }
    bytes_written = static_cast<ft_size_t>(send_result);
    networking_observability_emit("send", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, 0, bytes_written);
    return (send_result);
}

ssize_t nw_recv(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags)
{
    ssize_t receive_result;
    ft_size_t bytes_read;

    networking_observability_emit("recv", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    receive_result = recv_platform(socket_file_descriptor, buffer_value, buffer_length, receive_flags);
    if (receive_result < 0)
    {
        networking_observability_emit("recv", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_RECEIVE_FAILED, 0, 0);
        return (receive_result);
    }
    bytes_read = static_cast<ft_size_t>(receive_result);
    networking_observability_emit("recv", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, bytes_read, 0);
    return (receive_result);
}

ssize_t nw_sendto(int32_t socket_file_descriptor, const void *buffer_value, ft_size_t buffer_length, int32_t send_flags,
    const struct sockaddr *destination_address, socklen_t address_length)
{
    ssize_t send_result;
    ft_size_t bytes_written;

    networking_observability_emit("sendto", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    send_result = sendto_platform(socket_file_descriptor, buffer_value, buffer_length, send_flags, destination_address, address_length);
    if (send_result < 0)
    {
        networking_observability_emit("sendto", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_SEND_FAILED, 0, 0);
        return (send_result);
    }
    bytes_written = static_cast<ft_size_t>(send_result);
    networking_observability_emit("sendto", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, 0, bytes_written);
    return (send_result);
}

ssize_t nw_recvfrom(int32_t socket_file_descriptor, void *buffer_value, ft_size_t buffer_length, int32_t receive_flags,
    struct sockaddr *source_address, socklen_t *address_length)
{
    ssize_t receive_result;
    ft_size_t bytes_read;

    networking_observability_emit("recvfrom", FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    receive_result = recvfrom_platform(socket_file_descriptor, buffer_value, buffer_length, receive_flags, source_address, address_length);
    if (receive_result < 0)
    {
        networking_observability_emit("recvfrom", FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_SOCKET_RECEIVE_FAILED, 0, 0);
        return (receive_result);
    }
    bytes_read = static_cast<ft_size_t>(receive_result);
    networking_observability_emit("recvfrom", FT_OBSERVABILITY_TRACE_FINISH,
        FT_ERR_SUCCESS, bytes_read, 0);
    return (receive_result);
}

int32_t nw_close(int32_t socket_file_descriptor)
{
#ifdef _WIN32
    if (closesocket(static_cast<SOCKET>(socket_file_descriptor)) == SOCKET_ERROR)
        return (-1);
    ft_socket_runtime_release();
    return (0);
#else
    if (close(socket_file_descriptor) == -1)
        return (-1);
    return (0);
#endif
}

int32_t nw_shutdown(int32_t socket_file_descriptor, int32_t shutdown_mode)
{
    return (shutdown_platform(socket_file_descriptor, shutdown_mode));
}

int32_t nw_inet_pton(int32_t family_value, const char *ip_address, void *destination)
{
    int32_t conversion_result;

    if (ip_address == ft_nullptr || destination == ft_nullptr)
        return (-1);
    conversion_result = inet_pton(family_value, ip_address, destination);
    if (conversion_result == 1)
        return (1);
    if (conversion_result == 0)
        return (0);
    return (-1);
}

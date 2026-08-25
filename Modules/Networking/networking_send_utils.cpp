#include "networking.hpp"
#include "networking_ssl_compat.hpp"
#include "socket_class.hpp"
#include "../Errno/errno.hpp"
#include <thread>
#include <chrono>
#include <cerrno>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
#endif

int32_t networking_check_socket_after_send(int32_t socket_fd)
{
    int32_t attempt_count;
    int32_t attempt_limit;

    if (socket_fd < 0)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    attempt_limit = 3;
    attempt_count = 0;
    while (attempt_count < attempt_limit)
    {
        int32_t poll_descriptor;
        int32_t poll_result;
        char peek_buffer;
        ssize_t recv_result;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        poll_descriptor = socket_fd;
        poll_result = nw_poll(&poll_descriptor, 1, NULL, 0, 0);
        if (poll_result < 0)
        {
#ifdef _WIN32
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error == WSAEINTR)
            {
                attempt_count++;
                continue ;
            }
#else
            if (errno == EINTR)
            {
                attempt_count++;
                continue ;
            }
#endif
            return (-1);
        }
        if (poll_result == 0 || poll_descriptor == -1)
        {
            attempt_count++;
            continue ;
        }
        peek_buffer = 0;
        recv_result = nw_recv(socket_fd, &peek_buffer, 1, MSG_PEEK);
        if (recv_result == 0)
            return (1);
        if (recv_result < 0)
        {
#ifdef _WIN32
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error == WSAEWOULDBLOCK || last_error == WSAEINTR)
            {
                attempt_count++;
                continue ;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
            {
                attempt_count++;
                continue ;
            }
#endif
            return (-1);
        }
        break ;
    }
    (void)(FT_ERR_SUCCESS);
    return (0);
}

#if NETWORKING_HAS_OPENSSL
int32_t networking_check_ssl_after_send(SSL *ssl_connection)
{
    int32_t attempt_count;
    int32_t attempt_limit;
    int32_t socket_fd;

    if (ssl_connection == NULL)
    {
        (void)(FT_ERR_SOCKET_SEND_FAILED);
        return (-1);
    }
    attempt_limit = 3;
    attempt_count = 0;
    socket_fd = SSL_get_fd(ssl_connection);
    if (socket_fd < 0)
    {
        (void)(FT_ERR_SUCCESS);
        return (0);
    }
    while (attempt_count < attempt_limit)
    {
        char peek_buffer;
        int32_t peek_result;
        int32_t ssl_error;
        int32_t poll_descriptor;
        int32_t poll_result;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        poll_descriptor = socket_fd;
        poll_result = nw_poll(&poll_descriptor, 1, NULL, 0, 0);
        if (poll_result < 0)
        {
#ifdef _WIN32
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error == WSAEINTR)
            {
                attempt_count++;
                continue ;
            }
#else
            if (errno == EINTR)
            {
                attempt_count++;
                continue ;
            }
#endif
            return (-1);
        }
        if (poll_result == 0 || poll_descriptor == -1)
        {
            attempt_count++;
            continue ;
        }
        peek_buffer = 0;
        peek_result = SSL_peek(ssl_connection, &peek_buffer, 1);
        if (peek_result > 0)
            break ;
        if (peek_result == 0)
            break ;
        ssl_error = SSL_get_error(ssl_connection, peek_result);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
        {
            attempt_count++;
            continue ;
        }
        if (ssl_error == SSL_ERROR_ZERO_RETURN)
        {
            (void)(FT_ERR_SOCKET_SEND_FAILED);
            return (-1);
        }
#ifdef _WIN32
        if (ssl_error == SSL_ERROR_SYSCALL)
        {
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error == WSAEWOULDBLOCK || last_error == WSAEINTR)
            {
                attempt_count++;
                continue ;
            }
            if (last_error == 0)
                (void)(FT_ERR_SOCKET_SEND_FAILED);
            return (-1);
        }
#else
        if (ssl_error == SSL_ERROR_SYSCALL)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                attempt_count++;
                continue ;
            }
            if (errno == 0)
                (void)(FT_ERR_SOCKET_SEND_FAILED);
            return (-1);
        }
#endif
        (void)(FT_ERR_SOCKET_SEND_FAILED);
        return (-1);
    }
    (void)(FT_ERR_SUCCESS);
    return (0);
}
#endif

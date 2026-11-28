#include "networking.hpp"
#include "networking_ssl_compat.hpp"
#include <chrono>
#include <cerrno>

static constexpr std::chrono::milliseconds SEND_CHECK_TIMEOUT(30);

static ft_bool networking_send_check_was_interrupted()
{
#ifdef _WIN32
    return (WSAGetLastError() == WSAEINTR);
#else
    return (errno == EINTR);
#endif
}

static ft_bool networking_send_check_can_retry_peek()
{
#ifdef _WIN32
    const int32_t error = WSAGetLastError();
    return (error == WSAEWOULDBLOCK || error == WSAEINTR);
#else
    return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
}

static int32_t networking_wait_after_send(int32_t socket_fd, ft_bool wait_for_write,
    std::chrono::steady_clock::time_point deadline)
{
    while (1)
    {
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return (0);
        const int32_t timeout = static_cast<int32_t>(
            std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
        int32_t ready_descriptor = socket_fd;
        int32_t result;
        if (wait_for_write)
            result = nw_poll(ft_nullptr, 0, &ready_descriptor, 1, timeout);
        else
            result = nw_poll(&ready_descriptor, 1, ft_nullptr, 0, timeout);
        if (result < 0)
        {
            if (networking_send_check_was_interrupted())
                continue ;
            return (-1);
        }
        if (result == 0 || ready_descriptor < 0)
            return (0);
        return (1);
    }
}

int32_t networking_check_socket_after_send(int32_t socket_fd)
{
    if (socket_fd < 0)
        return (-1);
    const auto deadline = std::chrono::steady_clock::now() + SEND_CHECK_TIMEOUT;
    while (1)
    {
        const int32_t ready = networking_wait_after_send(socket_fd, FT_FALSE, deadline);
        if (ready <= 0)
            return (ready);
        char peek_buffer;
#ifdef MSG_DONTWAIT
        const ssize_t received = nw_recv(socket_fd, &peek_buffer, 1, MSG_PEEK | MSG_DONTWAIT);
#else
        const ssize_t received = nw_recv(socket_fd, &peek_buffer, 1, MSG_PEEK);
#endif
        if (received > 0)
            return (0);
        if (received == 0)
            return (1);
        if (!networking_send_check_can_retry_peek())
            return (-1);
    }
}

#if NETWORKING_HAS_OPENSSL
int32_t networking_check_ssl_after_send(SSL *ssl_connection)
{
    if (!ssl_connection)
        return (-1);
    if (SSL_pending(ssl_connection) > 0)
        return (0);
    const int32_t socket_fd = SSL_get_fd(ssl_connection);
    if (socket_fd < 0)
        return (0);
    const auto deadline = std::chrono::steady_clock::now() + SEND_CHECK_TIMEOUT;
    ft_bool wait_for_write = FT_FALSE;
    while (1)
    {
        const int32_t ready = networking_wait_after_send(socket_fd, wait_for_write, deadline);
        if (ready <= 0)
            return (ready);
        char peek_buffer;
        const int32_t received = SSL_peek(ssl_connection, &peek_buffer, 1);
        if (received >= 0)
            return (0);
        const int32_t ssl_error = SSL_get_error(ssl_connection, received);
        wait_for_write = (ssl_error == SSL_ERROR_WANT_WRITE);
        if (!wait_for_write && ssl_error != SSL_ERROR_WANT_READ
            && (ssl_error != SSL_ERROR_SYSCALL || !networking_send_check_can_retry_peek()))
            return (-1);
    }
}
#endif

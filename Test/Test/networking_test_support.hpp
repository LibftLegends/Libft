#ifndef NETWORKING_TEST_SUPPORT_HPP
# define NETWORKING_TEST_SUPPORT_HPP

#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <cerrno>

class networking_test_idle_datagram_io : public networking_datagram_io
{
    public:
        uint64_t sent_datagrams;

        networking_test_idle_datagram_io() noexcept : sent_datagrams(0U)
        {
            return ;
        }

        ~networking_test_idle_datagram_io() noexcept override
        {
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)destination;
            if (data == ft_nullptr && size != 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            this->sent_datagrams += 1U;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            (void)source;
            (void)data;
            (void)capacity;
            if (received_size == ft_nullptr)
                return (FT_ERR_INVALID_ARGUMENT);
            *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (0U);
        }
};

static const ft_size_t NETWORKING_TEST_DATAGRAM_CAPACITY = 2048U;

struct networking_test_datagram
{
    networking_message_endpoint source;
    uint8_t payload[NETWORKING_TEST_DATAGRAM_CAPACITY];
    ft_size_t size;

    networking_test_datagram() noexcept : source(), payload(), size(0U)
    {
        ft_memset(&this->source, 0, sizeof(this->source));
        ft_memset(this->payload, 0, sizeof(this->payload));
        return ;
    }
};

class networking_test_memory_datagram_io : public networking_datagram_io
{
    private:
        networking_test_memory_datagram_io *_peer;
        uint64_t _now;
        uint64_t _bytes_sent;
        uint64_t _bytes_received;
        ft_deque<networking_test_datagram> _incoming;
        ft_bool _drop_next;
        ft_bool _use_source;
        networking_message_endpoint _source;

    public:
        networking_test_memory_datagram_io() noexcept
            : _peer(ft_nullptr), _now(0U), _bytes_sent(0U),
              _bytes_received(0U), _incoming(), _drop_next(FT_FALSE),
              _use_source(FT_FALSE), _source()
        {
            (void)this->_incoming.initialize();
            ft_memset(&this->_source, 0, sizeof(this->_source));
            return ;
        }

        ~networking_test_memory_datagram_io() noexcept override
        {
            (void)this->_incoming.destroy();
            return ;
        }

        void connect(networking_test_memory_datagram_io &peer) noexcept
        {
            this->_peer = &peer;
            return ;
        }

        void advance(uint64_t milliseconds) noexcept
        {
            this->_now += milliseconds;
            return ;
        }

        void drop_next_datagram() noexcept
        {
            this->_drop_next = FT_TRUE;
            return ;
        }

        uint64_t bytes_sent() const noexcept;
        uint64_t bytes_received() const noexcept;

        int32_t inject_datagram(const networking_message_endpoint &source,
            const uint8_t *data, ft_size_t size) noexcept
        {
            networking_test_datagram datagram;

            if (data == ft_nullptr && size != 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (size > NETWORKING_TEST_DATAGRAM_CAPACITY)
                return (FT_ERR_OUT_OF_RANGE);
            datagram.source = source;
            datagram.size = size;
            if (size != 0U)
                ft_memcpy(datagram.payload, data, size);
            this->_incoming.push_back(datagram);
            if (this->_incoming.get_error() != FT_ERR_SUCCESS)
                return (this->_incoming.get_error());
            return (FT_ERR_SUCCESS);
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            networking_test_datagram datagram;

            if (this->_peer == ft_nullptr
                || (data == ft_nullptr && size != 0U))
                return (FT_ERR_SOCKET_SEND_FAILED);
            if (size > NETWORKING_TEST_DATAGRAM_CAPACITY)
                return (FT_ERR_OUT_OF_RANGE);
            if (this->_drop_next != FT_FALSE)
            {
                this->_drop_next = FT_FALSE;
                return (FT_ERR_SUCCESS);
            }
            if (this->_use_source != FT_FALSE)
                datagram.source = this->_source;
            else
                datagram.source = destination;
            datagram.size = size;
            if (size != 0U)
                ft_memcpy(datagram.payload, data, size);
            this->_peer->_incoming.push_back(datagram);
            if (this->_peer->_incoming.get_error() != FT_ERR_SUCCESS)
                return (this->_peer->_incoming.get_error());
            this->_bytes_sent += static_cast<uint64_t>(size);
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            networking_test_datagram datagram;

            if (received_size == ft_nullptr || data == ft_nullptr)
                return (FT_ERR_INVALID_ARGUMENT);
            *received_size = 0U;
            if (this->_incoming.empty())
                return (FT_ERR_EMPTY);
            datagram = this->_incoming.front();
            this->_incoming.pop_front();
            if (datagram.size > capacity)
                return (FT_ERR_OUT_OF_RANGE);
            source = datagram.source;
            if (datagram.size != 0U)
                ft_memcpy(data, datagram.payload, datagram.size);
            *received_size = datagram.size;
            this->_bytes_received += static_cast<uint64_t>(datagram.size);
            return (FT_ERR_SUCCESS);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (this->_now);
        }
};

inline uint64_t networking_test_memory_datagram_io::bytes_sent() const noexcept
{
    return (this->_bytes_sent);
}

inline uint64_t networking_test_memory_datagram_io::bytes_received() const noexcept
{
    return (this->_bytes_received);
}

static inline void networking_test_prepare_endpoint(
    networking_message_endpoint &endpoint) noexcept
{
    ft_memset(&endpoint, 0, sizeof(endpoint));
    endpoint.address.ss_family = AF_INET;
    endpoint.length = static_cast<socklen_t>(sizeof(sockaddr_in));
    return ;
}

static inline ft_bool networking_test_local_ipv4_available(void)
{
    sockaddr_in server_address;
    sockaddr_in client_address;
    socklen_t address_length;
    int32_t server_fd;
    int32_t client_fd;
    int32_t accepted_fd;
    char received_byte;
    ssize_t send_result;
    ssize_t receive_result;

    errno = 0;
    server_fd = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return (FT_FALSE);
    ft_memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = 0;
    if (nw_bind(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            sizeof(server_address)) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    if (nw_listen(server_fd, 1) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    address_length = sizeof(server_address);
    if (getsockname(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            &address_length) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    client_fd = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    ft_memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    client_address.sin_port = server_address.sin_port;
    if (nw_connect(client_fd, reinterpret_cast<struct sockaddr *>(&client_address),
            sizeof(client_address)) != 0)
    {
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    address_length = sizeof(server_address);
    accepted_fd = nw_accept(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            &address_length);
    if (accepted_fd < 0)
    {
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    send_result = nw_send(client_fd, "x", 1, 0);
    if (send_result != 1)
    {
        (void)nw_close(accepted_fd);
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    received_byte = '\0';
    receive_result = nw_recv(accepted_fd, &received_byte, 1, 0);
    if (receive_result != 1 || received_byte != 'x')
    {
        (void)nw_close(accepted_fd);
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    (void)nw_close(accepted_fd);
    (void)nw_close(client_fd);
    (void)nw_close(server_fd);
    return (FT_TRUE);
}

static inline ft_bool networking_test_local_ipv6_available(void)
{
    sockaddr_in6 server_address;
    sockaddr_in6 client_address;
    socklen_t address_length;
    int32_t server_fd;
    int32_t client_fd;
    int32_t accepted_fd;
    char received_byte;
    ssize_t send_result;
    ssize_t receive_result;

    errno = 0;
    server_fd = nw_socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd < 0)
        return (FT_FALSE);
    ft_memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_loopback;
    server_address.sin6_port = 0;
    if (nw_bind(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            sizeof(server_address)) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    if (nw_listen(server_fd, 1) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    address_length = sizeof(server_address);
    if (getsockname(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            &address_length) != 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    client_fd = nw_socket(AF_INET6, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    ft_memset(&client_address, 0, sizeof(client_address));
    client_address.sin6_family = AF_INET6;
    client_address.sin6_addr = in6addr_loopback;
    client_address.sin6_port = server_address.sin6_port;
    if (nw_connect(client_fd, reinterpret_cast<struct sockaddr *>(&client_address),
            sizeof(client_address)) != 0)
    {
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    address_length = sizeof(server_address);
    accepted_fd = nw_accept(server_fd, reinterpret_cast<struct sockaddr *>(&server_address),
            &address_length);
    if (accepted_fd < 0)
    {
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    send_result = nw_send(client_fd, "x", 1, 0);
    if (send_result != 1)
    {
        (void)nw_close(accepted_fd);
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    received_byte = '\0';
    receive_result = nw_recv(accepted_fd, &received_byte, 1, 0);
    if (receive_result != 1 || received_byte != 'x')
    {
        (void)nw_close(accepted_fd);
        (void)nw_close(client_fd);
        (void)nw_close(server_fd);
        return (FT_FALSE);
    }
    (void)nw_close(accepted_fd);
    (void)nw_close(client_fd);
    (void)nw_close(server_fd);
    return (FT_TRUE);
}

#endif

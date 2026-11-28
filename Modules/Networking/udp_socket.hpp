#ifndef NETWORKING_UDP_SOCKET_HPP
#define NETWORKING_UDP_SOCKET_HPP

#include "networking.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <unistd.h>
# include <arpa/inet.h>
#endif

class udp_socket
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        int32_t create_socket(const SocketConfig &config);
        int32_t set_non_blocking(const SocketConfig &config);
        int32_t set_timeouts(const SocketConfig &config);
        int32_t configure_address(const SocketConfig &config);
        int32_t bind_socket(const SocketConfig &config);
        int32_t connect_socket(const SocketConfig &config);

        struct sockaddr_storage _address;
        int32_t _socket_fd;
        ft_bool _connected;
        mutable pt_recursive_mutex *_mutex;

    public:
        udp_socket() noexcept;
        udp_socket(const udp_socket &other) noexcept = delete;
        udp_socket(udp_socket &&other) noexcept = delete;
        ~udp_socket() noexcept;
        udp_socket &operator=(const udp_socket &other) = delete;
        udp_socket &operator=(udp_socket &&other) noexcept = delete;
        int32_t move(udp_socket &other) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const udp_socket &other) noexcept;
        int32_t initialize(udp_socket &&other) noexcept;
        int32_t initialize(const SocketConfig &config);
        int32_t destroy() noexcept;
        ssize_t send_to(const void *data, ft_size_t size, int32_t flags,
                        const struct sockaddr *dest_addr, socklen_t addr_len);
        ssize_t receive_from(void *buffer, ft_size_t size, int32_t flags,
                             struct sockaddr *src_addr, socklen_t *addr_len);
        ft_bool close_socket();
        int32_t get_fd() const;
        const struct sockaddr_storage &get_address() const;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
};

#endif

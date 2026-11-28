#ifndef NETWORKING_SOCKET_CLASS_HPP
#define NETWORKING_SOCKET_CLASS_HPP

#include "networking.hpp"
#include "../Template/vector.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <sys/types.h>
# include <unistd.h>
#endif

class ft_socket
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        int32_t setup_server(const SocketConfig &config);
        int32_t setup_client(const SocketConfig &config);
        int32_t create_socket(const SocketConfig &config);
        int32_t set_reuse_address(const SocketConfig &config);
        int32_t set_non_blocking(const SocketConfig &config);
        int32_t set_timeouts(const SocketConfig &config);
        int32_t configure_address(const SocketConfig &config);
        int32_t bind_socket(const SocketConfig &config);
        int32_t listen_socket(const SocketConfig &config);
        int32_t accept_connection();
        void     reset_to_empty_state();
        void     reset_to_empty_state_locked();
        ft_size_t find_client_index_locked(int32_t file_descriptor) const;
        static void sleep_backoff();
        ssize_t send_data_locked(const void *data, ft_size_t size, int32_t flags);
        ssize_t send_all_locked(const void *data, ft_size_t size, int32_t flags);
        ssize_t receive_data_locked(void *buffer, ft_size_t size, int32_t flags);
        ft_bool close_socket_locked();

        struct sockaddr_storage _address;
        ft_vector<int32_t> _connected;
        int32_t _socket_file_descriptor;
        mutable pt_recursive_mutex *_mutex;

        ft_socket &operator=(const ft_socket &other) = delete;

    public:
        ft_socket() noexcept;
        ft_socket(const ft_socket &other) noexcept = delete;
        ft_socket(ft_socket &&other) noexcept = delete;
        ~ft_socket() noexcept;
        ft_socket &operator=(ft_socket &&other) noexcept = delete;
        int32_t move(ft_socket &other) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_socket &other) noexcept;
        int32_t initialize(ft_socket &&other) noexcept;
        int32_t initialize(const SocketConfig &config);
        int32_t destroy() noexcept;
        ssize_t send_data(const void *data, ft_size_t size, int32_t flags = 0);
        ssize_t send_all(const void *data, ft_size_t size, int32_t flags = 0);
        ssize_t receive_data(void *buffer, ft_size_t size, int32_t flags = 0);
        ft_bool close_socket();
        ssize_t broadcast_data(const void *data, ft_size_t size, int32_t flags);
        ssize_t broadcast_data(const void *data, ft_size_t size, int32_t flags, int32_t exception);
        ssize_t send_data(const void *data, ft_size_t size, int32_t flags, int32_t file_descriptor);
        ft_bool disconnect_client(int32_t file_descriptor);
        void        disconnect_all_clients();
        ft_size_t get_client_count() const;
        ft_bool is_client_connected(int32_t file_descriptor) const;
        int32_t get_file_descriptor() const;
        const struct sockaddr_storage &get_address() const;
        int32_t join_multicast_group(const SocketConfig &config);
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

#endif

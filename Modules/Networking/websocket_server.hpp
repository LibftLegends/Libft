#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "../CPP_class/class_string.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include <cstdint>

#ifdef _WIN32
# include <winsock2.h>
#else
# include <netinet/in.h>
#endif

class ft_socket;

class ft_websocket_server
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        struct s_connection_state
        {
            ft_bool _permessage_deflate_enabled;
        };

        ft_socket *_server_socket;
        mutable pt_recursive_mutex *_mutex;
        ft_map<int32_t, s_connection_state> _connection_states;

        void store_connection_state_locked(int32_t client_fd, ft_bool permessage_deflate_enabled);
        void remove_connection_state_locked(int32_t client_fd);
        ft_bool connection_supports_permessage_deflate_locked(int32_t client_fd) const;
        int32_t perform_handshake_locked(int32_t client_fd);
        int32_t receive_frame_locked(int32_t client_fd, ft_string &message);
        int32_t send_pong_locked(int32_t client_fd, const unsigned char *payload, ft_size_t length);

    public:
        ft_websocket_server() noexcept;
        ~ft_websocket_server() noexcept;
        ft_websocket_server(const ft_websocket_server &other) noexcept = delete;
        ft_websocket_server(ft_websocket_server &&other) noexcept = delete;
        ft_websocket_server &operator=(const ft_websocket_server &other) = delete;
        ft_websocket_server &operator=(ft_websocket_server &&other) noexcept = delete;
        int32_t move(ft_websocket_server &other) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_websocket_server &other) noexcept;
        int32_t initialize(ft_websocket_server &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t start(const char *ip_address, uint16_t port, int32_t address_family = AF_INET, ft_bool non_blocking = FT_FALSE);
        int32_t run_once(int32_t &client_fd, ft_string &message);
        int32_t send_text(int32_t client_fd, const ft_string &message);
        int32_t send_text_many(const int32_t *client_descriptors, ft_size_t count,
            const ft_string &message);
        int32_t get_port(uint16_t &port_value) const;
};

#endif

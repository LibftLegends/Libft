#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Basic/basic.hpp"

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netinet/in.h>
# include <sys/socket.h>
#endif
#include <cstdint>

#if defined(__linux__)
# define NETWORKING_USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
# define NETWORKING_USE_KQUEUE 1
#else
# define NETWORKING_USE_SELECT 1
#endif

int32_t nw_bind(int32_t sockfd, const struct sockaddr *addr, socklen_t addrlen);
int32_t nw_connect(int32_t sockfd, const struct sockaddr *addr, socklen_t addrlen);
int32_t nw_accept(int32_t sockfd, struct sockaddr *addr, socklen_t *addrlen);
int32_t nw_listen(int32_t sockfd, int32_t backlog);
typedef int32_t (*t_nw_socket_hook)(int32_t domain, int32_t type, int32_t protocol);

int32_t nw_socket(int32_t domain, int32_t type, int32_t protocol);
int32_t nw_close(int32_t sockfd);
int32_t nw_shutdown(int32_t sockfd, int32_t how);
void nw_set_socket_hook(t_nw_socket_hook hook);
ssize_t nw_send(int32_t sockfd, const void *buf, ft_size_t len, int32_t flags);
ssize_t nw_recv(int32_t sockfd, void *buf, ft_size_t len, int32_t flags);
ssize_t nw_sendto(int32_t sockfd, const void *buf, ft_size_t len, int32_t flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t nw_recvfrom(int32_t sockfd, void *buf, ft_size_t len, int32_t flags,
                    struct sockaddr *src_addr, socklen_t *addrlen);
int32_t nw_inet_pton(int32_t family, const char *ip_address, void *destination);
int32_t nw_set_nonblocking(int32_t socket_fd);
int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
            int32_t *write_file_descriptors, int32_t write_count,
            int32_t timeout_milliseconds);

class pt_mutex;

struct networking_resolved_address
{
    sockaddr_storage    address;
    socklen_t           length;
};

ft_bool networking_dns_resolve(const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags,
    ft_vector<networking_resolved_address> &out_addresses) noexcept;

ft_bool networking_dns_resolve_first(const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags,
    networking_resolved_address &out_address) noexcept;

ft_bool networking_resolved_address_to_string(
    const networking_resolved_address &address, char *buffer,
    ft_size_t buffer_size) noexcept;

int32_t networking_dns_enable_thread_safety(void) noexcept;
int32_t networking_dns_disable_thread_safety(void) noexcept;
ft_bool networking_dns_is_thread_safe(void) noexcept;

void networking_dns_clear_cache(void) noexcept;

void networking_dns_set_error(int32_t resolver_status) noexcept;

#ifdef LIBFT_TEST_BUILD
void networking_dns_destroy_cache_for_tests(void) noexcept;
#endif

struct event_loop
{
    int32_t *read_file_descriptors;
    int32_t read_count;
    int32_t *write_file_descriptors;
    int32_t write_count;
    pt_mutex *mutex;
    ft_bool thread_safe_enabled;
};

class udp_socket;

void event_loop_init(event_loop *loop);
void event_loop_clear(event_loop *loop);
int32_t event_loop_add_socket(event_loop *loop, int32_t socket_fd, ft_bool is_write);
int32_t event_loop_remove_socket(event_loop *loop, int32_t socket_fd, ft_bool is_write);
int32_t event_loop_run(event_loop *loop, int32_t timeout_milliseconds);
int32_t event_loop_prepare_thread_safety(event_loop *loop);
void event_loop_teardown_thread_safety(event_loop *loop);
int32_t event_loop_lock(event_loop *loop, ft_bool *lock_acquired);
void event_loop_unlock(event_loop *loop, ft_bool lock_acquired);

int32_t udp_event_loop_wait_read(event_loop *loop, udp_socket &socket, int32_t timeout_milliseconds);
int32_t udp_event_loop_wait_write(event_loop *loop, udp_socket &socket, int32_t timeout_milliseconds);
ssize_t udp_event_loop_receive(event_loop *loop, udp_socket &socket, void *buffer, ft_size_t size,
                               int32_t flags, struct sockaddr *source_address,
                               socklen_t *address_length, int32_t timeout_milliseconds);
ssize_t udp_event_loop_send(event_loop *loop, udp_socket &socket, const void *data, ft_size_t size,
                            int32_t flags, const struct sockaddr *destination_address,
                            socklen_t address_length, int32_t timeout_milliseconds);

int32_t networking_check_socket_after_send(int32_t socket_fd);

enum class SocketType
{
    SERVER,
    CLIENT,
    RAW
};

class SocketConfig
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;

    public:
        SocketType _type;
        char _ip[46];
        uint16_t _port;
        int32_t _backlog;
        int32_t _protocol;
        int32_t _address_family;
        ft_bool _reuse_address;
        ft_bool _non_blocking;
        int32_t _recv_timeout;
        int32_t _send_timeout;
        char _multicast_group[46];
        char _multicast_interface[46];

        SocketConfig() noexcept;
        SocketConfig(const SocketConfig& other) noexcept = delete;
        SocketConfig(SocketConfig&& other) noexcept = delete;
        ~SocketConfig() noexcept;

        SocketConfig& operator=(const SocketConfig& other) noexcept = delete;
        SocketConfig& operator=(SocketConfig&& other) noexcept = delete;
        int32_t move(SocketConfig &other) noexcept;
        int32_t initialize() noexcept;
        int32_t initialize(const SocketConfig &other) noexcept;
        int32_t initialize(SocketConfig &&other) noexcept;
        int32_t destroy() noexcept;
};

int32_t socket_config_prepare_thread_safety(SocketConfig *config);
void socket_config_teardown_thread_safety(SocketConfig *config);
int32_t socket_config_lock(const SocketConfig *config, ft_bool *lock_acquired);
void socket_config_unlock(const SocketConfig *config, ft_bool lock_acquired);

#endif

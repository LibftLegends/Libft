#ifndef API_INTERNAL_HPP
#define API_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../Basic/limits.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/openssl_support.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <cstddef>
#include <cstdint>

#ifdef LIBFT_TEST_BUILD
typedef ft_bool (*api_request_wait_until_ready_hook)(void);
void api_request_set_downgrade_wait_hook(
    api_request_wait_until_ready_hook hook);
#endif

enum class api_connection_security_mode
{
    PLAIN,
    TLS
};

class api_connection_pool_handle
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t                     _initialised_state;
        mutable pt_recursive_mutex  *_mutex;

        void abort_lifecycle_error(const char *method_name,
            const char *reason) const noexcept;
        void abort_if_not_initialised(const char *method_name) const noexcept;

    public:
        ft_string key;
        ft_socket socket;
#if NETWORKING_HAS_OPENSSL
        SSL *tls_session;
        SSL_CTX *tls_context;
#endif
        api_connection_security_mode security_mode;
        ft_bool has_socket;
        ft_bool from_pool;
        ft_bool should_store;
        ft_bool negotiated_http2;
        ft_bool plain_socket_timed_out;
        ft_bool plain_socket_validated;

        api_connection_pool_handle();
        api_connection_pool_handle(const api_connection_pool_handle &other) noexcept = delete;
        api_connection_pool_handle(api_connection_pool_handle &&other) noexcept = delete;
        ~api_connection_pool_handle();

        api_connection_pool_handle &operator=(const api_connection_pool_handle &other) = delete;
        api_connection_pool_handle &operator=(api_connection_pool_handle &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const api_connection_pool_handle &other) noexcept;
        int32_t initialize(api_connection_pool_handle &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(api_connection_pool_handle &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;
#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

ft_bool api_connection_pool_acquire(api_connection_pool_handle &handle,
    const char *host, uint16_t port,
    api_connection_security_mode security_mode,
    const char *security_identity);
void api_connection_pool_mark_idle(api_connection_pool_handle &handle);
void api_connection_pool_evict(api_connection_pool_handle &handle);
void api_connection_pool_disable_store(api_connection_pool_handle &handle);
void api_connection_pool_flush(void);
void api_connection_pool_set_enabled(ft_bool enabled);
ft_bool api_connection_pool_is_enabled(void);
void api_connection_pool_set_max_idle(ft_size_t max_idle);
ft_size_t api_connection_pool_get_max_idle(void);
void api_connection_pool_set_idle_timeout(int64_t idle_timeout_ms);
int64_t api_connection_pool_get_idle_timeout(void);
#if NETWORKING_HAS_OPENSSL
ft_bool api_connection_pool_track_tls_session(SSL *tls_session);
ft_bool api_connection_pool_untrack_tls_session(SSL *tls_session);
#endif
void api_debug_reset_connection_pool_counters(void);
ft_size_t api_debug_get_connection_pool_acquires(void);
ft_size_t api_debug_get_connection_pool_reuses(void);
ft_size_t api_debug_get_connection_pool_misses(void);

ft_bool api_append_content_length_header(ft_string &request, ft_size_t content_length);
ft_size_t api_debug_get_last_async_request_size(void);
ft_size_t api_debug_get_last_async_bytes_sent(void);
int32_t api_debug_get_last_async_send_state(void);
int32_t api_debug_get_last_async_send_timeout(void);
ft_size_t api_debug_get_last_async_bytes_received(void);
int32_t api_debug_get_last_async_receive_state(void);
int32_t api_debug_get_last_async_receive_timeout(void);

#ifdef LIBFT_TEST_BUILD
void api_retry_circuit_destroy_for_tests(void);
#endif

ft_bool api_is_configuration_socket_error(int32_t error_code);

#endif

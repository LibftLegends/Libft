#ifndef API_WRAPPER_HPP
#define API_WRAPPER_HPP

#include "../Basic/limits.hpp"
#include "../JSon/json.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "api_request_signing.hpp"
#include <cstdint>
#include <cstddef>

typedef void (*api_callback)(char *body, int32_t status, void *user_data);
typedef void (*api_json_callback)(json_group *body, int32_t status, void *user_data);

typedef void (*api_stream_headers_callback)(int32_t status_code,
        const char *headers, void *user_data);
typedef ft_bool (*api_stream_body_callback)(const char *chunk_data,
        ft_size_t chunk_size, ft_bool is_final_chunk, void *user_data);

class api_streaming_handler
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        api_stream_headers_callback _headers_callback;
        api_stream_body_callback _body_callback;
        void *_user_data;
        mutable pt_recursive_mutex *_mutex;

        void abort_lifecycle_error(const char *method_name,
            const char *reason) const noexcept;
        void abort_if_not_initialised(const char *method_name) const noexcept;

    public:
        api_streaming_handler() noexcept;
        api_streaming_handler(const api_streaming_handler &other) noexcept = delete;
        api_streaming_handler &operator=(const api_streaming_handler &other) noexcept = delete;
        api_streaming_handler(api_streaming_handler &&other) noexcept = delete;
        api_streaming_handler &operator=(api_streaming_handler &&other) noexcept = delete;
        ~api_streaming_handler();
        int32_t initialize() noexcept;
        int32_t initialize(const api_streaming_handler &other) noexcept;
        int32_t initialize(api_streaming_handler &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(api_streaming_handler &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        void reset() noexcept;

        void set_headers_callback(api_stream_headers_callback callback) noexcept;
        void set_body_callback(api_stream_body_callback callback) noexcept;
        void set_user_data(void *user_data) noexcept;

        ft_bool invoke_headers_callback(int32_t status_code,
            const char *headers) const noexcept;
        ft_bool invoke_body_callback(const char *chunk_data, ft_size_t chunk_size,
            ft_bool is_final_chunk, ft_bool &should_continue) const noexcept;
#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

class api_retry_policy
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        int32_t _max_attempts;
        int32_t _initial_delay_ms;
        int32_t _max_delay_ms;
        int32_t _backoff_multiplier;
        int32_t _circuit_breaker_threshold;
        int32_t _circuit_breaker_cooldown_ms;
        int32_t _circuit_breaker_half_open_successes;
        mutable pt_recursive_mutex *_mutex;

        void clear_values() noexcept;
        int32_t read_value(const int32_t &field) const noexcept;
        void write_value(int32_t &field, int32_t value) noexcept;

        void abort_lifecycle_error(const char *method_name,
            const char *reason) const noexcept;
        void abort_if_not_initialised(const char *method_name) const noexcept;

    public:
        api_retry_policy() noexcept;
        api_retry_policy(const api_retry_policy &other) noexcept = delete;
        api_retry_policy &operator=(const api_retry_policy &other) noexcept = delete;
        api_retry_policy(api_retry_policy &&other) noexcept = delete;
        api_retry_policy &operator=(api_retry_policy &&other) noexcept = delete;
        ~api_retry_policy();
        int32_t initialize() noexcept;
        int32_t initialize(const api_retry_policy &other) noexcept;
        int32_t initialize(api_retry_policy &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(api_retry_policy &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        void reset() noexcept;

        void set_max_attempts(int32_t value) noexcept;
        void set_initial_delay_ms(int32_t value) noexcept;
        void set_max_delay_ms(int32_t value) noexcept;
        void set_backoff_multiplier(int32_t value) noexcept;
        void set_circuit_breaker_threshold(int32_t value) noexcept;
        void set_circuit_breaker_cooldown_ms(int32_t value) noexcept;
        void set_circuit_breaker_half_open_successes(int32_t value) noexcept;

        int32_t get_max_attempts() const noexcept;
        int32_t get_initial_delay_ms() const noexcept;
        int32_t get_max_delay_ms() const noexcept;
        int32_t get_backoff_multiplier() const noexcept;
        int32_t get_circuit_breaker_threshold() const noexcept;
        int32_t get_circuit_breaker_cooldown_ms() const noexcept;
        int32_t get_circuit_breaker_half_open_successes() const noexcept;
#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

struct api_transport_hooks
{
    ft_bool (*request_stream)(const char *ip_address, uint16_t port,
            const char *method, const char *path,
            const api_streaming_handler *streaming_handler,
            json_group *payload, const char *headers, int32_t timeout,
            const api_retry_policy *retry_policy, void *user_data);
    ft_bool (*request_stream_http2)(const char *ip_address, uint16_t port,
            const char *method, const char *path,
            const api_streaming_handler *streaming_handler,
            json_group *payload, const char *headers, int32_t timeout,
            ft_bool *used_http2, const api_retry_policy *retry_policy,
            void *user_data);
    ft_bool (*request_stream_host)(const char *host, uint16_t port,
            const char *method, const char *path,
            const api_streaming_handler *streaming_handler,
            json_group *payload, const char *headers, int32_t timeout,
            const api_retry_policy *retry_policy, void *user_data);
    ft_bool (*request_stream_tls)(const char *host, uint16_t port,
            const char *method, const char *path,
            const api_streaming_handler *streaming_handler,
            json_group *payload, const char *headers, int32_t timeout,
            const char *ca_certificate, ft_bool verify_peer,
            const api_retry_policy *retry_policy, void *user_data);
    ft_bool (*request_stream_tls_http2)(const char *host, uint16_t port,
            const char *method, const char *path,
            const api_streaming_handler *streaming_handler,
            json_group *payload, const char *headers, int32_t timeout,
            const char *ca_certificate, ft_bool verify_peer, ft_bool *used_http2,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_string)(const char *ip_address, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_string_http2)(const char *ip_address, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_string_host)(const char *host, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_string_tls)(const char *host, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_string_tls_http2)(const char *host, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_https)(const char *ip_address, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout,
            const char *ca_certificate, ft_bool verify_peer,
            const api_retry_policy *retry_policy, void *user_data);
    char *(*request_https_http2)(const char *ip_address, uint16_t port,
            const char *method, const char *path, json_group *payload,
            const char *headers, int32_t *status, int32_t timeout,
            const char *ca_certificate, ft_bool verify_peer, ft_bool *used_http2,
            const api_retry_policy *retry_policy, void *user_data);
    ft_bool (*request_string_async)(const char *ip_address, uint16_t port,
            const char *method, const char *path, api_callback callback,
            void *user_data, json_group *payload, const char *headers,
            int32_t timeout, void *transport_user_data);
    ft_bool (*request_string_tls_async)(const char *host, uint16_t port,
            const char *method, const char *path, api_callback callback,
            void *user_data, json_group *payload, const char *headers,
            int32_t timeout, void *transport_user_data);
    void *user_data;
};

void    api_set_transport_hooks(const api_transport_hooks *hooks);
void    api_clear_transport_hooks(void);
const api_transport_hooks    *api_get_transport_hooks(void);

ft_bool    api_request_string_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000);
ft_bool    api_request_string_http2_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000,
        ft_bool *used_http2 = ft_nullptr);

ft_bool    api_request_string_tls_async(const char *host, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000);
ft_bool    api_request_string_tls_http2_async(const char *host, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000,
        ft_bool *used_http2 = ft_nullptr);

ft_bool    api_request_json_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000);
ft_bool    api_request_json_http2_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000,
        ft_bool *used_http2 = ft_nullptr);

ft_bool    api_request_json_tls_async(const char *host, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000);
ft_bool    api_request_json_tls_http2_async(const char *host, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t timeout = 60000,
        ft_bool *used_http2 = ft_nullptr);

ft_bool    api_request(const char *ip_address, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000, ft_bool enable_http2 = FT_TRUE,
        ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);
ft_bool    api_request_stream(const char *ip_address, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
ft_bool    api_request_stream_http2(const char *ip_address, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);
ft_bool    api_request_stream_host(const char *host, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
ft_bool    api_request_stream_tls(const char *host, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000, const char *ca_certificate = ft_nullptr,
        ft_bool verify_peer = FT_TRUE,
        const api_retry_policy *retry_policy = ft_nullptr);
ft_bool    api_request_stream_tls_http2(const char *host, uint16_t port,
        const char *method, const char *path,
        const api_streaming_handler *streaming_handler,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t timeout = 60000, const char *ca_certificate = ft_nullptr,
        ft_bool verify_peer = FT_TRUE, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
char    *api_request_string_http2(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_bearer(const char *ip_address, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_basic(const char *ip_address, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_https(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, const char *ca_certificate = ft_nullptr,
        ft_bool verify_peer = FT_TRUE,
        const api_retry_policy *retry_policy = ft_nullptr);
char    *api_request_https_http2(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, const char *ca_certificate = ft_nullptr,
        ft_bool verify_peer = FT_TRUE, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_tls(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
char    *api_request_string_tls_http2(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_host(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_host_bearer(const char *host, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_host_basic(const char *host, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
json_group *api_request_json_http2(const char *ip_address, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_bearer(const char *ip_address, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_basic(const char *ip_address, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_tls(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);
json_group *api_request_json_tls_http2(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000, ft_bool *used_http2 = ft_nullptr,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_host(const char *host, uint16_t port,
        const char *method, const char *path, json_group *payload = ft_nullptr,
        const char *headers = ft_nullptr, int32_t *status = ft_nullptr,
        int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_host_bearer(const char *host, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_host_basic(const char *host, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_tls_bearer(const char *host, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_tls_bearer(const char *host, uint16_t port,
        const char *method, const char *path, const char *token,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_tls_basic(const char *host, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_tls_basic(const char *host, uint16_t port,
        const char *method, const char *path, const char *credentials,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

char    *api_request_string_url(const char *url, const char *method,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

json_group *api_request_json_url(const char *url, const char *method,
        json_group *payload = ft_nullptr, const char *headers = ft_nullptr,
        int32_t *status = ft_nullptr, int32_t timeout = 60000,
        const api_retry_policy *retry_policy = ft_nullptr);

void    api_connection_pool_flush(void);
void    api_connection_pool_set_max_idle(ft_size_t max_idle);
ft_size_t  api_connection_pool_get_max_idle(void);
void    api_connection_pool_set_idle_timeout(int64_t idle_timeout_ms);
int64_t   api_connection_pool_get_idle_timeout(void);
void    api_debug_reset_connection_pool_counters(void);
ft_size_t  api_debug_get_connection_pool_acquires(void);
ft_size_t  api_debug_get_connection_pool_reuses(void);
ft_size_t  api_debug_get_connection_pool_misses(void);

ft_size_t  api_debug_get_last_async_request_size(void);
ft_size_t  api_debug_get_last_async_bytes_sent(void);
int32_t     api_debug_get_last_async_send_state(void);
int32_t     api_debug_get_last_async_send_timeout(void);
ft_size_t  api_debug_get_last_async_bytes_received(void);
int32_t     api_debug_get_last_async_receive_state(void);
int32_t     api_debug_get_last_async_receive_timeout(void);

#endif

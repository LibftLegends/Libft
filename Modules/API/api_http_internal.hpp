#ifndef API_HTTP_INTERNAL_HPP
#define API_HTTP_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../Basic/limits.hpp"
#include "api_internal.hpp"
#include "api.hpp"
#include "../JSon/json.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <limits.h>

class ft_string;

char *api_http_execute_plain(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port,
    const api_retry_policy *retry_policy, int32_t &error_code);
char *api_https_execute(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *ca_certificate, ft_bool verify_peer, const char *host,
    uint16_t port, const char *security_identity,
    const api_retry_policy *retry_policy, int32_t &error_code);
#if NETWORKING_HAS_OPENSSL
char *api_http_execute_plain_http2(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port,
    const api_retry_policy *retry_policy, ft_bool &used_http2, int32_t &error_code);
#endif
ft_bool api_http_execute_plain_streaming(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t timeout,
    const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, int32_t &error_code);
ft_bool api_retry_circuit_allow(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy, int32_t &error_code);
void api_retry_circuit_record_success(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy);
void api_retry_circuit_record_failure(const api_connection_pool_handle &handle,
    const api_retry_policy *retry_policy);
void api_retry_circuit_reset(void);
ft_bool api_http_prepare_plain_socket(api_connection_pool_handle &connection_handle,
    const char *host, uint16_t port, int32_t timeout, int32_t &error_code);
ft_bool api_http_plain_socket_is_alive(api_connection_pool_handle &connection_handle);
ft_bool api_http_should_retry_plain(int32_t error_code);
#if NETWORKING_HAS_OPENSSL
ft_bool api_http_execute_plain_http2_streaming(
    api_connection_pool_handle &connection_handle, const char *method,
    const char *path, const char *host_header, json_group *payload,
    const char *headers, int32_t timeout, const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, ft_bool &used_http2,
    int32_t &error_code);
#endif
ft_bool api_https_execute_streaming(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t timeout,
    const char *ca_certificate, ft_bool verify_peer, const char *host,
    uint16_t port, const char *security_identity,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, int32_t &error_code);
ft_bool api_http_stream_invoke_body(const api_streaming_handler *streaming_handler,
    const char *chunk_data, ft_size_t chunk_size, ft_bool is_final_chunk,
    int32_t &error_code);
void api_http_stream_invoke_headers(
    const api_streaming_handler *streaming_handler, int32_t status_code,
    const char *headers);
ft_bool api_http_stream_process_chunked_buffer(ft_string &buffer,
    int64_t &chunk_remaining, ft_bool &trailers_pending,
    ft_bool &final_chunk_sent,
    const api_streaming_handler *streaming_handler, int32_t &error_code);
int32_t api_request_set_resolve_error(int32_t resolver_status);
#if NETWORKING_HAS_OPENSSL
void api_request_set_ssl_error(SSL *ssl_session, int32_t operation_result);
#endif

int32_t api_retry_get_max_attempts(const api_retry_policy *retry_policy);
int32_t api_retry_get_initial_delay(const api_retry_policy *retry_policy);
int32_t api_retry_get_max_delay(const api_retry_policy *retry_policy);
int32_t api_retry_get_multiplier(const api_retry_policy *retry_policy);
int32_t api_retry_get_circuit_threshold(const api_retry_policy *retry_policy);
int32_t api_retry_get_circuit_cooldown(const api_retry_policy *retry_policy);
int32_t api_retry_get_half_open_successes(const api_retry_policy *retry_policy);
int32_t api_retry_prepare_delay(int32_t delay, int32_t max_delay);
int32_t api_retry_next_delay(int32_t current_delay, int32_t max_delay,
    int32_t multiplier);

#endif

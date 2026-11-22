#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include "../CPP_class/class_string.hpp"
#include "ssl_wrapper.hpp"
#include "openssl_support.hpp"

#if NETWORKING_HAS_OPENSSL
typedef void (*http_response_handler)(int32_t status_code, const ft_string &headers,
    const char *body_chunk, ft_size_t chunk_size, ft_bool finished);

int32_t http_get_stream(const char *host, const char *path, http_response_handler handler,
    ft_bool use_ssl = FT_FALSE, const char *custom_port = NULL);
int32_t http_get(const char *host, const char *path, ft_string &response, ft_bool use_ssl = FT_FALSE, const char *custom_port = NULL);
int32_t http_post(const char *host, const char *path, const ft_string &body, ft_string &response, ft_bool use_ssl = FT_FALSE, const char *custom_port = NULL);
int32_t http_client_send_plain_request(int32_t socket_fd, const char *buffer, ft_size_t length);
int32_t http_client_send_ssl_request(SSL *ssl_connection, const char *buffer, ft_size_t length);
// The shared pool is synchronized by default. Disabling it closes idle connections.
int32_t http_client_pool_enable_thread_safety(void);
int32_t http_client_pool_disable_thread_safety(void);
ft_bool http_client_pool_is_thread_safe(void);
void http_client_pool_flush(void);
void http_client_pool_set_max_idle(ft_size_t max_idle);
ft_size_t http_client_pool_get_idle_count(void);
void http_client_pool_debug_reset_counters(void);
ft_size_t http_client_pool_debug_get_reuse_count(void);
ft_size_t http_client_pool_debug_get_miss_count(void);
#endif

#endif

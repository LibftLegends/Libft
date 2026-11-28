#include "api.hpp"
#include "api_internal.hpp"
#include "api_http_internal.hpp"
#include "api_http_common.hpp"
#include "api_request_metrics.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/socket_handle.hpp"
#include "../Networking/ssl_wrapper.hpp"
#include "../Networking/networking.hpp"
#include "../Networking/openssl_support.hpp"
#include "../CPP_class/class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Logger/logger.hpp"
#include "../Printf/printf.hpp"
#include "../Threading/thread.hpp"
#include "../Time/time.hpp"
#include <errno.h>
#ifndef _WIN32
# include <fcntl.h>
#endif
#include "../Template/move.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <sys/time.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/select.h>
#endif
#if NETWORKING_HAS_OPENSSL
#include <openssl/err.h>
#include <cstdint>
#include <climits>
#include <utility>
#endif

#if NETWORKING_HAS_OPENSSL

static int32_t api_request_tls_prepare_connection_handle(api_connection_pool_handle &handle,
    int32_t &error_code) noexcept
{
    int32_t initialize_result = handle.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
        error_code = initialize_result;
    return (initialize_result);
}

static constexpr char TLS_UNVERIFIED_IDENTITY[] = "verify:0";
static constexpr char TLS_DEFAULT_CA_IDENTITY[] = "verify:1";

static const char *api_request_tls_pool_identity(ft_string &storage, ft_bool verify_peer,
    const char *ca_certificate, int32_t &error_code)
{
    if (!verify_peer)
        return (TLS_UNVERIFIED_IDENTITY);
    if (!ca_certificate || ca_certificate[0] == '\0')
        return (TLS_DEFAULT_CA_IDENTITY);
    error_code = storage.initialize("verify:1:");
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage.append(ca_certificate);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (storage.c_str());
}

static void api_request_copy_ip(char destination[46], const char *source)
{
    destination[0] = '\0';
    if (source == ft_nullptr)
        return ;
    ft_strncpy(destination, source, 45);
    destination[45] = '\0';
    return ;
}

char *api_request_https(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const char *ca_certificate, ft_bool verify_peer,
    const api_retry_policy *retry_policy)
{
    if (ft_log_get_api_logging())
    {
        const char *log_ip = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";
        if (ip_address)
            log_ip = ip_address;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_https %s:%u %s %s",
            log_ip, port, log_method, log_path);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_https)
    {
        return (hooks->request_https(ip_address, port, method, path, payload, headers,
                status, timeout, ca_certificate, verify_peer, retry_policy,
                hooks->user_data));
    }
    int32_t error_code = FT_ERR_SUCCESS;
    char *metrics_result_body;
    int32_t metrics_status_storage;
    int32_t *metrics_status_pointer;

    metrics_result_body = ft_nullptr;
    metrics_status_storage = -1;
    if (status)
        metrics_status_pointer = status;
    else
        metrics_status_pointer = &metrics_status_storage;
    api_request_metrics_guard metrics_guard;
    (void)metrics_guard.initialize_for_request(ip_address, port, method, path, headers, payload,
        &metrics_result_body, metrics_status_pointer,
        &error_code);

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    ft_string security_identity;
    const char *security_identity_pointer = api_request_tls_pool_identity(security_identity,
        verify_peer, ca_certificate, error_code);
    if (!security_identity_pointer)
        return (ft_nullptr);

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::TLS, security_identity_pointer);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (ft_nullptr);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }

    metrics_result_body = api_https_execute(connection_handle, method, path, ip_address,
            payload, headers, status, timeout, ca_certificate, verify_peer,
            ip_address, port, security_identity_pointer, retry_policy, error_code);
    if (!metrics_result_body)
        return (ft_nullptr);
    connection_guard.set_success();
    return (metrics_result_body);
}

char *api_request_https_http2(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const char *ca_certificate, ft_bool verify_peer, ft_bool *used_http2,
    const api_retry_policy *retry_policy)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
    if (ft_log_get_api_logging())
    {
        const char *log_ip = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";
        if (ip_address)
            log_ip = ip_address;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_https_http2 %s:%u %s %s",
            log_ip, port, log_method, log_path);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_https_http2)
    {
        return (hooks->request_https_http2(ip_address, port, method, path, payload,
                headers, status, timeout, ca_certificate, verify_peer,
                used_http2, retry_policy, hooks->user_data));
    }
    int32_t error_code = FT_ERR_SUCCESS;
    char *metrics_result_body;
    int32_t metrics_status_storage;
    int32_t *metrics_status_pointer;

    metrics_result_body = ft_nullptr;
    metrics_status_storage = -1;
    if (status)
        metrics_status_pointer = status;
    else
        metrics_status_pointer = &metrics_status_storage;
    api_request_metrics_guard metrics_guard;
    (void)metrics_guard.initialize_for_request(ip_address, port, method, path, headers, payload,
        &metrics_result_body, metrics_status_pointer,
        &error_code);

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    ft_string security_identity;
    const char *security_identity_pointer = api_request_tls_pool_identity(security_identity,
        verify_peer, ca_certificate, error_code);
    if (!security_identity_pointer)
        return (ft_nullptr);

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::TLS, security_identity_pointer);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (ft_nullptr);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }

    metrics_result_body = api_https_execute(connection_handle, method, path, ip_address,
            payload, headers, status, timeout, ca_certificate, verify_peer,
            ip_address, port, security_identity_pointer, retry_policy, error_code);
    if (!metrics_result_body)
        return (ft_nullptr);
    connection_guard.set_success();
    return (metrics_result_body);
}

ft_bool api_request_stream_tls(const char *host, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout, const char *ca_certificate,
    ft_bool verify_peer, const api_retry_policy *retry_policy)
{
    if (ft_log_get_api_logging())
    {
        const char *log_host = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";

        if (host)
            log_host = host;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_stream_tls %s:%u %s %s", log_host, port,
            log_method, log_path);
    }
    if (!streaming_handler || !host || !method || !path)
        return (FT_FALSE);
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_stream_tls)
    {
        return (hooks->request_stream_tls(host, port, method, path,
                streaming_handler, payload, headers, timeout, ca_certificate,
                verify_peer, retry_policy, hooks->user_data));
    }
    int32_t error_code = FT_ERR_SUCCESS;

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, host);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    ft_string security_identity;
    const char *security_identity_pointer = api_request_tls_pool_identity(security_identity,
        verify_peer, ca_certificate, error_code);
    if (!security_identity_pointer)
        return (FT_FALSE);

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    pooled_connection = api_connection_pool_acquire(connection_handle, host,
            port, api_connection_security_mode::TLS, security_identity_pointer);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (FT_FALSE);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    const ft_bool result = api_https_execute_streaming(connection_handle, method, path,
            host, payload, headers, timeout, ca_certificate, verify_peer,
            host, port, security_identity_pointer, retry_policy, streaming_handler, error_code);
    if (!result)
        return (FT_FALSE);
    connection_guard.set_success();
    return (FT_TRUE);
}

ft_bool api_request_stream_tls_http2(const char *host, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout, const char *ca_certificate,
    ft_bool verify_peer, ft_bool *used_http2, const api_retry_policy *retry_policy)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
    if (ft_log_get_api_logging())
    {
        const char *log_host = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";

        if (host)
            log_host = host;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_stream_tls_http2 %s:%u %s %s", log_host,
            port, log_method, log_path);
    }
    if (!streaming_handler || !host || !method || !path)
        return (FT_FALSE);
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_stream_tls_http2)
    {
        return (hooks->request_stream_tls_http2(host, port, method, path,
                streaming_handler, payload, headers, timeout, ca_certificate,
                verify_peer, used_http2, retry_policy, hooks->user_data));
    }
    int32_t error_code = FT_ERR_SUCCESS;

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, host);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    ft_string security_identity;
    const char *security_identity_pointer = api_request_tls_pool_identity(security_identity,
        verify_peer, ca_certificate, error_code);
    if (!security_identity_pointer)
        return (FT_FALSE);

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    pooled_connection = api_connection_pool_acquire(connection_handle, host,
            port, api_connection_security_mode::TLS, security_identity_pointer);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (FT_FALSE);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    const ft_bool result = api_https_execute_streaming(connection_handle, method, path,
            host, payload, headers, timeout, ca_certificate, verify_peer,
            host, port, security_identity_pointer, retry_policy, streaming_handler, error_code);
    if (!result)
        return (FT_FALSE);
    connection_guard.set_success();
    return (FT_TRUE);
}

char *api_request_string_tls(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (ft_log_get_api_logging())
    {
        const char *log_host = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";
        if (host)
            log_host = host;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_string_tls %s:%u %s %s",
            log_host, port, log_method, log_path);
    }
    int32_t error_code = FT_ERR_SUCCESS;

    if (!host || !method || !path)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_tls)
    {
        return (hooks->request_string_tls(host, port, method, path, payload,
                headers, status, timeout, retry_policy, hooks->user_data));
    }

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, host);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    pooled_connection = api_connection_pool_acquire(connection_handle, host,
            port, api_connection_security_mode::TLS, TLS_UNVERIFIED_IDENTITY);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (ft_nullptr);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }

    char *metrics_result_body;
    int32_t metrics_status_storage;
    int32_t *metrics_status_pointer;

    metrics_result_body = ft_nullptr;
    metrics_status_storage = -1;
    if (status)
        metrics_status_pointer = status;
    else
        metrics_status_pointer = &metrics_status_storage;
    api_request_metrics_guard metrics_guard;
    (void)metrics_guard.initialize_for_request(host, port, method, path, headers, payload,
        &metrics_result_body, metrics_status_pointer,
        &error_code);

    metrics_result_body = api_https_execute(connection_handle, method, path, host,
            payload, headers, status, timeout, ft_nullptr, FT_FALSE, host, port,
            TLS_UNVERIFIED_IDENTITY, retry_policy, error_code);
    if (!metrics_result_body)
        return (ft_nullptr);
    connection_guard.set_success();
    return (metrics_result_body);
}

char *api_request_string_tls_http2(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
    const api_retry_policy *retry_policy)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
    if (ft_log_get_api_logging())
    {
        const char *log_host = "(null)";
        const char *log_method = "(null)";
        const char *log_path = "(null)";
        if (host)
            log_host = host;
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_request_string_tls_http2 %s:%u %s %s",
            log_host, port, log_method, log_path);
    }
    int32_t error_code = FT_ERR_SUCCESS;

    if (!host || !method || !path)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_tls_http2)
    {
        return (hooks->request_string_tls_http2(host, port, method, path,
                payload, headers, status, timeout, used_http2, retry_policy,
                hooks->user_data));
    }

    SocketConfig config;
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, host);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_tls_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    pooled_connection = api_connection_pool_acquire(connection_handle, host, port,
            api_connection_security_mode::TLS, TLS_DEFAULT_CA_IDENTITY);
    if (!pooled_connection)
    {
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            error_code = socket_setup_error;
            return (ft_nullptr);
        }
        connection_handle.has_socket = FT_TRUE;
    }
    struct api_connection_return_guard
    {
        api_connection_pool_handle *handle;
        ft_bool success;
        api_connection_return_guard(api_connection_pool_handle &value)
        {
            handle = &value;
            success = FT_FALSE;
            return ;
        }
        ~api_connection_return_guard()
        {
            if (!handle)
                return ;
            if (success)
            {
                api_connection_pool_mark_idle(*handle);
                return ;
            }
            api_connection_pool_evict(*handle);
            return ;
        }
        void set_success(void)
        {
            success = FT_TRUE;
            return ;
        }
    } connection_guard(connection_handle);
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    char *metrics_result_body;
    int32_t metrics_status_storage;
    int32_t *metrics_status_pointer;

    metrics_result_body = ft_nullptr;
    metrics_status_storage = -1;
    if (status)
        metrics_status_pointer = status;
    else
        metrics_status_pointer = &metrics_status_storage;
    api_request_metrics_guard metrics_guard;
    (void)metrics_guard.initialize_for_request(host, port, method, path, headers, payload,
        &metrics_result_body, metrics_status_pointer,
        &error_code);

    metrics_result_body = api_https_execute(connection_handle, method, path, host,
            payload, headers, status, timeout, ft_nullptr, FT_TRUE,
            host, port, TLS_DEFAULT_CA_IDENTITY, retry_policy, error_code);
    if (!metrics_result_body)
        return (ft_nullptr);
    connection_guard.set_success();
    return (metrics_result_body);
}

json_group *api_request_json_tls(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    char *body = api_request_string_tls(host, port, method, path, payload,
                                       headers, status, timeout, retry_policy);
    if (!body)
        return (ft_nullptr);
    json_group *result = json_read_from_string(body);
    cma_free(body);
    return (result);
}

json_group *api_request_json_tls_http2(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
    const api_retry_policy *retry_policy)
{
    ft_bool http2_used_local;
    char *body;

    http2_used_local = FT_FALSE;
    body = api_request_string_tls_http2(host, port, method, path, payload,
            headers, status, timeout, &http2_used_local, retry_policy);
    if (used_http2)
        *used_http2 = http2_used_local;
    if (!body)
        return (ft_nullptr);
    json_group *result = json_read_from_string(body);
    cma_free(body);
    return (result);
}

char *api_request_string_tls_bearer(const char *host, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!token)
        return (api_request_string_tls(host, port, method, path, payload,
                                       headers, status, timeout,
                                       retry_policy));
    ft_string header_string;
    if (api_http_build_authorization_headers(header_string, headers,
            api_http_authorization_scheme::BEARER, token) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (api_request_string_tls(host, port, method, path, payload,
                                   header_string.c_str(), status, timeout,
                                   retry_policy));
}

json_group *api_request_json_tls_bearer(const char *host, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    char *body = api_request_string_tls_bearer(host, port, method, path, token,
                                               payload, headers, status, timeout,
                                               retry_policy);
    if (!body)
        return (ft_nullptr);
    json_group *result = json_read_from_string(body);
    cma_free(body);
    return (result);
}

char *api_request_string_tls_basic(const char *host, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!credentials)
        return (api_request_string_tls(host, port, method, path, payload,
                                       headers, status, timeout,
                                       retry_policy));
    ft_string header_string;
    if (api_http_build_authorization_headers(header_string, headers,
            api_http_authorization_scheme::BASIC, credentials) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (api_request_string_tls(host, port, method, path, payload,
                                   header_string.c_str(), status, timeout,
                                   retry_policy));
}

json_group *api_request_json_tls_basic(const char *host, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    char *body = api_request_string_tls_basic(host, port, method, path,
                                              credentials, payload, headers,
                                              status, timeout, retry_policy);
    if (!body)
        return (ft_nullptr);
    json_group *result = json_read_from_string(body);
    cma_free(body);
    return (result);
}

struct api_tls_async_request
{
    char *host;
    uint16_t port;
    char *method;
    char *path;
    json_group *payload;
    char *headers;
    int32_t timeout;
    api_callback callback;
    void *user_data;
};

static ft_bool api_tls_async_wait_for_connect(int32_t socket_fd, int32_t timeout)
{
    fd_set write_set;
    fd_set error_set;
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);
    FD_SET(socket_fd, &write_set);
    FD_SET(socket_fd, &error_set);
    timeval remaining{};
    remaining.tv_sec = timeout / 1000;
    remaining.tv_usec = (timeout % 1000) * 1000;
    if (select(socket_fd + 1, ft_nullptr, &write_set, &error_set, &remaining) <= 0)
        return (FT_FALSE);
    int32_t connect_error = 0;
    socklen_t error_size = sizeof(connect_error);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
            reinterpret_cast<char *>(&connect_error), &error_size) != 0)
        return (FT_FALSE);
    return (connect_error == 0 && FD_ISSET(socket_fd, &write_set));
}

static ft_bool api_tls_async_connect_socket(int32_t socket_fd,
    const networking_resolved_address &address, int32_t timeout)
{
#ifndef _WIN32
    if (socket_fd >= FD_SETSIZE)
        return (FT_FALSE);
#endif
    if (nw_set_nonblocking(socket_fd) != 0)
        return (FT_FALSE);
    if (nw_connect(socket_fd, reinterpret_cast<const sockaddr *>(&address.address),
            address.length) == 0)
        return (FT_TRUE);
#ifdef _WIN32
    const int32_t connect_error = WSAGetLastError();
    if (connect_error != WSAEINPROGRESS && connect_error != WSAEWOULDBLOCK)
        return (FT_FALSE);
#else
    if (errno != EINPROGRESS)
        return (FT_FALSE);
#endif
    return (api_tls_async_wait_for_connect(socket_fd, timeout));
}

static int32_t api_tls_async_connect_resolved(
    const ft_vector<networking_resolved_address> &addresses, int32_t timeout)
{
    const t_monotonic_time_point start = time_monotonic_point_now();
    const ft_size_t address_count = addresses.size();
    ft_size_t address_index = 0;
    while (address_index < address_count)
    {
        const auto &address = addresses[address_index];
        const int32_t socket_fd = nw_socket(address.address.ss_family, SOCK_STREAM, 0);
        if (socket_fd >= 0)
        {
            const int64_t elapsed = time_monotonic_point_diff_ms(start,
                time_monotonic_point_now());
            int32_t remaining = 0;
            if (elapsed < timeout)
                remaining = timeout - static_cast<int32_t>(elapsed);
            if (api_tls_async_connect_socket(socket_fd, address, remaining))
                return (socket_fd);
            nw_close(socket_fd);
        }
        if (timeout > 0 && time_monotonic_point_diff_ms(start,
                time_monotonic_point_now()) >= timeout)
            break ;
        ++address_index;
    }
    return (-1);
}

static int32_t api_tls_async_open_socket(const api_tls_async_request &data)
{
    ft_vector<networking_resolved_address> addresses;
    if (addresses.initialize() != FT_ERR_SUCCESS)
        return (-1);
    char port_string[6];
    pf_snprintf(port_string, sizeof(port_string), "%u", data.port);
#ifdef _WIN32
    if (ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
        return (-1);
#endif
    int32_t socket_fd = -1;
    if (networking_dns_resolve(data.host, port_string, AF_UNSPEC, SOCK_STREAM, 0, 0, addresses))
        socket_fd = api_tls_async_connect_resolved(addresses, data.timeout);
#ifdef _WIN32
    ft_socket_runtime_release();
#endif
    return (socket_fd);
}

static ft_bool api_tls_async_wait_for_io(int32_t socket_fd, int32_t ssl_error, timeval &timeout)
{
    fd_set socket_set;
    FD_ZERO(&socket_set);
    FD_SET(socket_fd, &socket_set);
    if (ssl_error == SSL_ERROR_WANT_READ)
        return (select(socket_fd + 1, &socket_set, ft_nullptr, ft_nullptr, &timeout) > 0);
    if (ssl_error == SSL_ERROR_WANT_WRITE)
        return (select(socket_fd + 1, ft_nullptr, &socket_set, ft_nullptr, &timeout) > 0);
    return (FT_FALSE);
}

struct api_tls_async_response
{
    ft_size_t header_search_offset = 0;
    ft_size_t header_length = 0;
    int64_t content_length = -1;
    int32_t status = -1;
    ft_bool chunked = FT_FALSE;
    ft_bool complete = FT_FALSE;
    api_http_chunk_scan chunk_scan;
};

static int32_t api_tls_async_scan_response(ft_string &response,
    const char *method, api_tls_async_response &state)
{
    while (state.header_length == 0)
    {
        const char *start = response.c_str();
        const ft_size_t size = response.size();
        const char *end = ft_strstr(start + state.header_search_offset, "\r\n\r\n");
        if (!end)
        {
            if (size > 3)
                state.header_search_offset = size - 3;
            return (FT_ERR_SUCCESS);
        }
        const char *status_line_end = api_http_find_crlf(start, end + 2);
        const char *status_start = ft_strchr(start, ' ');
        int64_t status;
        if (!status_start || !status_line_end || status_start >= status_line_end
            || status_line_end - status_start < 4
            || !api_http_parse_decimal(status_start + 1, status_start + 4, status)
            || status < 100 || status > 599
            || (status_start + 4 != status_line_end && status_start[4] != ' '))
            return (FT_ERR_IO);
        const ft_size_t header_length = static_cast<ft_size_t>(end - start) + 4;
        if (status >= 100 && status < 200 && status != 101)
        {
            const int32_t erase_error = response.erase(0, header_length);
            if (erase_error != FT_ERR_SUCCESS)
                return (erase_error);
            state.header_search_offset = 0;
            continue ;
        }
        state.status = static_cast<int32_t>(status);
        state.header_length = header_length;
        if (status == 101 || status == 204 || status == 304 || ft_strcmp(method, "HEAD") == 0)
        {
            state.content_length = 0;
            state.complete = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }
        ft_bool connection_close;
        ft_bool has_length;
        int64_t content_length;
        api_http_parse_headers(start, end + 2, connection_close,
            state.chunked, has_length, content_length);
        if (has_length)
            state.content_length = content_length;
    }

    const ft_size_t body_size = response.size() - state.header_length;
    if (state.chunked)
    {
        state.complete = api_http_chunked_body_complete(response.c_str() + state.header_length,
            body_size, state.chunk_scan);
        if (state.chunk_scan.invalid)
            return (FT_ERR_IO);
    }
    else if (state.content_length >= 0 &&
        body_size >= static_cast<ft_size_t>(state.content_length))
        state.complete = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static char *api_tls_async_copy_body(const ft_string &response,
    const api_tls_async_response &state)
{
    const char *body = response.c_str() + state.header_length;
    ft_size_t body_size = response.size() - state.header_length;
    ft_string decoded;
    if (state.chunked)
    {
        ft_size_t consumed_length;
        if (decoded.initialize() != FT_ERR_SUCCESS
            || !api_http_decode_chunked(body, body_size, decoded, consumed_length))
            return (ft_nullptr);
        body = decoded.c_str();
        body_size = decoded.size();
    }
    else if (state.content_length >= 0)
        body_size = static_cast<ft_size_t>(state.content_length);
    if (body_size == FT_SYSTEM_SIZE_MAX)
        return (ft_nullptr);
    char *copy = static_cast<char *>(cma_malloc(body_size + 1));
    if (!copy)
        return (ft_nullptr);
    ft_memcpy(copy, body, body_size);
    copy[body_size] = '\0';
    return (copy);
}

static char *api_tls_async_receive_response(SSL *ssl_session, int32_t socket_fd,
    const char *method, timeval &timeout, int32_t &status)
{
    ft_string response;
    if (response.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    api_tls_async_response state;
    char buffer[4096];
    while (!state.complete)
    {
        const int32_t received = SSL_read(ssl_session, buffer, sizeof(buffer));
        if (received > 0)
        {
            if (response.append(buffer, static_cast<ft_size_t>(received)) != FT_ERR_SUCCESS
                || api_tls_async_scan_response(response, method, state) != FT_ERR_SUCCESS)
                return (ft_nullptr);
            continue ;
        }
        const int32_t ssl_error = SSL_get_error(ssl_session, received);
        if (ssl_error == SSL_ERROR_ZERO_RETURN
            || (received == 0 && ssl_error == SSL_ERROR_SYSCALL && ERR_peek_error() == 0))
        {
            if (state.header_length == 0 || state.content_length >= 0 || state.chunked)
                return (ft_nullptr);
            break ;
        }
        if (!api_tls_async_wait_for_io(socket_fd, ssl_error, timeout))
            return (ft_nullptr);
    }
    char *body = api_tls_async_copy_body(response, state);
    if (body)
        status = state.status;
    return (body);
}

static void api_tls_async_worker(api_tls_async_request *data)
{
    SSL_CTX *context = ft_nullptr;
    SSL *ssl_session = ft_nullptr;
    int32_t socket_fd = -1;
    ft_string request;
    ft_string body_string;
    char *result_body = ft_nullptr;
    int32_t status = -1;
    struct timeval tv;
    ft_size_t total_sent;
    int32_t ssl_ret;
    if (!data)
        return ;
    if (!data->host || !data->method || !data->path)
        goto cleanup;
    if (!OPENSSL_init_ssl(0, ft_nullptr))
    {
        api_request_set_ssl_error(ft_nullptr, 0);
        goto cleanup;
    }
    context = SSL_CTX_new(TLS_client_method());
    if (!context)
    {
        api_request_set_ssl_error(ft_nullptr, 0);
        goto cleanup;
    }

    socket_fd = api_tls_async_open_socket(*data);
    if (socket_fd < 0)
        goto cleanup;
    tv.tv_sec = data->timeout / 1000;
    tv.tv_usec = (data->timeout % 1000) * 1000;

    ssl_session = SSL_new(context);
    if (!ssl_session)
    {
        api_request_set_ssl_error(ft_nullptr, 0);
        goto cleanup;
    }
    if (SSL_set_fd(ssl_session, socket_fd) != 1)
    {
        api_request_set_ssl_error(ssl_session, 0);
        goto cleanup;
    }

    ssl_ret = SSL_connect(ssl_session);
    while (ssl_ret <= 0)
    {
        if (!api_tls_async_wait_for_io(socket_fd, SSL_get_error(ssl_session, ssl_ret), tv))
            goto cleanup;
        ssl_ret = SSL_connect(ssl_session);
    }

    request = data->method;
    request += " ";
    request += data->path;
    request += " HTTP/1.1\r\nHost: ";
    request += data->host;
    if (data->headers && data->headers[0])
    {
        request += "\r\n";
        request += data->headers;
    }
    if (data->payload)
    {
        char *temporary_string = json_write_to_string(data->payload);
        if (!temporary_string)
            goto cleanup;
        body_string = temporary_string;
        cma_free(temporary_string);
        request += "\r\nContent-Type: application/json";
        if (!api_append_content_length_header(request, body_string.size()))
        {
            goto cleanup;
        }
    }
    request += "\r\nConnection: close\r\n\r\n";
    if (data->payload)
        request += body_string.c_str();

    total_sent = 0;
    while (total_sent < request.size())
    {
        int32_t write_ret = SSL_write(ssl_session, request.c_str() + total_sent,
                                  static_cast<int32_t>(request.size() - total_sent));
        if (write_ret <= 0)
        {
            if (!api_tls_async_wait_for_io(socket_fd, SSL_get_error(ssl_session, write_ret), tv))
                goto cleanup;
            continue ;
        }
        total_sent += static_cast<ft_size_t>(write_ret);
    }

    result_body = api_tls_async_receive_response(ssl_session, socket_fd, data->method, tv, status);

cleanup:
    if (data->callback)
        data->callback(result_body, status, data->user_data);
    if (ssl_session)
    {
        SSL_shutdown(ssl_session);
        SSL_free(ssl_session);
    }
    if (socket_fd >= 0)
        nw_close(socket_fd);
    if (context)
        SSL_CTX_free(context);
    if (data->host)
        cma_free(data->host);
    if (data->method)
        cma_free(data->method);
    if (data->path)
        cma_free(data->path);
    if (data->headers)
        cma_free(data->headers);
    cma_free(data);
    return ;
}

ft_bool    api_request_string_tls_async(const char *host, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout)
{
    if (!host || !method || !path || !callback)
        return (FT_FALSE);
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_tls_async)
    {
        return (hooks->request_string_tls_async(host, port, method, path,
                callback, user_data, payload, headers, timeout,
                hooks->user_data));
    }
    api_tls_async_request *data = static_cast<api_tls_async_request*>(cma_malloc(sizeof(api_tls_async_request)));
    if (!data)
        return (FT_FALSE);
    ft_bzero(data, sizeof(api_tls_async_request));
    data->host = adv_strdup(host);
    data->method = adv_strdup(method);
    data->path = adv_strdup(path);
    if (headers)
        data->headers = adv_strdup(headers);
    data->port = port;
    data->payload = payload;
    data->timeout = timeout;
    data->callback = callback;
    data->user_data = user_data;
    if (!data->host || !data->method || !data->path || (headers && !data->headers))
    {
        if (data->host)
            cma_free(data->host);
        if (data->method)
            cma_free(data->method);
        if (data->path)
            cma_free(data->path);
        if (data->headers)
            cma_free(data->headers);
        cma_free(data);
        return (FT_FALSE);
    }
    ft_thread thread_worker(api_tls_async_worker, data);
    thread_worker.detach();
    return (FT_TRUE);
}

struct api_json_async_data
{
    api_json_callback callback;
    void *user_data;
};

static void api_json_async_wrapper(char *body, int32_t status, void *user_data)
{
    api_json_async_data *data;
    json_group *json_body;

    data = static_cast<api_json_async_data*>(user_data);
    json_body = ft_nullptr;
    if (body)
    {
        json_body = json_read_from_string(body);
        cma_free(body);
    }
    if (data && data->callback)
        data->callback(json_body, status, data->user_data);
    if (data)
        cma_free(data);
    return ;
}

ft_bool    api_request_json_tls_async(const char *host, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout)
{
    api_json_async_data *data;

    if (!host || !method || !path || !callback)
        return (FT_FALSE);
    data = static_cast<api_json_async_data*>(cma_malloc(sizeof(api_json_async_data)));
    if (!data)
        return (FT_FALSE);
    data->callback = callback;
    data->user_data = user_data;
    if (!api_request_string_tls_async(host, port, method, path, api_json_async_wrapper,
            data, payload, headers, timeout))
    {
        cma_free(data);
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

#endif

#include "api.hpp"
#include "api_internal.hpp"
#include "api_http_internal.hpp"
#include "api_request_metrics.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/networking.hpp"
#include "../Networking/openssl_support.hpp"
#include "../CPP_class/class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Logger/logger.hpp"
#include "../Printf/printf.hpp"
#include "../Time/time.hpp"
#include <errno.h>
#include <cstdio>
#include <utility>
#include "../Template/move.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t api_request_prepare_connection_handle(api_connection_pool_handle &handle,
    int32_t &error_code) noexcept
{
    int32_t initialize_result = handle.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
        error_code = initialize_result;
    return (initialize_result);
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

#ifdef LIBFT_TEST_BUILD
static api_request_wait_until_ready_hook g_api_request_wait_hook = ft_nullptr;
#endif

#ifdef DEBUG
static void api_request_debug_log(const char *message)
{
    std::FILE *file_pointer;

    file_pointer = std::fopen("api_request_debug.log", "a");
    if (!file_pointer)
        return ;
    std::fprintf(file_pointer, "%s\n", message);
    std::fclose(file_pointer);
    return ;
}
#else
#define api_request_debug_log(message) do { } while (0)
#endif

#ifdef LIBFT_TEST_BUILD
void api_request_set_downgrade_wait_hook(
    api_request_wait_until_ready_hook hook)
{
    g_api_request_wait_hook = hook;
    return ;
}
#endif

ft_bool api_request(const char *ip_address, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout, ft_bool enable_http2,
    ft_bool *used_http2, const api_retry_policy *retry_policy)
{
    ft_bool http2_used_local;
    ft_bool request_success;
    int32_t error_code = FT_ERR_SUCCESS;

    if (used_http2)
        *used_http2 = FT_FALSE;
    if (!streaming_handler)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    http2_used_local = FT_FALSE;
    if (enable_http2)
    {
        request_success = api_request_stream_http2(ip_address, port, method, path,
                streaming_handler, payload, headers, timeout,
                &http2_used_local, retry_policy);
        if (!request_success)
        {
            error_code = FT_ERR_IO;
            if (error_code == FT_ERR_SUCCESS)
                error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        if (used_http2)
            *used_http2 = http2_used_local;
        return (FT_TRUE);
    }
    request_success = api_request_stream(ip_address, port, method, path,
            streaming_handler, payload, headers, timeout, retry_policy);
    if (!request_success)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    if (used_http2)
        *used_http2 = FT_FALSE;
    return (FT_TRUE);
}

ft_bool api_request_stream(const char *ip_address, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout,
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
        ft_log_debug("api_request_stream %s:%u %s %s", log_ip, port,
            log_method, log_path);
    }
    int32_t error_code = FT_ERR_SUCCESS;
    if (!streaming_handler)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_stream)
    {
        ft_bool hook_result = hooks->request_stream(ip_address, port, method, path,
                streaming_handler, payload, headers, timeout, retry_policy,
                hooks->user_data);
        if (!hook_result)
            error_code = FT_ERR_IO;
        return (hook_result);
    }

    SocketConfig config;
    int32_t config_error;

    config_error = config.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        error_code = config_error;
        return (FT_FALSE);
    }
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::PLAIN, ft_nullptr);
    if (!pooled_connection)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (FT_FALSE);
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            if (api_is_configuration_socket_error(socket_setup_error))
                error_code = socket_setup_error;
            else
                error_code = FT_ERR_SOCKET_CONNECT_FAILED;
            api_retry_circuit_record_failure(connection_handle, retry_policy);
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
    ft_bool result;

    result = api_http_execute_plain_streaming(connection_handle, method, path,
            ip_address, payload, headers, timeout, ip_address, port, retry_policy,
            streaming_handler, error_code);
    if (!result)
        return (FT_FALSE);
    connection_guard.set_success();
    return (FT_TRUE);
}

ft_bool api_request_stream_http2(const char *ip_address, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout, ft_bool *used_http2,
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
        ft_log_debug("api_request_stream_http2 %s:%u %s %s", log_ip, port,
            log_method, log_path);
    }
    int32_t error_code = FT_ERR_SUCCESS;
    if (!streaming_handler)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_stream_http2)
    {
        ft_bool hook_result = hooks->request_stream_http2(ip_address, port, method, path,
                streaming_handler, payload, headers, timeout, used_http2,
                retry_policy, hooks->user_data);
        if (!hook_result)
            error_code = FT_ERR_IO;
        return (hook_result);
    }

    SocketConfig config;
    int32_t config_error;

    config_error = config.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        error_code = config_error;
        return (FT_FALSE);
    }
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::PLAIN, ft_nullptr);
    if (!pooled_connection)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (FT_FALSE);
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            if (api_is_configuration_socket_error(socket_setup_error))
                error_code = socket_setup_error;
            else
                error_code = FT_ERR_SOCKET_CONNECT_FAILED;
            api_retry_circuit_record_failure(connection_handle, retry_policy);
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
    ft_bool http2_used_local;

    http2_used_local = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
    ft_bool result;
    result = api_http_execute_plain_http2_streaming(connection_handle, method,
            path, ip_address, payload, headers, timeout, ip_address, port, retry_policy,
            streaming_handler, http2_used_local, error_code);
    if (!result)
    {
        error_code = FT_ERR_SUCCESS;
        result = api_http_execute_plain_streaming(connection_handle, method,
                path, ip_address, payload, headers, timeout, ip_address, port, retry_policy,
                streaming_handler, error_code);
        if (!result)
            return (FT_FALSE);
        http2_used_local = FT_FALSE;
    }
#else
    error_code = FT_ERR_INVALID_OPERATION;
    return (FT_FALSE);
#endif
    connection_guard.set_success();
    if (used_http2)
        *used_http2 = http2_used_local;
    return (FT_TRUE);
}

char *api_request_string(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
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
        ft_log_debug("api_request_string %s:%u %s %s",
            log_ip, port, log_method, log_path);
    }
    const api_transport_hooks *hooks;
    int32_t error_code = FT_ERR_SUCCESS;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string)
    {
        char *hook_result = hooks->request_string(ip_address, port, method, path, payload,
                headers, status, timeout, retry_policy, hooks->user_data);
        if (!hook_result)
            error_code = FT_ERR_IO;
        return (hook_result);
    }
    ft_size_t metrics_request_bytes;
    char *metrics_payload_string;

    metrics_request_bytes = 0;
    if (method)
        metrics_request_bytes += ft_strlen(method);
    if (path)
        metrics_request_bytes += ft_strlen(path);
    if (headers && headers[0])
        metrics_request_bytes += ft_strlen(headers);
    if (ip_address)
        metrics_request_bytes += ft_strlen(ip_address);
    metrics_payload_string = ft_nullptr;
    if (payload)
        metrics_payload_string = json_write_to_string(payload);
    if (metrics_payload_string)
    {
        metrics_request_bytes += ft_strlen(metrics_payload_string);
        cma_free(metrics_payload_string);
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
    (void)metrics_guard.initialize(ip_address, port, method, path,
        metrics_request_bytes, &metrics_result_body, metrics_status_pointer,
        &error_code);

    SocketConfig config;
    int32_t config_error;

    config_error = config.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        error_code = config_error;
        return (ft_nullptr);
    }
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    if (api_request_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::PLAIN, ft_nullptr);
    if (!pooled_connection)
    {
        api_request_debug_log("request_string: no pooled connection");
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
        {
            api_request_debug_log("request_string: circuit blocked");
            return (ft_nullptr);
        }
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            api_request_debug_log("request_string: socket initialize failed");
            if (api_is_configuration_socket_error(socket_setup_error))
                error_code = socket_setup_error;
            else
                error_code = FT_ERR_SOCKET_CONNECT_FAILED;
            api_retry_circuit_record_failure(connection_handle, retry_policy);
            return (ft_nullptr);
        }
        connection_handle.has_socket = FT_TRUE;
        api_request_debug_log("request_string: socket initialize ok");
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
    metrics_result_body = api_http_execute_plain(connection_handle, method,
            path, ip_address, payload, headers, status, timeout, ip_address, port,
            retry_policy, error_code);
    if (!metrics_result_body)
    {
        api_request_debug_log("request_string: api_http_execute_plain returned null");
        return (ft_nullptr);
    }
    connection_guard.set_success();
    api_request_debug_log("request_string: success");
    return (metrics_result_body);
}

char *api_request_string_http2(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
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
        ft_log_debug("api_request_string_http2 %s:%u %s %s",
            log_ip, port, log_method, log_path);
    }
    const api_transport_hooks *hooks;
    int32_t error_code = FT_ERR_SUCCESS;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_http2)
    {
        char *hook_result = hooks->request_string_http2(ip_address, port, method, path,
                payload, headers, status, timeout, used_http2, retry_policy,
                hooks->user_data);
        if (!hook_result)
            error_code = FT_ERR_IO;
        return (hook_result);
    }
    ft_size_t metrics_request_bytes;
    char *metrics_payload_string;

    metrics_request_bytes = 0;
    if (method)
        metrics_request_bytes += ft_strlen(method);
    if (path)
        metrics_request_bytes += ft_strlen(path);
    if (headers && headers[0])
        metrics_request_bytes += ft_strlen(headers);
    if (ip_address)
        metrics_request_bytes += ft_strlen(ip_address);
    metrics_payload_string = ft_nullptr;
    if (payload)
        metrics_payload_string = json_write_to_string(payload);
    if (metrics_payload_string)
    {
        metrics_request_bytes += ft_strlen(metrics_payload_string);
        cma_free(metrics_payload_string);
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
    (void)metrics_guard.initialize(ip_address, port, method, path,
        metrics_request_bytes, &metrics_result_body, metrics_status_pointer,
        &error_code);

    SocketConfig config;
    int32_t config_error;

    config_error = config.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        error_code = config_error;
        return (ft_nullptr);
    }
    config._type = SocketType::CLIENT;
    api_request_copy_ip(config._ip, ip_address);
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;

    api_connection_pool_handle connection_handle;
    ft_bool pooled_connection;
    ft_bool downgrade_due_to_connect_failure;
    if (api_request_prepare_connection_handle(connection_handle, error_code) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    downgrade_due_to_connect_failure = FT_FALSE;
    pooled_connection = api_connection_pool_acquire(connection_handle, ip_address, port,
            api_connection_security_mode::PLAIN, ft_nullptr);
    if (!pooled_connection)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (ft_nullptr);
        int32_t socket_setup_error;

        socket_setup_error = connection_handle.socket.initialize(config);
        if (socket_setup_error != FT_ERR_SUCCESS)
        {
            if (api_is_configuration_socket_error(socket_setup_error))
                error_code = socket_setup_error;
            else
            {
                error_code = FT_ERR_SOCKET_CONNECT_FAILED;
                downgrade_due_to_connect_failure = FT_TRUE;
            }
            api_retry_circuit_record_failure(connection_handle, retry_policy);
            if (!downgrade_due_to_connect_failure)
                return (ft_nullptr);
        }
        if (!downgrade_due_to_connect_failure)
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
    if (downgrade_due_to_connect_failure)
    {
#ifdef LIBFT_TEST_BUILD
        if (g_api_request_wait_hook)
        {
            ft_bool wait_ready;

            wait_ready = g_api_request_wait_hook();
            if (!wait_ready)
                return (ft_nullptr);
        }
#endif
#if NETWORKING_HAS_OPENSSL
        int32_t max_attempts;
        int32_t attempt_index;
        int32_t initial_delay;
        int32_t current_delay;
        int32_t max_delay;
        int32_t multiplier;

        max_attempts = api_retry_get_max_attempts(retry_policy);
        initial_delay = api_retry_get_initial_delay(retry_policy);
        max_delay = api_retry_get_max_delay(retry_policy);
        multiplier = api_retry_get_multiplier(retry_policy);
        current_delay = api_retry_prepare_delay(initial_delay, max_delay);
        attempt_index = 0;
        while (attempt_index < max_attempts)
        {
            ft_bool socket_ready;
            ft_bool should_retry;

            if (!api_retry_circuit_allow(connection_handle, retry_policy,
                    error_code))
                return (ft_nullptr);
            error_code = FT_ERR_SUCCESS;
            socket_ready = api_http_prepare_plain_socket(connection_handle, ip_address,
                    port, timeout, error_code);
            if (socket_ready)
            {
                metrics_result_body = api_http_execute_plain(connection_handle,
                        method, path, ip_address, payload, headers, status, timeout,
                        ip_address, port, retry_policy, error_code);
                if (metrics_result_body)
                {
                    connection_guard.set_success();
                    if (used_http2)
                        *used_http2 = FT_FALSE;
                    return (metrics_result_body);
                }
            }
            should_retry = api_http_should_retry_plain(error_code);
            if (!should_retry)
                return (ft_nullptr);
            api_retry_circuit_record_failure(connection_handle, retry_policy);
            api_connection_pool_evict(connection_handle);
            attempt_index = attempt_index + 1;
            if (attempt_index >= max_attempts)
                break ;
            if (current_delay > 0)
            {
                int32_t sleep_delay;

                sleep_delay = api_retry_prepare_delay(current_delay, max_delay);
                if (sleep_delay > 0)
                    time_sleep_ms(static_cast<uint32_t>(sleep_delay));
            }
            current_delay = api_retry_next_delay(current_delay, max_delay,
                    multiplier);
            if (current_delay <= 0)
                current_delay = api_retry_prepare_delay(initial_delay,
                        max_delay);
        }
#else
        error_code = FT_ERR_INVALID_OPERATION;
        if (used_http2)
            *used_http2 = FT_FALSE;
        return (ft_nullptr);
#endif
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        if (used_http2)
            *used_http2 = FT_FALSE;
        return (ft_nullptr);
    }
    if (!connection_handle.has_socket)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    ft_bool http2_used_local;

    http2_used_local = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
    metrics_result_body = api_http_execute_plain_http2(connection_handle,
            method, path, ip_address, payload, headers, status, timeout, ip_address, port,
            retry_policy, http2_used_local, error_code);
    if (!metrics_result_body)
    {
        ft_bool fallback_socket_alive;
        int32_t previous_error_code;

        fallback_socket_alive = FT_FALSE;
        if (connection_handle.has_socket)
            fallback_socket_alive = api_http_plain_socket_is_alive(connection_handle);
        if (!fallback_socket_alive)
        {
            connection_handle.socket.close_socket();
            connection_handle.has_socket = FT_FALSE;
            connection_handle.from_pool = FT_FALSE;
        }
        connection_handle.should_store = pooled_connection;
        connection_handle.negotiated_http2 = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
        connection_handle.tls_session = ft_nullptr;
        connection_handle.tls_context = ft_nullptr;
#endif
        previous_error_code = error_code;
        error_code = FT_ERR_SUCCESS;
        metrics_result_body = api_http_execute_plain(connection_handle, method,
                path, ip_address, payload, headers, status, timeout, ip_address, port,
                retry_policy, error_code);
        if (!metrics_result_body)
        {
            if (previous_error_code != FT_ERR_SUCCESS
                && previous_error_code != FT_ERR_SOCKET_CONNECT_FAILED
                && error_code == FT_ERR_SOCKET_CONNECT_FAILED)
            {
                error_code = previous_error_code;
            }
            return (ft_nullptr);
        }
        http2_used_local = FT_FALSE;
    }
#else
    error_code = FT_ERR_INVALID_OPERATION;
    return (ft_nullptr);
#endif
    connection_guard.set_success();
    if (used_http2)
        *used_http2 = http2_used_local;
    return (metrics_result_body);
}

json_group *api_request_json(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string(ip_address, port, method, path, payload,
            headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

json_group *api_request_json_http2(const char *ip_address, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout, ft_bool *used_http2,
    const api_retry_policy *retry_policy)
{
    ft_bool http2_used_local;
    char *body;

    http2_used_local = FT_FALSE;
    int32_t error_code = FT_ERR_SUCCESS;
    body = api_request_string_http2(ip_address, port, method, path, payload,
            headers, status, timeout, &http2_used_local, retry_policy);
    if (used_http2)
        *used_http2 = http2_used_local;
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

ft_bool api_request_stream_host(const char *host, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!host || !method || !path || !streaming_handler)
        return (FT_FALSE);
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_stream_host)
    {
        return (hooks->request_stream_host(host, port, method, path,
                streaming_handler, payload, headers, timeout, retry_policy,
                hooks->user_data));
    }
    networking_resolved_address resolved_address;
    char port_string[6];
    char ip_buffer[INET6_ADDRSTRLEN];

    pf_snprintf(port_string, sizeof(port_string), "%u", port);
    if (!networking_dns_resolve_first(host, port_string, AF_UNSPEC,
            SOCK_STREAM, 0, 0, resolved_address))
    {
        return (FT_FALSE);
    }
    if (!networking_resolved_address_to_string(resolved_address, ip_buffer,
            sizeof(ip_buffer)))
    {
        return (FT_FALSE);
    }
    return (api_request_stream(ip_buffer, port, method, path,
        streaming_handler, payload, headers, timeout, retry_policy));
}

char *api_request_string_host(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!host || !method || !path)
        return (ft_nullptr);
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_host)
    {
        char *hook_result = hooks->request_string_host(host, port, method,
                path, payload, headers, status, timeout, retry_policy,
                hooks->user_data);
        return (hook_result);
    }
    networking_resolved_address resolved_address;
    char port_string[6];
    char ip_buffer[INET6_ADDRSTRLEN];

    pf_snprintf(port_string, sizeof(port_string), "%u", port);
    if (!networking_dns_resolve_first(host, port_string, AF_UNSPEC,
            SOCK_STREAM, 0, 0, resolved_address))
    {
        return (ft_nullptr);
    }
    if (!networking_resolved_address_to_string(resolved_address, ip_buffer,
            sizeof(ip_buffer)))
    {
        return (ft_nullptr);
    }
    char *result = api_request_string(ip_buffer, port, method, path, payload,
            headers, status, timeout, retry_policy);
    return (result);
}

json_group *api_request_json_host(const char *host, uint16_t port,
    const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_host(host, port, method, path, payload,
            headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

char *api_request_string_bearer(const char *ip_address, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!token)
        return (api_request_string(ip_address, port, method, path, payload,
                                   headers, status, timeout, retry_policy));
    ft_string header_string;

    if (header_string.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);

    if (headers && headers[0])
    {
        header_string = headers;
        header_string += "\r\n";
    }
    header_string += "Authorization: Bearer ";
    header_string += token;
    return (api_request_string(ip_address, port, method, path, payload,
                               header_string.c_str(), status, timeout,
                               retry_policy));
}

json_group *api_request_json_bearer(const char *ip_address, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_bearer(ip_address, port, method, path, token,
            payload, headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

char *api_request_string_basic(const char *ip_address, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!credentials)
        return (api_request_string(ip_address, port, method, path, payload,
                                   headers, status, timeout, retry_policy));
    ft_string header_string;

    if (header_string.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);

    if (headers && headers[0])
    {
        header_string = headers;
        header_string += "\r\n";
    }
    header_string += "Authorization: Basic ";
    header_string += credentials;
    return (api_request_string(ip_address, port, method, path, payload,
                               header_string.c_str(), status, timeout,
                               retry_policy));
}

json_group *api_request_json_basic(const char *ip_address, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_basic(ip_address, port, method, path, credentials,
            payload, headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

char *api_request_string_host_bearer(const char *host, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!token)
        return (api_request_string_host(host, port, method, path, payload,
                                        headers, status, timeout,
                                        retry_policy));
    ft_string header_string;

    if (header_string.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);

    if (headers && headers[0])
    {
        header_string = headers;
        header_string += "\r\n";
    }
    header_string += "Authorization: Bearer ";
    header_string += token;
    return (api_request_string_host(host, port, method, path, payload,
                                   header_string.c_str(), status, timeout,
                                   retry_policy));
}

json_group *api_request_json_host_bearer(const char *host, uint16_t port,
    const char *method, const char *path, const char *token,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_host_bearer(host, port, method, path, token,
            payload, headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

char *api_request_string_host_basic(const char *host, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (!credentials)
        return (api_request_string_host(host, port, method, path, payload,
                                        headers, status, timeout,
                                        retry_policy));
    ft_string header_string;

    if (header_string.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);

    if (headers && headers[0])
    {
        header_string = headers;
        header_string += "\r\n";
    }
    header_string += "Authorization: Basic ";
    header_string += credentials;
    return (api_request_string_host(host, port, method, path, payload,
                                   header_string.c_str(), status, timeout,
                                   retry_policy));
}

json_group *api_request_json_host_basic(const char *host, uint16_t port,
    const char *method, const char *path, const char *credentials,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_host_basic(host, port, method, path,
            credentials, payload, headers, status, timeout,
            retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

static ft_bool parse_url(const char *url, ft_bool &tls, ft_string &host,
    uint16_t &port, ft_string &path, int32_t &error_code)
{
    const char *scheme_end;
    const char *path_start;
    const char *walker;
    ft_string scheme;
    ft_string hostport;
    const char *colon;
    const char *slash;
    const char *host_start;
    int32_t initialize_result;

    if (!url)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    (void)host.destroy();
    initialize_result = host.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        error_code = initialize_result;
        return (FT_FALSE);
    }
    (void)path.destroy();
    initialize_result = path.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        error_code = initialize_result;
        return (FT_FALSE);
    }
    initialize_result = scheme.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        error_code = initialize_result;
        return (FT_FALSE);
    }
    initialize_result = hostport.initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        (void)scheme.destroy();
        error_code = initialize_result;
        return (FT_FALSE);
    }
    scheme_end = ft_strstr(url, "://");
    if (!scheme_end)
    {
        (void)hostport.destroy();
        (void)scheme.destroy();
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    walker = url;
    while (walker < scheme_end)
    {
        scheme.append(*walker);
        int32_t scheme_error = scheme.get_error();
        if (scheme_error != FT_ERR_SUCCESS)
        {
            (void)hostport.destroy();
            (void)scheme.destroy();
            error_code = scheme_error;
            return (FT_FALSE);
        }
        walker++;
    }
    tls = (scheme == "https");
    host_start = scheme_end + 3;
    path_start = ft_strchr(host_start, '/');
    path.clear();
    if (path_start)
    {
        path.append(path_start);
        int32_t path_error = path.get_error();
        if (path_error != FT_ERR_SUCCESS)
        {
            (void)hostport.destroy();
            (void)scheme.destroy();
            error_code = path_error;
            return (FT_FALSE);
        }
    }
    else
    {
        slash = "/";
        path = slash;
        int32_t path_error = path.get_error();
        if (path_error != FT_ERR_SUCCESS)
        {
            (void)hostport.destroy();
            (void)scheme.destroy();
            error_code = path_error;
            return (FT_FALSE);
        }
    }
    if (path_start)
    {
        hostport.clear();
        walker = host_start;
        while (walker < path_start)
        {
            hostport.append(*walker);
            int32_t hostport_error = hostport.get_error();
            if (hostport_error != FT_ERR_SUCCESS)
            {
                (void)hostport.destroy();
                (void)scheme.destroy();
                error_code = hostport_error;
                return (FT_FALSE);
            }
            walker++;
        }
    }
    else
    {
        hostport.clear();
        hostport.append(host_start);
        int32_t hostport_error = hostport.get_error();
        if (hostport_error != FT_ERR_SUCCESS)
        {
            (void)hostport.destroy();
            (void)scheme.destroy();
            error_code = hostport_error;
            return (FT_FALSE);
        }
    }
    colon = ft_strchr(hostport.c_str(), ':');
    if (colon)
    {
        host.clear();
        walker = hostport.c_str();
        while (walker < colon)
        {
            host.append(*walker);
            int32_t host_error = host.get_error();
            if (host_error != FT_ERR_SUCCESS)
            {
                (void)hostport.destroy();
                (void)scheme.destroy();
                error_code = host_error;
                return (FT_FALSE);
            }
            walker++;
        }
        port = static_cast<uint16_t>(ft_atoi(colon + 1));
    }
    else
    {
        host = hostport;
        int32_t host_error = host.get_error();
        if (host_error != FT_ERR_SUCCESS)
        {
            (void)hostport.destroy();
            (void)scheme.destroy();
            error_code = host_error;
            return (FT_FALSE);
        }
        if (tls)
            port = 443;
        else
            port = 80;
    }
    (void)hostport.destroy();
    (void)scheme.destroy();
    error_code = FT_ERR_SUCCESS;
    return (FT_TRUE);
}

char *api_request_string_url(const char *url, const char *method,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    if (ft_log_get_api_logging())
    {
        const char *log_url = "(null)";
        const char *log_method = "(null)";
        if (url)
            log_url = url;
        if (method)
            log_method = method;
        ft_log_debug("api_request_string_url %s %s",
            log_url, log_method);
    }
    int32_t error_code = FT_ERR_SUCCESS;
    if (!method)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    ft_bool tls;
    ft_string host;
    ft_string path;
    uint16_t port;
    if (!parse_url(url, tls, host, port, path, error_code))
        return (ft_nullptr);
    char *result;
    if (tls)
    {
#if NETWORKING_HAS_OPENSSL
        result = api_request_string_tls(host.c_str(), port, method,
                path.c_str(), payload, headers, status, timeout,
                retry_policy);
#else
        error_code = FT_ERR_INVALID_OPERATION;
        return (ft_nullptr);
#endif
    }
    else
        result = api_request_string_host(host.c_str(), port, method,
                path.c_str(), payload, headers, status, timeout,
                retry_policy);
    if (!result)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

json_group *api_request_json_url(const char *url, const char *method,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy)
{
    int32_t error_code = FT_ERR_SUCCESS;
    char *body = api_request_string_url(url, method, payload,
            headers, status, timeout, retry_policy);
    if (!body)
    {
        error_code = FT_ERR_IO;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
    {
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    error_code = FT_ERR_SUCCESS;
    return (result);
}

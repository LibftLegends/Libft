#include "api.hpp"
#include "api_internal.hpp"
#include "api_http_common.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/openssl_support.hpp"
#include "../CPP_class/class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Printf/printf.hpp"
#include "../Threading/thread.hpp"
#include "../Time/time.hpp"
#include <atomic>
#include <new>
#include <errno.h>
#include <limits.h>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/select.h>
#endif

static std::atomic<ft_size_t> g_api_async_last_request_size(0);
static std::atomic<ft_size_t> g_api_async_last_bytes_sent(0);
static std::atomic<ft_size_t> g_api_async_last_bytes_received(0);
static std::atomic<int32_t> g_api_async_last_send_state(0);
static std::atomic<int32_t> g_api_async_last_send_timeout(0);
static std::atomic<int32_t> g_api_async_last_receive_state(0);
static std::atomic<int32_t> g_api_async_last_receive_timeout(0);

static void api_request_reset_async_debug_counters(void)
{
    g_api_async_last_request_size.store(0);
    g_api_async_last_bytes_sent.store(0);
    g_api_async_last_bytes_received.store(0);
    g_api_async_last_send_state.store(0);
    g_api_async_last_send_timeout.store(0);
    g_api_async_last_receive_state.store(0);
    g_api_async_last_receive_timeout.store(0);
    return ;
}

struct api_async_request
{
    const char *ip_address;
    uint16_t port;
    const char *method;
    const char *path;
    json_group *payload;
    const char *headers;
    int32_t timeout;
    api_callback callback;
    void *user_data;
};

static ft_bool api_async_add_size(ft_size_t &total, ft_size_t size)
{
    if (size > FT_SYSTEM_SIZE_MAX - total)
        return (FT_FALSE);
    total += size;
    return (FT_TRUE);
}

static ft_bool api_async_measure_request(const api_async_request &data,
    ft_size_t &request_size)
{
    constexpr ft_size_t REQUEST_OVERHEAD = sizeof(" " " HTTP/1.1\r\nHost: "
        "\r\nConnection: keep-alive\r\n\r\n") - 1U;
    constexpr ft_size_t JSON_OVERHEAD = sizeof("\r\nContent-Type: application/json"
        "\r\nContent-Length: ") - 1U;
    request_size = 0;
    ft_size_t total = REQUEST_OVERHEAD;
    const ft_size_t host_size = ft_strlen_size_t(data.ip_address);
    if (!api_async_add_size(total, ft_strlen_size_t(data.method))
        || !api_async_add_size(total, ft_strlen_size_t(data.path))
        || !api_async_add_size(total, host_size))
        return (FT_FALSE);
    if (data.headers)
    {
        const char *begin = data.headers;
        const char *end = begin + ft_strlen_size_t(begin);
        while (begin < end && (*begin == '\r' || *begin == '\n'))
            ++begin;
        while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
            --end;
        if (begin < end)
        {
            if (!(host_size >= 2U && data.ip_address[host_size - 2U] == '\r'
                    && data.ip_address[host_size - 1U] == '\n')
                && !api_async_add_size(total, 2U))
                return (FT_FALSE);
            if (!api_async_add_size(total, static_cast<ft_size_t>(end - begin)))
                return (FT_FALSE);
        }
    }
    if (data.payload)
    {
        ft_size_t payload_size = 0;
        if (!api_http_measure_json_payload(data.payload, payload_size)
            || !api_async_add_size(total, payload_size))
            return (FT_FALSE);
        ft_size_t decimal_digits = 1;
        while (payload_size >= 10U)
        {
            payload_size /= 10U;
            ++decimal_digits;
        }
        if (!api_async_add_size(total, JSON_OVERHEAD + decimal_digits))
            return (FT_FALSE);
    }
    request_size = total;
    return (FT_TRUE);
}

static void api_async_calculate_timeouts(ft_size_t request_size, int32_t timeout_ms,
    int32_t &send_timeout_ms, int32_t &receive_timeout_ms)
{
    ft_size_t request_kib = request_size / 1024U + (request_size % 1024U != 0U);
    if (request_kib > static_cast<ft_size_t>(INT32_MAX))
        request_kib = INT32_MAX;
    send_timeout_ms = timeout_ms;
    if (timeout_ms > 0 && request_kib > static_cast<ft_size_t>(timeout_ms))
        send_timeout_ms = static_cast<int32_t>(request_kib);
    if (send_timeout_ms < 10000)
        send_timeout_ms = 10000;

    int64_t receive_timeout = static_cast<int64_t>(send_timeout_ms)
        + static_cast<int64_t>(request_kib);
    if (receive_timeout < 15000)
        receive_timeout = 15000;
    if (receive_timeout > INT32_MAX)
        receive_timeout = INT32_MAX;
    receive_timeout_ms = static_cast<int32_t>(receive_timeout);
    return ;
}

static void api_async_worker(api_async_request *data)
{
    ft_size_t request_size = 0;
    api_retry_policy retry_policy;
    int32_t status;
    char *result_body;
    ft_bool request_prepared;
    int32_t send_timeout_ms;
    int32_t receive_timeout_ms;
    ft_size_t body_length;

    if (!data)
        return ;
    status = -1;
    result_body = ft_nullptr;
    api_request_reset_async_debug_counters();
    request_prepared = api_async_measure_request(*data, request_size);
    g_api_async_last_request_size.store(request_size);
    api_async_calculate_timeouts(request_size, data->timeout,
        send_timeout_ms, receive_timeout_ms);
    g_api_async_last_send_timeout.store(send_timeout_ms);
    g_api_async_last_receive_timeout.store(receive_timeout_ms);
    if (retry_policy.initialize() == FT_ERR_SUCCESS)
    {
        retry_policy.set_max_attempts(10);
        retry_policy.set_initial_delay_ms(100);
        retry_policy.set_max_delay_ms(500);
        retry_policy.set_backoff_multiplier(2);
    }
    if (request_prepared)
    {
        result_body = api_request_string_host(data->ip_address, data->port,
                data->method, data->path, data->payload, data->headers,
                &status, receive_timeout_ms, &retry_policy);
    }
    (void)retry_policy.destroy();
    if (result_body)
    {
        body_length = ft_strlen_size_t(result_body);
        g_api_async_last_bytes_sent.store(request_size);
        g_api_async_last_send_state.store(5);
        g_api_async_last_bytes_received.store(body_length);
        g_api_async_last_receive_state.store(5);
    }
    else
    {
        g_api_async_last_bytes_sent.store(0);
        g_api_async_last_send_state.store(9);
        g_api_async_last_bytes_received.store(0);
        g_api_async_last_receive_state.store(9);
    }
    const api_callback callback = data->callback;
    void *const user_data = data->user_data;
    cma_free(data);
    callback(result_body, status, user_data);
    return ;
}

ft_size_t  api_debug_get_last_async_request_size(void)
{
    return (g_api_async_last_request_size.load());
}

ft_size_t  api_debug_get_last_async_bytes_sent(void)
{
    return (g_api_async_last_bytes_sent.load());
}

int32_t     api_debug_get_last_async_send_state(void)
{
    return (g_api_async_last_send_state.load());
}

int32_t     api_debug_get_last_async_send_timeout(void)
{
    return (g_api_async_last_send_timeout.load());
}

ft_size_t  api_debug_get_last_async_bytes_received(void)
{
    return (g_api_async_last_bytes_received.load());
}

int32_t     api_debug_get_last_async_receive_state(void)
{
    return (g_api_async_last_receive_state.load());
}

int32_t     api_debug_get_last_async_receive_timeout(void)
{
    return (g_api_async_last_receive_timeout.load());
}

static const char *api_async_copy_text(char *&output, const char *input, ft_size_t size)
{
    char *const start = output;
    ft_memcpy(output, input, size);
    output += size;
    return (start);
}

ft_bool    api_request_string_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout)
{
    if (!ip_address || !method || !path || !callback)
    {
        return (FT_FALSE);
    }
    const api_transport_hooks *hooks;

    hooks = api_get_transport_hooks();
    if (hooks && hooks->request_string_async)
    {
        return (hooks->request_string_async(ip_address, port, method, path, callback,
                user_data, payload, headers, timeout, hooks->user_data));
    }
    constexpr ft_size_t REQUEST_SIZE = sizeof(api_async_request);
    const ft_size_t host_size = ft_strlen_size_t(ip_address) + 1U;
    const ft_size_t method_size = ft_strlen_size_t(method) + 1U;
    const ft_size_t path_size = ft_strlen_size_t(path) + 1U;
    ft_size_t headers_size = 0;
    if (headers)
        headers_size = ft_strlen_size_t(headers) + 1U;
    ft_size_t storage_size = REQUEST_SIZE;
    if (!api_async_add_size(storage_size, host_size)
        || !api_async_add_size(storage_size, method_size)
        || !api_async_add_size(storage_size, path_size)
        || !api_async_add_size(storage_size, headers_size))
        return (FT_FALSE);
    void *const storage = cma_malloc(storage_size);
    if (!storage)
        return (FT_FALSE);
    auto *data = new (storage) api_async_request{};
    char *text_storage = static_cast<char *>(storage) + REQUEST_SIZE;
    data->ip_address = api_async_copy_text(text_storage, ip_address, host_size);
    data->method = api_async_copy_text(text_storage, method, method_size);
    data->path = api_async_copy_text(text_storage, path, path_size);
    if (headers)
        data->headers = api_async_copy_text(text_storage, headers, headers_size);
    data->port = port;
    data->payload = payload;
    data->timeout = timeout;
    data->callback = callback;
    data->user_data = user_data;
    ft_thread thread_worker(api_async_worker, data);
    if (!thread_worker.joinable())
    {
        cma_free(data);
        return (FT_FALSE);
    }
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

ft_bool    api_request_json_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout)
{
    api_json_async_data *data;

    if (!ip_address || !method || !path || !callback)
    {
        return (FT_FALSE);
    }
    data = static_cast<api_json_async_data*>(cma_malloc(sizeof(api_json_async_data)));
    if (!data)
    {
        return (FT_FALSE);
    }
    data->callback = callback;
    data->user_data = user_data;
    if (!api_request_string_async(ip_address, port, method, path, api_json_async_wrapper,
            data, payload, headers, timeout))
    {
        cma_free(data);
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_bool    api_request_string_http2_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout,
        ft_bool *used_http2)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
    return (api_request_string_async(ip_address, port, method, path, callback,
            user_data, payload, headers, timeout));
}

ft_bool    api_request_string_tls_http2_async(const char *host, uint16_t port,
        const char *method, const char *path, api_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout,
        ft_bool *used_http2)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
    return (api_request_string_tls_async(host, port, method, path, callback,
            user_data, payload, headers, timeout));
#else
    (void)host;
    (void)port;
    (void)method;
    (void)path;
    (void)callback;
    (void)user_data;
    (void)payload;
    (void)headers;
    (void)timeout;
    return (FT_FALSE);
#endif
}

ft_bool    api_request_json_http2_async(const char *ip_address, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout,
        ft_bool *used_http2)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
    return (api_request_json_async(ip_address, port, method, path, callback,
            user_data, payload, headers, timeout));
}

ft_bool    api_request_json_tls_http2_async(const char *host, uint16_t port,
        const char *method, const char *path, api_json_callback callback,
        void *user_data, json_group *payload, const char *headers, int32_t timeout,
        ft_bool *used_http2)
{
    if (used_http2)
        *used_http2 = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
    return (api_request_json_tls_async(host, port, method, path, callback,
            user_data, payload, headers, timeout));
#else
    (void)host;
    (void)port;
    (void)method;
    (void)path;
    (void)callback;
    (void)user_data;
    (void)payload;
    (void)headers;
    (void)timeout;
    return (FT_FALSE);
#endif
}

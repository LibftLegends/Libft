#include "../test_internal.hpp"
#include "../../Modules/API/api.hpp"
#include "../../Modules/API/api_internal.hpp"
#include "../../Modules/API/api_http_internal.hpp"
#include "../../Modules/Networking/socket_class.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Networking/http2_client.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Threading/thread.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Printf/printf.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Time/time.hpp"
#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <climits>
#include <atomic>
#include <chrono>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#ifdef _WIN32
# include <windows.h>
#else
# if defined(_WIN32) || defined(_WIN64)
#  include <winsock2.h>
#  include <ws2tcpip.h>
# else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
# endif
# include <unistd.h>
#endif

static void api_request_noop_callback(char *body, int status, void *user_data)
{
    (void)body;
    (void)status;
    (void)user_data;
    return ;
}

static ft_bool api_request_local_sockets_available(void)
{
    int32_t socket_fd;

    errno = 0;
    socket_fd = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd >= 0)
    {
        nw_close(socket_fd);
        return (FT_TRUE);
    }
    if (errno == EPERM || errno == EACCES)
        return (FT_FALSE);
    return (FT_TRUE);
}

#if NETWORKING_HAS_OPENSSL
FT_TEST(test_api_request_hmac_signature_basic)
{
    api_hmac_signature_input input;
    unsigned char key_buffer[6];
    ft_string signature;
    size_t index;

    input.method = "POST";
    input.path = "/v1/resource";
    input.canonical_headers = "content-type:application/json";
    input.canonical_query = ft_nullptr;
    input.body = "{\"ok\":true}";
    index = 0;
    while (index < sizeof(key_buffer))
    {
        key_buffer[index] = "secret"[index];
        index += 1;
    }
    signature = "";
    if (api_sign_request_hmac_sha256(input, key_buffer,
            sizeof(key_buffer), signature) != 0)
        return (0);
    if (signature.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (ft_strcmp(signature.c_str(),
            "jyahK7KdXeTAsWB9y99qdzuUS5V6UK8fdyx51G18uBY=") != 0)
        return (0);
    return (1);
}

FT_TEST(test_api_request_oauth1_header_hmac_sha256)
{
    api_oauth1_parameter extras[1];
    api_oauth1_parameters params;
    ft_string header;

    extras[0].key = "status";
    extras[0].value = "active";
    params.method = "POST";
    params.url = "https://api.example.com/resource";
    params.consumer_key = "key123";
    params.consumer_secret = "secret456";
    params.token = "token789";
    params.token_secret = "tokensecret";
    params.timestamp = "1700000000";
    params.nonce = "randomstring";
    params.signature_method = "HMAC-SHA256";
    params.version = "1.0";
    params.additional_parameters = extras;
    params.additional_parameter_count = 1;
    header = "";
    if (api_build_oauth1_authorization_header(params, header) != 0)
        return (0);
    if (header.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (ft_strcmp(header.c_str(),
            "Authorization: OAuth oauth_consumer_key=\"key123\", "
            "oauth_nonce=\"randomstring\", oauth_signature_method=\"HMAC-SHA256\", "
            "oauth_timestamp=\"1700000000\", oauth_token=\"token789\", "
            "oauth_version=\"1.0\", status=\"active\", "
            "oauth_signature=\"Lih20ttr%2Fmuyb8m5AqpjPKa%2FNzc%2BYT%2FRvRZKId3RS2o%3D\"") != 0)
        return (0);
    return (1);
}
#endif

struct api_stream_test_context
{
    size_t total_bytes;
    size_t chunk_count;
    bool headers_received;
    bool final_received;
    int status_code;
};

static bool g_api_request_test_http2_called = false;
static bool g_api_request_test_http1_called = false;

static ft_bool api_request_test_stream_http2_hook(const char *ip, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers, int timeout, ft_bool *used_http2,
    const api_retry_policy *retry_policy, void *user_data)
{
    (void)ip;
    (void)port;
    (void)method;
    (void)path;
    (void)streaming_handler;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    (void)user_data;
    g_api_request_test_http2_called = true;
    if (used_http2)
        *used_http2 = true;
    return (true);
}

static ft_bool api_request_test_stream_http1_hook(const char *ip, uint16_t port,
    const char *method, const char *path,
    const api_streaming_handler *streaming_handler, json_group *payload,
    const char *headers,     int32_t timeout,
    const api_retry_policy *retry_policy, void *user_data)
{
    (void)ip;
    (void)port;
    (void)method;
    (void)path;
    (void)streaming_handler;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    (void)user_data;
    g_api_request_test_http1_called = true;
    return (true);
}

#if NETWORKING_HAS_OPENSSL
static int api_request_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_http2_frame_get_error_returned = FT_FALSE;
static int32_t g_http2_frame_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_http2_frame_get_error_str_returned = FT_FALSE;
static const char *g_http2_frame_get_error_str_result = ft_nullptr;
static ft_bool g_http2_stream_manager_get_error_returned = FT_FALSE;
static int32_t g_http2_stream_manager_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_http2_stream_manager_get_error_str_returned = FT_FALSE;
static const char *g_http2_stream_manager_get_error_str_result = ft_nullptr;

static void http2_frame_get_error_uninitialised_operation(void)
{
    http2_frame frame;

    g_http2_frame_get_error_result = frame.get_error();
    g_http2_frame_get_error_returned = FT_TRUE;
    return ;
}

static void http2_frame_get_error_str_uninitialised_operation(void)
{
    http2_frame frame;

    g_http2_frame_get_error_str_result = frame.get_error_str();
    g_http2_frame_get_error_str_returned = FT_TRUE;
    return ;
}

static void http2_stream_manager_get_error_uninitialised_operation(void)
{
    http2_stream_manager manager;

    g_http2_stream_manager_get_error_result = manager.get_error();
    g_http2_stream_manager_get_error_returned = FT_TRUE;
    return ;
}

static void http2_stream_manager_get_error_str_uninitialised_operation(void)
{
    http2_stream_manager manager;

    g_http2_stream_manager_get_error_str_result = manager.get_error_str();
    g_http2_stream_manager_get_error_str_returned = FT_TRUE;
    return ;
}
#endif

FT_TEST(test_api_request_prefers_http2_streaming)
{
    api_transport_hooks hooks;
    api_streaming_handler handler;
    ft_bool used_http2;
    ft_bool request_result;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_stream = api_request_test_stream_http1_hook;
    hooks.request_stream_http2 = api_request_test_stream_http2_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    g_api_request_test_http1_called = false;
    g_api_request_test_http2_called = false;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.reset();
    request_result = api_request("127.0.0.1", 8080, "GET", "/", &handler,
            ft_nullptr, ft_nullptr, 0, true, &used_http2, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT(request_result);
    FT_ASSERT(g_api_request_test_http2_called);
    FT_ASSERT(!g_api_request_test_http1_called);
    FT_ASSERT(used_http2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.destroy());
    return (1);
}

FT_TEST(test_api_request_disables_http2_streaming)
{
    api_transport_hooks hooks;
    api_streaming_handler handler;
    ft_bool used_http2;
    ft_bool request_result;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_stream = api_request_test_stream_http1_hook;
    hooks.request_stream_http2 = api_request_test_stream_http2_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    g_api_request_test_http1_called = false;
    g_api_request_test_http2_called = false;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.reset();
    request_result = api_request("127.0.0.1", 8080, "GET", "/", &handler,
            ft_nullptr, ft_nullptr, 0, false, &used_http2, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT(request_result);
    FT_ASSERT(g_api_request_test_http1_called);
    FT_ASSERT(!g_api_request_test_http2_called);
    FT_ASSERT(!used_http2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.destroy());
    return (1);
}

struct [[maybe_unused]] api_request_bearer_server_context
{
    std::atomic<bool> ready;
    std::atomic<int> start_error;
    std::atomic<bool> header_received;
    std::atomic<int> result;
    int client_fd;
    ft_string request_data;
};

struct [[maybe_unused]] api_request_basic_server_context
{
    std::atomic<bool> ready;
    std::atomic<int> start_error;
    std::atomic<bool> basic_header_received;
    std::atomic<bool> basic_header_after_custom;
    std::atomic<int> result;
    int client_fd;
    ft_string request_data;
};

struct api_request_circuit_server_context
{
    std::atomic<bool> ready;
    std::atomic<int> start_error;
    uint16_t port;
    int responses;
};

static void api_request_stream_headers_callback(int status_code,
    const char *headers, void *user_data)
{
    api_stream_test_context *context;

    (void)headers;
    context = static_cast<api_stream_test_context*>(user_data);
    if (!context)
        return ;
    context->headers_received = true;
    context->status_code = status_code;
    return ;
}

static ft_bool api_request_stream_body_callback(const char *chunk_data,
    ft_size_t chunk_size, ft_bool is_final_chunk, void *user_data)
{
    api_stream_test_context *context;

    context = static_cast<api_stream_test_context*>(user_data);
    if (!context)
        return (false);
    if (chunk_data && chunk_size > 0)
    {
        context->total_bytes += chunk_size;
        context->chunk_count += 1;
    }
    if (is_final_chunk)
        context->final_received = true;
    return (true);
}

static char *api_request_test_string_success_hook(const char *ip_address,
    uint16_t port, const char *method, const char *path, json_group *payload,
    const char *headers, int32_t *status, int32_t timeout,
    const api_retry_policy *retry_policy, void *user_data)
{
    char *body;

    (void)ip_address;
    (void)port;
    (void)method;
    (void)path;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    (void)user_data;
    if (status)
        *status = 200;
    body = static_cast<char *>(cma_malloc(1));
    if (!body)
        return (ft_nullptr);
    body[0] = '\0';
    return (body);
}

static char *api_request_test_string_http2_fallback_hook(
    const char *ip_address, uint16_t port, const char *method,
    const char *path, json_group *payload, const char *headers,
    int32_t *status, int32_t timeout, ft_bool *used_http2,
    const api_retry_policy *retry_policy, void *user_data)
{
    char *body;

    (void)ip_address;
    (void)port;
    (void)method;
    (void)path;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    (void)user_data;
    if (status)
        *status = 200;
    if (used_http2)
        *used_http2 = FT_FALSE;
    body = static_cast<char *>(cma_malloc(6));
    if (!body)
        return (ft_nullptr);
    ft_memcpy(body, "Hello", 6);
    return (body);
}

static ft_bool api_request_test_stream_large_response_hook(
    const char *ip_address, uint16_t port, const char *method,
    const char *path, const api_streaming_handler *streaming_handler,
    json_group *payload, const char *headers, int32_t timeout,
    const api_retry_policy *retry_policy, void *user_data)
{
    char chunk_buffer[32768];
    size_t remaining_bytes;
    ft_bool should_continue;

    (void)ip_address;
    (void)port;
    (void)method;
    (void)path;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    if (!streaming_handler || !user_data)
        return (FT_FALSE);
    streaming_handler->invoke_headers_callback(200,
        "HTTP/1.1 200 OK\r\nContent-Length: 2097152\r\n\r\n");
    ft_memset(chunk_buffer, 'A', sizeof(chunk_buffer));
    remaining_bytes = 2 * 1024 * 1024;
    should_continue = FT_TRUE;
    while (remaining_bytes > 0)
    {
        size_t chunk_size;
        ft_bool final_chunk;

        chunk_size = remaining_bytes;
        if (chunk_size > sizeof(chunk_buffer))
            chunk_size = sizeof(chunk_buffer);
        final_chunk = (remaining_bytes == chunk_size) ? FT_TRUE : FT_FALSE;
        if (!streaming_handler->invoke_body_callback(chunk_buffer,
                chunk_size, final_chunk, should_continue))
            return (FT_FALSE);
        if (!should_continue)
            return (FT_FALSE);
        remaining_bytes -= chunk_size;
    }
    return (FT_TRUE);
}

static ft_bool api_request_test_stream_chunked_response_hook(
    const char *ip_address, uint16_t port, const char *method,
    const char *path, const api_streaming_handler *streaming_handler,
    json_group *payload, const char *headers, int32_t timeout,
    const api_retry_policy *retry_policy, void *user_data)
{
    ft_bool should_continue;
    const char first_chunk[] = "0123456789ABCDEF";
    const char second_chunk[] = "ABCDEFGH";

    (void)ip_address;
    (void)port;
    (void)method;
    (void)path;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)retry_policy;
    if (!streaming_handler || !user_data)
        return (FT_FALSE);
    streaming_handler->invoke_headers_callback(200,
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    should_continue = FT_TRUE;
    if (!streaming_handler->invoke_body_callback(first_chunk, 16, FT_FALSE,
            should_continue))
        return (FT_FALSE);
    if (!should_continue)
        return (FT_FALSE);
    if (!streaming_handler->invoke_body_callback(second_chunk, 8, FT_TRUE,
            should_continue))
        return (FT_FALSE);
    if (!should_continue)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool api_request_test_async_success_hook(const char *ip_address,
    uint16_t port, const char *method, const char *path, api_callback callback,
    void *user_data, json_group *payload, const char *headers, int32_t timeout,
    void *transport_user_data)
{
    char *body;

    (void)ip_address;
    (void)port;
    (void)method;
    (void)path;
    (void)payload;
    (void)headers;
    (void)timeout;
    (void)transport_user_data;
    body = static_cast<char *>(cma_malloc(3));
    if (!body)
        return (FT_FALSE);
    ft_memcpy(body, "OK", 3);
    callback(body, 200, user_data);
    return (FT_TRUE);
}

struct api_request_async_test_callback_data
{
    std::atomic<bool> *completed;
    std::atomic<int> *status;
    std::atomic<char *> *body;
};

static void api_request_test_async_callback(char *body, int status,
    void *user_data)
{
    api_request_async_test_callback_data *data;

    data = static_cast<api_request_async_test_callback_data *>(user_data);
    if (!data || !data->completed || !data->status || !data->body)
    {
        if (body)
            cma_free(body);
        return ;
    }
    data->status->store(status, std::memory_order_relaxed);
    data->body->store(body, std::memory_order_relaxed);
    data->completed->store(true, std::memory_order_release);
    return ;
}

static std::atomic<size_t> g_api_async_retry_server_bytes_received(0);
static std::atomic<bool> g_api_async_retry_server_ready(false);
static std::atomic<int> g_api_async_retry_server_start_error(FT_ERR_SUCCESS);
static std::atomic<bool> g_api_async_retry_server_header_complete(false);
static std::atomic<int> g_api_async_retry_server_last_recv_result(0);
static std::atomic<int> g_api_async_retry_server_last_errno(0);
static std::atomic<bool> g_api_request_success_server_ready(false);
static std::atomic<int> g_api_request_success_server_start_error(FT_ERR_SUCCESS);
static std::atomic<bool> g_api_request_stream_large_server_ready(false);
static std::atomic<int> g_api_request_stream_large_server_start_error(FT_ERR_SUCCESS);
static std::atomic<bool> g_api_request_stream_chunked_server_ready(false);
static std::atomic<int> g_api_request_stream_chunked_server_start_error(FT_ERR_SUCCESS);
static std::atomic<bool> g_api_request_send_failure_server_ready(false);
static std::atomic<int> g_api_request_send_failure_server_start_error(FT_ERR_SUCCESS);
static std::atomic<int> g_api_request_send_failure_server_port(0);
static std::atomic<bool> g_api_request_retry_success_server_ready(false);
static std::atomic<int> g_api_request_retry_success_server_start_error(FT_ERR_SUCCESS);


static void api_request_send_failure_server(void);

static void api_request_small_delay(void);

static void api_request_send_failure_server_reset_state(void);
static void api_request_send_failure_server_signal_ready(int error_code);
static bool api_request_send_failure_server_wait_until_ready(void);

static void api_request_send_failure_server(void)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    int config_error;
    int server_port;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        api_request_send_failure_server_signal_ready(config_error);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 0;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        api_request_send_failure_server_signal_ready(initialize_error);
        return ;
    }
    address_length = sizeof(address_storage);
    if (getsockname(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr *>(&address_storage), &address_length) != 0)
    {
        api_request_send_failure_server_signal_ready(cmp_map_system_error_to_ft(errno));
        return ;
    }
    if (address_storage.ss_family == AF_INET)
    {
        const struct sockaddr_in *address_ipv4;

        address_ipv4 = reinterpret_cast<const struct sockaddr_in *>(&address_storage);
        server_port = ntohs(address_ipv4->sin_port);
    }
    else if (address_storage.ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *address_ipv6;

        address_ipv6 = reinterpret_cast<const struct sockaddr_in6 *>(&address_storage);
        server_port = ntohs(address_ipv6->sin6_port);
    }
    else
    {
        api_request_send_failure_server_signal_ready(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    g_api_request_send_failure_server_port.store(server_port, std::memory_order_release);
    api_request_send_failure_server_signal_ready(initialize_error);
    if (server_socket.get_file_descriptor() < 0)
        return ;
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(), reinterpret_cast<struct sockaddr*>(&address_storage), &address_length);
    if (client_fd >= 0)
        nw_close(client_fd);
    return ;
}

static void api_request_small_delay(void)
{
#ifdef _WIN32
    Sleep(100);
#else
    usleep(100000);
#endif
    return ;
}

static bool api_request_send_failure_server_wait_until_ready(void)
{
    while (!g_api_request_send_failure_server_ready.load(std::memory_order_acquire))
        api_request_small_delay();
    return (g_api_request_send_failure_server_start_error.load(std::memory_order_acquire) == FT_ERR_SUCCESS);
}

static void api_request_send_failure_server_reset_state(void)
{
    g_api_request_send_failure_server_ready.store(false, std::memory_order_relaxed);
    g_api_request_send_failure_server_start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    g_api_request_send_failure_server_port.store(0, std::memory_order_relaxed);
}

static void api_request_send_failure_server_signal_ready(int error_code)
{
    g_api_request_send_failure_server_start_error.store(error_code, std::memory_order_relaxed);
    g_api_request_send_failure_server_ready.store(true, std::memory_order_release);
}

[[maybe_unused]] static void api_request_success_server_reset_state(void)
{
    g_api_request_success_server_ready.store(false, std::memory_order_relaxed);
    g_api_request_success_server_start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    return ;
}
[[maybe_unused]] static void api_request_success_server_signal_ready(int error_code)
{
    g_api_request_success_server_start_error.store(error_code, std::memory_order_relaxed);
    g_api_request_success_server_ready.store(true, std::memory_order_release);
    return ;
}
[[maybe_unused]] static void api_request_stream_large_server_reset_state(void)
{
    g_api_request_stream_large_server_ready.store(false, std::memory_order_relaxed);
    g_api_request_stream_large_server_start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    return ;
}

[[maybe_unused]] static void api_request_stream_large_server_signal_ready(int error_code)
{
    g_api_request_stream_large_server_start_error.store(error_code, std::memory_order_relaxed);
    g_api_request_stream_large_server_ready.store(true, std::memory_order_release);
    return ;
}

[[maybe_unused]] static bool api_request_stream_large_server_wait_until_ready(void)
{
    size_t wait_iterations;

    wait_iterations = 0;
    while (!g_api_request_stream_large_server_ready.load(std::memory_order_acquire))
    {
        if (wait_iterations >= 50)
            return (false);
        api_request_small_delay();
        wait_iterations += 1;
    }
    if (g_api_request_stream_large_server_start_error.load(std::memory_order_acquire) != FT_ERR_SUCCESS)
        return (false);
    return (true);
}

[[maybe_unused]] static void api_request_stream_chunked_server_reset_state(void)
{
    g_api_request_stream_chunked_server_ready.store(false, std::memory_order_relaxed);
    g_api_request_stream_chunked_server_start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    return ;
}

[[maybe_unused]] static void api_request_stream_chunked_server_signal_ready(int error_code)
{
    g_api_request_stream_chunked_server_start_error.store(error_code, std::memory_order_relaxed);
    g_api_request_stream_chunked_server_ready.store(true, std::memory_order_release);
    return ;
}

[[maybe_unused]] static bool api_request_stream_chunked_server_wait_until_ready(void)
{
    size_t wait_iterations;

    wait_iterations = 0;
    while (!g_api_request_stream_chunked_server_ready.load(std::memory_order_acquire))
    {
        if (wait_iterations >= 50)
            return (false);
        api_request_small_delay();
        wait_iterations += 1;
    }
    if (g_api_request_stream_chunked_server_start_error.load(std::memory_order_acquire) != FT_ERR_SUCCESS)
        return (false);
    return (true);
}

[[maybe_unused]] static void api_request_retry_success_server_reset_state(void)
{
    g_api_request_retry_success_server_ready.store(false,
        std::memory_order_relaxed);
    g_api_request_retry_success_server_start_error.store(FT_ERR_SUCCESS,
        std::memory_order_relaxed);
    return ;
}

[[maybe_unused]] static bool api_request_retry_success_server_wait_until_ready(void)
{
    while (!g_api_request_retry_success_server_ready.load(
        std::memory_order_acquire))
    {
        api_request_small_delay();
    }
    return (g_api_request_retry_success_server_start_error.load(
        std::memory_order_acquire) == FT_ERR_SUCCESS);
}

[[maybe_unused]] static void api_request_bearer_server(api_request_bearer_server_context *context)
{
    SocketConfig server_configuration;
        struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    char buffer[512];
    ssize_t bytes_received;
    const char *header_location;
    const char *response;
    ssize_t send_result;

    if (!context)
        return ;
    int config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(config_error, std::memory_order_relaxed);
        context->result.store(config_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54365;
    context->client_fd = -1;
    context->request_data.clear();
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(initialize_error, std::memory_order_relaxed);
        context->result.store(initialize_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        context->start_error.store(FT_ERR_INVALID_OPERATION,
            std::memory_order_relaxed);
        context->result.store(FT_ERR_INVALID_OPERATION,
            std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    context->start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    context->result.store(0, std::memory_order_relaxed);
    context->ready.store(true, std::memory_order_release);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(), reinterpret_cast<struct sockaddr*>(&address_storage), &address_length);
    if (client_fd < 0)
    {
        context->result.store(-1, std::memory_order_relaxed);
        return ;
    }
    context->client_fd = client_fd;
    while (1)
    {
        bytes_received = nw_recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
            break;
        buffer[bytes_received] = '\0';
        context->request_data.append(buffer);
        if (ft_strstr(context->request_data.c_str(), "\r\n\r\n"))
            break;
    }
    header_location = ft_strstr(context->request_data.c_str(), "Authorization: Bearer test-token");
    if (header_location)
        context->header_received.store(true, std::memory_order_relaxed);
    response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    send_result = nw_send(client_fd, response, ft_strlen(response), 0);
    if (send_result < 0)
        context->result.store(-2, std::memory_order_relaxed);
    nw_close(client_fd);
    return ;
}

[[maybe_unused]] static void api_request_basic_server(api_request_basic_server_context *context)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    char buffer[512];
    ssize_t bytes_received;
    const char *request_cstring;
    const char *custom_header_location;
    const char *basic_header_location;
    const char *response;
    ssize_t send_result;
    int config_error;

    if (!context)
        return ;
    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(config_error, std::memory_order_relaxed);
        context->result.store(config_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54366;
    context->client_fd = -1;
    context->request_data.clear();
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(initialize_error, std::memory_order_relaxed);
        context->result.store(initialize_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        context->start_error.store(FT_ERR_INVALID_OPERATION,
            std::memory_order_relaxed);
        context->result.store(FT_ERR_INVALID_OPERATION,
            std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    context->start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    context->result.store(0, std::memory_order_relaxed);
    context->ready.store(true, std::memory_order_release);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(), reinterpret_cast<struct sockaddr*>(&address_storage), &address_length);
    if (client_fd < 0)
    {
        context->result.store(-1, std::memory_order_relaxed);
        return ;
    }
    context->client_fd = client_fd;
    while (1)
    {
        bytes_received = nw_recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
            break;
        buffer[bytes_received] = '\0';
        context->request_data.append(buffer);
        if (ft_strstr(context->request_data.c_str(), "\r\n\r\n"))
            break;
    }
    request_cstring = context->request_data.c_str();
    basic_header_location = ft_strstr(request_cstring, "Authorization: Basic dXNlcjpwYXNz");
    if (basic_header_location)
        context->basic_header_received.store(true, std::memory_order_relaxed);
    custom_header_location = ft_strstr(request_cstring, "X-Test-Header: Value");
    if (custom_header_location && basic_header_location && custom_header_location < basic_header_location)
        context->basic_header_after_custom.store(true, std::memory_order_relaxed);
    response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    send_result = nw_send(client_fd, response, ft_strlen(response), 0);
    if (send_result < 0)
        context->result.store(-2, std::memory_order_relaxed);
    nw_close(client_fd);
    return ;
}

[[maybe_unused]] static ft_bool api_request_success_server_wait_until_ready(void)
{
    while (!g_api_request_success_server_ready.load(std::memory_order_acquire))
        api_request_small_delay();
    if (g_api_request_success_server_start_error.load(std::memory_order_acquire) != FT_ERR_SUCCESS)
        return (false);
    return (true);
}

[[maybe_unused]] static void api_request_success_server(void)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    char buffer[512];
    const char *response;
    size_t response_length;
    size_t total_sent;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        api_request_success_server_signal_ready(config_error);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54338;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        api_request_success_server_signal_ready(initialize_error);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        api_request_success_server_signal_ready(FT_ERR_INVALID_OPERATION);
        return ;
    }
    api_request_success_server_signal_ready(FT_ERR_SUCCESS);
    response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
    response_length = ft_strlen(response);
    int remaining_connections;
    bool header_complete;
    int terminator_state;

    remaining_connections = 1;
    while (remaining_connections > 0)
    {
        ssize_t bytes_sent;
        ssize_t bytes_received;
        size_t buffer_index;

        address_length = sizeof(address_storage);
        client_fd = nw_accept(server_socket.get_file_descriptor(),
                              reinterpret_cast<struct sockaddr*>(&address_storage),
                              &address_length);
        if (client_fd < 0)
            break;
        header_complete = false;
        terminator_state = 0;
        while (!header_complete)
        {
            bytes_received = nw_recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
                break;
            buffer[bytes_received] = '\0';
            buffer_index = 0;
            while (buffer_index < static_cast<size_t>(bytes_received))
            {
                if ((terminator_state == 0 || terminator_state == 2) && buffer[buffer_index] == '\r')
                    terminator_state += 1;
                else if ((terminator_state == 1 || terminator_state == 3) && buffer[buffer_index] == '\n')
                {
                    terminator_state += 1;
                    if (terminator_state == 4)
                    {
                        header_complete = true;
                        break;
                    }
                }
                else if (buffer[buffer_index] == '\r')
                    terminator_state = 1;
                else
                    terminator_state = 0;
                buffer_index += 1;
            }
        }
        total_sent = 0;
        while (total_sent < response_length)
        {
            bytes_sent = nw_send(client_fd, response + total_sent,
                                 response_length - total_sent, 0);
            if (bytes_sent <= 0)
                break;
            total_sent += static_cast<size_t>(bytes_sent);
        }
        nw_close(client_fd);
        remaining_connections -= 1;
    }
    return ;
}

[[maybe_unused]] static void api_request_stream_large_response_server(void)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    size_t body_size;
    char header_buffer[128];
    char *body_buffer;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        api_request_stream_large_server_signal_ready(config_error);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54358;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        api_request_stream_large_server_signal_ready(initialize_error);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        api_request_stream_large_server_signal_ready(
            FT_ERR_INVALID_OPERATION);
        return ;
    }
    api_request_stream_large_server_signal_ready(FT_ERR_SUCCESS);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr*>(&address_storage),
            &address_length);
    if (client_fd < 0)
    {
        g_api_request_stream_large_server_start_error.store(FT_ERR_SOCKET_ACCEPT_FAILED, std::memory_order_relaxed);
        return ;
    }
    char drain_buffer[512];
    const char *header_terminator;
    size_t header_match_index;
    bool header_complete;

    header_terminator = "\r\n\r\n";
    header_match_index = 0;
    header_complete = false;
    while (header_complete == false)
    {
        ssize_t bytes_received;
        size_t byte_index;

        bytes_received = nw_recv(client_fd, drain_buffer, sizeof(drain_buffer), 0);
        if (bytes_received <= 0)
            break ;
        byte_index = 0;
        while (byte_index < static_cast<size_t>(bytes_received)
            && header_complete == false)
        {
            if (drain_buffer[byte_index] == header_terminator[header_match_index])
            {
                header_match_index += 1;
                if (header_match_index == 4)
                    header_complete = true;
            }
            else if (drain_buffer[byte_index] == header_terminator[0])
            {
                header_match_index = 1;
            }
            else
            {
                header_match_index = 0;
            }
            byte_index += 1;
        }
        if (header_complete == true)
            break ;
    }
    body_size = 2 * 1024 * 1024;
    body_buffer = static_cast<char*>(cma_malloc(body_size));
    if (!body_buffer)
    {
        nw_close(client_fd);
        return ;
    }
    ft_memset(body_buffer, 'A', body_size);
    pf_snprintf(header_buffer, sizeof(header_buffer),
        "HTTP/1.1 200 OK\r\nContent-Length: %llu\r\n\r\n",
        static_cast<unsigned long long>(body_size));
    nw_send(client_fd, header_buffer, ft_strlen(header_buffer), 0);
    size_t total_sent;

    total_sent = 0;
    while (total_sent < body_size)
    {
        size_t remaining;
        size_t chunk_size;
        ssize_t bytes_sent;

        remaining = body_size - total_sent;
        chunk_size = 32768;
        if (chunk_size > remaining)
            chunk_size = remaining;
        bytes_sent = nw_send(client_fd, body_buffer + total_sent,
                chunk_size, 0);
        if (bytes_sent <= 0)
            break ;
        total_sent += static_cast<size_t>(bytes_sent);
    }
    cma_free(body_buffer);
    nw_close(client_fd);
    return ;
}

[[maybe_unused]] static void api_request_retry_success_server(void)
{
    SocketConfig server_configuration;
        struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    int accepted_count;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point current_time;
    std::chrono::milliseconds elapsed;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        g_api_request_retry_success_server_start_error.store(config_error,
            std::memory_order_relaxed);
        g_api_request_retry_success_server_ready.store(true,
            std::memory_order_release);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54339;
    server_configuration._non_blocking = true;
    ft_socket server_socket;
    int initialize_error;

    initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        g_api_request_retry_success_server_start_error.store(
            initialize_error, std::memory_order_relaxed);
        g_api_request_retry_success_server_ready.store(true,
            std::memory_order_release);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        g_api_request_retry_success_server_start_error.store(
            FT_ERR_INVALID_OPERATION, std::memory_order_relaxed);
        g_api_request_retry_success_server_ready.store(true,
            std::memory_order_release);
        return ;
    }
    g_api_request_retry_success_server_start_error.store(FT_ERR_SUCCESS,
        std::memory_order_relaxed);
    g_api_request_retry_success_server_ready.store(true,
        std::memory_order_release);
    accepted_count = 0;
    start_time = std::chrono::steady_clock::now();
    while (accepted_count < 2)
    {
        current_time = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time);
        if (elapsed.count() > 2000)
            break ;
        address_length = sizeof(address_storage);
        client_fd = nw_accept(server_socket.get_file_descriptor(),
                reinterpret_cast<struct sockaddr*>(&address_storage),
                &address_length);
        if (client_fd < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue ;
        }
        accepted_count++;
        if (accepted_count == 1)
        {
            nw_close(client_fd);
            continue ;
        }
        const char *response;
        size_t response_length;
        size_t total_sent;

        response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
        response_length = ft_strlen(response);
        total_sent = 0;
        while (total_sent < response_length)
        {
            ssize_t bytes_sent;

            bytes_sent = nw_send(client_fd, response + total_sent,
                    response_length - total_sent, 0);
            if (bytes_sent <= 0)
                break ;
            total_sent += static_cast<size_t>(bytes_sent);
        }
        nw_close(client_fd);
    }
    if (accepted_count == 2)
    return ;
}

static void api_request_retry_failure_server(void)
{
    SocketConfig server_configuration;
        struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    int accepted_count;
    std::chrono::steady_clock::time_point start_time;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
        return ;
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54340;
    ft_socket server_socket;
    if (server_socket.initialize(server_configuration) != FT_ERR_SUCCESS)
        return ;
    if (server_socket.get_file_descriptor() < 0)
        return ;
    accepted_count = 0;
    start_time = std::chrono::steady_clock::now();
    while (accepted_count < 2)
    {
        std::chrono::steady_clock::time_point current_time;
        std::chrono::milliseconds elapsed_time;

        current_time = std::chrono::steady_clock::now();
        elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time);
        if (elapsed_time.count() > 2000)
            break ;
        address_length = sizeof(address_storage);
        client_fd = nw_accept(server_socket.get_file_descriptor(),
                reinterpret_cast<struct sockaddr*>(&address_storage),
                &address_length);
        if (client_fd < 0)
            continue ;
        accepted_count++;
        nw_close(client_fd);
    }
    return ;
}

static void api_request_retry_exhaustion_watchdog(std::atomic<bool> *completed,
    std::atomic<bool> *timed_out)
{
    std::chrono::steady_clock::time_point start_time;

    if (!completed || !timed_out)
        return ;
    start_time = std::chrono::steady_clock::now();
    while (!completed->load(std::memory_order_acquire))
    {
        std::chrono::steady_clock::time_point current_time;
        std::chrono::milliseconds elapsed_time;

        current_time = std::chrono::steady_clock::now();
        elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time);
        if (elapsed_time.count() >= 30000)
        {
            timed_out->store(true, std::memory_order_release);
            completed->store(true, std::memory_order_release);
            return ;
        }
        time_sleep_ms(10);
    }
    return ;
}

static void api_request_circuit_success_server(
    api_request_circuit_server_context *context)
{
    SocketConfig server_configuration;
        struct sockaddr_storage address_storage;
    socklen_t address_length;
    int served_count;

    if (!context)
        return ;
    int config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(config_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = context->port;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        context->start_error.store(initialize_error, std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        context->start_error.store(FT_ERR_INVALID_OPERATION,
            std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    address_length = sizeof(address_storage);
    if (getsockname(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr *>(&address_storage),
            &address_length) != 0)
    {
        context->start_error.store(cmp_map_system_error_to_ft(errno),
            std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    if (address_storage.ss_family == AF_INET)
    {
        const struct sockaddr_in *address_ipv4;

        address_ipv4 = reinterpret_cast<const struct sockaddr_in *>(
            &address_storage);
        context->port = ntohs(address_ipv4->sin_port);
    }
    else if (address_storage.ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *address_ipv6;

        address_ipv6 = reinterpret_cast<const struct sockaddr_in6 *>(
            &address_storage);
        context->port = ntohs(address_ipv6->sin6_port);
    }
    else
    {
        context->start_error.store(FT_ERR_INVALID_ARGUMENT,
            std::memory_order_relaxed);
        context->ready.store(true, std::memory_order_release);
        return ;
    }
    served_count = 0;
    long long start_time_ms;
    int client_fd;

    client_fd = -1;
    start_time_ms = time_monotonic();
    context->start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    context->ready.store(true, std::memory_order_release);
    while (served_count < context->responses)
    {
        long long elapsed_ms;

        elapsed_ms = time_monotonic() - start_time_ms;
        if (elapsed_ms > 20000)
            break ;
        if (client_fd < 0)
        {
            address_length = sizeof(address_storage);
            client_fd = nw_accept(server_socket.get_file_descriptor(),
                    reinterpret_cast<struct sockaddr*>(&address_storage),
                    &address_length);
            if (client_fd < 0)
            {
                time_sleep_ms(1);
                continue ;
            }
        }
        const char *response;
        size_t response_length;
        size_t total_sent;
        ft_string request_buffer;
        char request_chunk[1024];

        if (request_buffer.initialize() != FT_ERR_SUCCESS)
        {
            nw_close(client_fd);
            client_fd = -1;
            break ;
        }
        while (true)
        {
            ssize_t bytes_received;

            bytes_received = nw_recv(client_fd, request_chunk,
                    sizeof(request_chunk) - 1, 0);
            if (bytes_received <= 0)
            {
                break ;
            }
            request_buffer.append(request_chunk,
                static_cast<ft_size_t>(bytes_received));
            if (request_buffer.get_error() != FT_ERR_SUCCESS)
            {
                break ;
            }
            if (ft_strstr(request_buffer.c_str(), "\r\n\r\n"))
            {
                break ;
            }
            if (request_buffer.size() >= 8192)
            {
                break ;
            }
        }
        if (request_buffer.size() == 0)
        {
            nw_close(client_fd);
            client_fd = -1;
            continue ;
        }
        response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\nOK";
        response_length = ft_strlen(response);
        total_sent = 0;
        while (total_sent < response_length)
        {
            ssize_t bytes_sent;

            bytes_sent = nw_send(client_fd, response + total_sent,
                    response_length - total_sent, 0);
            if (bytes_sent <= 0)
            {
                break ;
            }
            total_sent += static_cast<size_t>(bytes_sent);
        }
        if (total_sent >= response_length)
        {
            time_sleep_ms(10);
        }
        nw_close(client_fd);
        client_fd = -1;
        served_count++;
    }
    if (client_fd >= 0)
        nw_close(client_fd);
    return ;
}

static void api_request_retry_timeout_server(void)
{
    SocketConfig server_configuration;
        struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    int accepted_count;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds accept_timeout;
    int poll_descriptors[1];
    int poll_result;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
        return ;
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54341;
    ft_socket server_socket;
    if (server_socket.initialize(server_configuration) != FT_ERR_SUCCESS)
        return ;
    if (server_socket.get_file_descriptor() < 0)
        return ;
    accepted_count = 0;
    start_time = std::chrono::steady_clock::now();
    accept_timeout = std::chrono::milliseconds(2000);
    poll_descriptors[0] = server_socket.get_file_descriptor();
    while (accepted_count < 3)
    {
        std::chrono::steady_clock::time_point current_time;
        std::chrono::milliseconds elapsed_time;
        int remaining_timeout;

        current_time = std::chrono::steady_clock::now();
        elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time);
        if (elapsed_time >= accept_timeout)
            break ;
        remaining_timeout = static_cast<int>(accept_timeout.count()
                - elapsed_time.count());
        if (remaining_timeout <= 0)
            break ;
        poll_result = nw_poll(poll_descriptors, 1, ft_nullptr, 0,
                remaining_timeout);
        if (poll_result <= 0)
            continue ;
        address_length = sizeof(address_storage);
        client_fd = nw_accept(server_socket.get_file_descriptor(),
                reinterpret_cast<struct sockaddr*>(&address_storage),
                &address_length);
        if (client_fd < 0)
            continue ;
        accepted_count++;
#ifdef _WIN32
        Sleep(200);
#else
        usleep(200000);
#endif
        nw_close(client_fd);
    }
    return ;
}

[[maybe_unused]] static void api_request_stream_chunked_response_server(void)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    const char *response;
    size_t response_length;
    size_t total_sent;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        api_request_stream_chunked_server_signal_ready(config_error);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54359;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        api_request_stream_chunked_server_signal_ready(initialize_error);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        api_request_stream_chunked_server_signal_ready(
            FT_ERR_INVALID_OPERATION);
        return ;
    }
    api_request_stream_chunked_server_signal_ready(FT_ERR_SUCCESS);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr*>(&address_storage),
            &address_length);
    if (client_fd < 0)
    {
        g_api_request_stream_chunked_server_start_error.store(FT_ERR_SOCKET_ACCEPT_FAILED, std::memory_order_relaxed);
        return ;
    }
    response = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
        "10\r\n0123456789ABCDEF\r\n"
        "08\r\nABCDEFGH\r\n"
        "0\r\n\r\n";
    response_length = ft_strlen(response);
    total_sent = 0;
    while (total_sent < response_length)
    {
        ssize_t bytes_sent;

        bytes_sent = nw_send(client_fd, response + total_sent,
                response_length - total_sent, 0);
        if (bytes_sent <= 0)
            break ;
        total_sent += static_cast<size_t>(bytes_sent);
    }
    nw_close(client_fd);
    return ;
}

[[maybe_unused]] static void api_request_async_retry_server_reset_state(void)
{
    g_api_async_retry_server_ready.store(false, std::memory_order_relaxed);
    g_api_async_retry_server_start_error.store(FT_ERR_SUCCESS,
        std::memory_order_relaxed);
    g_api_async_retry_server_bytes_received.store(0,
        std::memory_order_relaxed);
    g_api_async_retry_server_header_complete.store(false,
        std::memory_order_relaxed);
    g_api_async_retry_server_last_recv_result.store(0,
        std::memory_order_relaxed);
    g_api_async_retry_server_last_errno.store(0,
        std::memory_order_relaxed);
    return ;
}

[[maybe_unused]] static void api_request_async_retry_server_signal_ready(int error_code)
{
    g_api_async_retry_server_start_error.store(error_code,
        std::memory_order_relaxed);
    g_api_async_retry_server_ready.store(true, std::memory_order_release);
    return ;
}

[[maybe_unused]] static bool api_request_async_retry_server_wait_until_ready(void)
{
    size_t wait_iterations;

    wait_iterations = 0;
    while (!g_api_async_retry_server_ready.load(std::memory_order_acquire))
    {
        if (wait_iterations >= 50)
            return (false);
        api_request_small_delay();
        wait_iterations += 1;
    }
    if (g_api_async_retry_server_start_error.load(std::memory_order_acquire) != FT_ERR_SUCCESS)
        return (false);
    return (true);
}

struct api_async_retry_callback_data
{
    std::atomic<bool> *completed;
    std::atomic<int> *status;
    std::atomic<char*> *body;
};

[[maybe_unused]] static char *api_request_build_large_header(void)
{
    const char *header_prefix;
    size_t header_prefix_length;
    size_t filler_length;
    size_t total_length;
    char *header_buffer;

    header_prefix = "X-Test: ";
    header_prefix_length = ft_strlen(header_prefix);
    filler_length = 4 * 1024 * 1024;
    total_length = header_prefix_length + filler_length + 2;
    header_buffer = static_cast<char*>(cma_malloc(total_length + 1));
    if (!header_buffer)
        return (ft_nullptr);
    ft_memcpy(header_buffer, header_prefix, header_prefix_length);
    ft_memset(header_buffer + header_prefix_length, 'A', filler_length);
    header_buffer[header_prefix_length + filler_length] = '\r';
    header_buffer[header_prefix_length + filler_length + 1] = '\n';
    header_buffer[header_prefix_length + filler_length + 2] = '\0';
    return (header_buffer);
}

[[maybe_unused]] static void api_request_async_retry_callback(char *body, int status, void *user_data)
{
    api_async_retry_callback_data *data;

    data = static_cast<api_async_retry_callback_data*>(user_data);
    if (!data || !data->completed || !data->status || !data->body)
    {
        if (body)
            cma_free(body);
        return ;
    }
    data->status->store(status);
    data->body->store(body);
    data->completed->store(true);
    return ;
}

[[maybe_unused]] static void api_request_async_retry_server(void)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    const char *response;
    char buffer[4096];
    size_t total_bytes_received;
    bool header_complete;
    int last_recv_result;
    int last_recv_errno;
    int config_error;

    config_error = server_configuration.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        api_request_async_retry_server_signal_ready(config_error);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    server_configuration._port = 54339;
    ft_socket server_socket;
    int initialize_error = server_socket.initialize(server_configuration);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        api_request_async_retry_server_signal_ready(initialize_error);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        api_request_async_retry_server_signal_ready(FT_ERR_INVALID_OPERATION);
        return ;
    }
    api_request_async_retry_server_signal_ready(FT_ERR_SUCCESS);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(),
                          reinterpret_cast<struct sockaddr*>(&address_storage),
                          &address_length);
    if (client_fd < 0)
        return ;
    g_api_async_retry_server_bytes_received.store(0);
    g_api_async_retry_server_header_complete.store(false);
    g_api_async_retry_server_last_recv_result.store(0);
    g_api_async_retry_server_last_errno.store(0);
    api_request_small_delay();
    api_request_small_delay();
    total_bytes_received = 0;
    header_complete = false;
    last_recv_result = 0;
    last_recv_errno = 0;
    int terminator_state;

    terminator_state = 0;
    while (true)
    {
        ssize_t bytes_received;

        bytes_received = nw_recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            last_recv_result = static_cast<int>(bytes_received);
            last_recv_errno = errno;
            break;
        }
        total_bytes_received += static_cast<size_t>(bytes_received);
        buffer[bytes_received] = '\0';
        size_t buffer_index;

        buffer_index = 0;
        while (buffer_index < static_cast<size_t>(bytes_received))
        {
            char current_char;

            current_char = buffer[buffer_index];
            if ((terminator_state == 0 || terminator_state == 2) && current_char == '\r')
                terminator_state += 1;
            else if ((terminator_state == 1 || terminator_state == 3) && current_char == '\n')
            {
                terminator_state += 1;
                if (terminator_state == 4)
                {
                    header_complete = true;
                    break;
                }
            }
            else if (current_char == '\r')
                terminator_state = 1;
            else
                terminator_state = 0;
            buffer_index += 1;
        }
        if (header_complete)
            break;
    }
    g_api_async_retry_server_bytes_received.store(total_bytes_received);
    g_api_async_retry_server_header_complete.store(header_complete);
    g_api_async_retry_server_last_recv_result.store(last_recv_result);
    g_api_async_retry_server_last_errno.store(last_recv_errno);
    response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    nw_send(client_fd, response, ft_strlen(response), 0);
    nw_close(client_fd);
    return ;
}

FT_TEST(test_api_request_invalid_ip_sets_socket_error)
{
    char *result;

    result = api_request_string("bad-ip", 8080, "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, 10);
    if (result != ft_nullptr)
        return (0);
    return (1);
}

FT_TEST(test_api_request_connect_failure_sets_errno)
{
    char *result;

    result = api_request_string_host("127.0.0.1", 59999, "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, 50);
    if (result != ft_nullptr)
        return (0);
    return (1);
}

FT_TEST(test_api_request_send_failure_sets_errno)
{
    char *result;
    ft_thread server_thread;

    if (api_request_local_sockets_available() == FT_FALSE)
        return (1);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    api_request_send_failure_server_reset_state();
    server_thread = ft_thread(api_request_send_failure_server);
    FT_ASSERT(server_thread.joinable());
    api_request_small_delay();
    FT_ASSERT(api_request_send_failure_server_wait_until_ready());
    result = api_request_string("127.0.0.1",
            static_cast<uint16_t>(g_api_request_send_failure_server_port.load(
                    std::memory_order_acquire)),
            "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    server_thread.join();
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_api_request_async_alloc_failure_sets_errno)
{
    bool result;

    cma_set_alloc_limit(1);
    result = api_request_string_async("127.0.0.1", 8080, "GET", "/", api_request_noop_callback,
                                      ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    cma_set_alloc_limit(0);
    if (result)
        return (0);
    return (1);
}

FT_TEST(test_api_request_bad_input_sets_errno)
{
    char *result;

    result = api_request_string_host(ft_nullptr, 8080, "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, 10);
    if (result != ft_nullptr)
        return (0);
    return (1);
}

FT_TEST(test_api_request_success_resets_errno)
{
    char *body;
    int32_t status_code;
    api_transport_hooks hooks;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_string = api_request_test_string_success_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    status_code = 0;
    body = api_request_string("127.0.0.1", 54338, "GET", "/", ft_nullptr,
        ft_nullptr, &status_code, 1000);
    api_clear_transport_hooks();
    FT_ASSERT(body != ft_nullptr);
    FT_ASSERT_EQ(200, status_code);
    FT_ASSERT(ft_strlen(body) == 0);
    cma_free(body);
    return (1);
}

#if NETWORKING_HAS_OPENSSL
FT_TEST(test_api_request_host_bearer_adds_header)
{
    ft_thread server_thread;
    api_request_bearer_server_context context;
    char *body;
    int status_code;

    if (api_request_local_sockets_available() == FT_FALSE)
        return (1);
    api_retry_circuit_reset();
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    context.ready.store(false, std::memory_order_relaxed);
    context.start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    context.header_received.store(false, std::memory_order_relaxed);
    context.result.store(-99, std::memory_order_relaxed);
    context.client_fd = -1;
    if (context.request_data.initialize() != FT_ERR_SUCCESS)
        return (0);
    server_thread = ft_thread(api_request_bearer_server, &context);
    FT_ASSERT(server_thread.joinable());
    FT_ASSERT(server_thread.joinable());
    while (!context.ready.load(std::memory_order_acquire))
        api_request_small_delay();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        context.start_error.load(std::memory_order_acquire));
    status_code = 0;
    body = api_request_string_host_bearer("127.0.0.1", 54365, "GET", "/",
                                          "test-token", ft_nullptr, ft_nullptr,
                                          &status_code, 1000, ft_nullptr);
    server_thread.join();
    FT_ASSERT(body != ft_nullptr);
    cma_free(body);
    FT_ASSERT(context.result.load(std::memory_order_relaxed) == 0);
    FT_ASSERT(context.header_received.load(std::memory_order_relaxed));
    FT_ASSERT(status_code == 200);
    return (1);
}

FT_TEST(test_api_request_host_basic_appends_after_existing_header)
{
    ft_thread server_thread;
    api_request_basic_server_context context;
    char *body;
    int status_code;

    if (api_request_local_sockets_available() == FT_FALSE)
        return (1);
    api_retry_circuit_reset();
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    context.ready.store(false, std::memory_order_relaxed);
    context.start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    context.basic_header_received.store(false, std::memory_order_relaxed);
    context.basic_header_after_custom.store(false, std::memory_order_relaxed);
    context.result.store(-99, std::memory_order_relaxed);
    context.client_fd = -1;
    if (context.request_data.initialize() != FT_ERR_SUCCESS)
        return (0);
    server_thread = ft_thread(api_request_basic_server, &context);
    FT_ASSERT(server_thread.joinable());
    if (server_thread.joinable() == false)
        return (0);
    while (!context.ready.load(std::memory_order_acquire))
        api_request_small_delay();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        context.start_error.load(std::memory_order_acquire));
    status_code = 0;
    body = api_request_string_host_basic("127.0.0.1", 54366, "GET", "/",
                                         "dXNlcjpwYXNz", ft_nullptr,
                                         "X-Test-Header: Value",
                                         &status_code, 1000, ft_nullptr);
    server_thread.join();
    FT_ASSERT(body != ft_nullptr);
    cma_free(body);
    FT_ASSERT(context.result.load(std::memory_order_relaxed) == 0);
    FT_ASSERT(context.basic_header_received.load(std::memory_order_relaxed));
    FT_ASSERT(context.basic_header_after_custom.load(
        std::memory_order_relaxed));
    if (status_code != 200)
        return (0);
    return (1);
}
#else
FT_TEST(test_api_request_host_bearer_adds_header)
{
    return (1);
}

FT_TEST(test_api_request_host_basic_appends_after_existing_header)
{
    return (1);
}
#endif

FT_TEST(test_api_request_stream_large_response)
{
    api_stream_test_context context;
    api_streaming_handler handler;
    api_transport_hooks hooks;
    bool result;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_stream = api_request_test_stream_large_response_hook;
    hooks.user_data = &context;
    api_set_transport_hooks(&hooks);
    context.total_bytes = 0;
    context.chunk_count = 0;
    context.headers_received = false;
    context.final_received = false;
    context.status_code = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.reset();
    handler.set_headers_callback(api_request_stream_headers_callback);
    handler.set_body_callback(api_request_stream_body_callback);
    handler.set_user_data(&context);
    result = api_request_stream("127.0.0.1", 54358, "GET", "/", &handler,
            ft_nullptr, ft_nullptr, 5000, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT(result);
    FT_ASSERT(context.headers_received);
    FT_ASSERT_EQ(200, context.status_code);
    FT_ASSERT(context.final_received);
    FT_ASSERT_EQ(static_cast<size_t>(2 * 1024 * 1024), context.total_bytes);
    FT_ASSERT(context.chunk_count != 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.destroy());
    return (1);
}

FT_TEST(test_api_request_stream_chunked_response)
{
    api_stream_test_context context;
    api_streaming_handler handler;
    api_transport_hooks hooks;
    bool result;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_stream = api_request_test_stream_chunked_response_hook;
    hooks.user_data = &context;
    api_set_transport_hooks(&hooks);
    context.total_bytes = 0;
    context.chunk_count = 0;
    context.headers_received = false;
    context.final_received = false;
    context.status_code = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.initialize());
    handler.reset();
    handler.set_headers_callback(api_request_stream_headers_callback);
    handler.set_body_callback(api_request_stream_body_callback);
    handler.set_user_data(&context);
    result = api_request_stream("127.0.0.1", 54359, "GET", "/", &handler,
            ft_nullptr, ft_nullptr, 2000, ft_nullptr);
    api_clear_transport_hooks();
    FT_ASSERT(result);
    FT_ASSERT(context.headers_received);
    FT_ASSERT_EQ(200, context.status_code);
    FT_ASSERT(context.final_received);
    FT_ASSERT_EQ(static_cast<size_t>(24), context.total_bytes);
    FT_ASSERT(context.chunk_count >= 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handler.destroy());
    return (1);
}

FT_TEST(test_api_request_async_large_send_retries_do_not_timeout)
{
    std::atomic<bool> callback_completed(false);
    std::atomic<int> callback_status(0);
    std::atomic<char *> callback_body(ft_nullptr);
    api_request_async_test_callback_data callback_data;
    bool async_result;
    api_transport_hooks hooks;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_string_async = api_request_test_async_success_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    api_retry_circuit_reset();
    callback_data.completed = &callback_completed;
    callback_data.status = &callback_status;
    callback_data.body = &callback_body;
    async_result = api_request_string_async("127.0.0.1", 54339, "POST", "/",
        api_request_test_async_callback, &callback_data, ft_nullptr,
        ft_nullptr, 1000);
    api_clear_transport_hooks();
    if (!async_result)
        return (0);
    FT_ASSERT(callback_completed.load(std::memory_order_acquire));
    FT_ASSERT_EQ(200, callback_status.load(std::memory_order_acquire));
    FT_ASSERT_EQ(ft_strcmp(callback_body.load(std::memory_order_acquire),
        "OK"), 0);
    cma_free(callback_body.load(std::memory_order_acquire));
    return (1);
}

FT_TEST(test_api_request_string_url_invalid_sets_errno)
{
    char *result;

    result = api_request_string_url("example.com/path", "GET", ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    if (result != ft_nullptr)
        return (0);
    return (1);
}

FT_TEST(test_api_request_string_url_success)
{
    char *body;
    api_transport_hooks hooks;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_string_host = api_request_test_string_success_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    body = api_request_string_url("http://127.0.0.1:54338/", "GET",
        ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    api_clear_transport_hooks();
    FT_ASSERT(body != ft_nullptr);
    FT_ASSERT(ft_strlen(body) == 0);
    cma_free(body);
    return (1);
}

FT_TEST(test_api_request_tls_missing_host_sets_errno)
{
#if NETWORKING_HAS_OPENSSL
    char *result;

    result = api_request_string_tls(ft_nullptr, 443, "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    if (result != ft_nullptr)
        return (0);
#endif
    return (1);
}

FT_TEST(test_api_request_async_missing_callback_sets_errno)
{
    bool result;

    result = api_request_string_async("127.0.0.1", 8080, "GET", "/", ft_nullptr, ft_nullptr, ft_nullptr, ft_nullptr, 1000);
    if (result)
        return (0);
    return (1);
}

FT_TEST(test_api_request_async_success_resets_errno)
{
    bool result;

    result = api_request_string_async("127.0.0.1", 59999, "GET", "/", api_request_noop_callback,
                                      ft_nullptr, ft_nullptr, ft_nullptr, 100);
    if (!result)
        return (0);
    return (1);
}

FT_TEST(test_api_request_formats_large_content_length)
{
    ft_string request;
    size_t payload_size;
    bool append_result;
    char expected_buffer[64];
    int expected_length;
    ft_string expected_header;
    const char *header_pointer;

    payload_size = static_cast<unsigned long long>(INT_MAX) + 42ULL;
    time_sleep_ms(6000);
    if (request.initialize("POST /resource HTTP/1.1") != FT_ERR_SUCCESS)
        return (0);
    append_result = api_append_content_length_header(request, payload_size);
    if (!append_result)
        return (0);
    expected_length = pf_snprintf(expected_buffer, sizeof(expected_buffer), "%llu",
        static_cast<unsigned long long>(payload_size));
    if (expected_length < 0)
        return (0);
    if (static_cast<size_t>(expected_length) >= sizeof(expected_buffer))
        return (0);
    if (expected_header.initialize("\r\nContent-Length: ") != FT_ERR_SUCCESS)
        return (0);
    expected_header += expected_buffer;
    header_pointer = ft_strstr(request.c_str(), expected_header.c_str());
    if (header_pointer == ft_nullptr)
        return (0);
    return (1);
}

#if NETWORKING_HAS_OPENSSL
struct api_http2_frame_roundtrip_result
{
    int32_t status;
};

static int32_t api_http2_frame_roundtrip_body(void)
{
    http2_frame input_frame;
    ft_string encoded;
    http2_frame decoded_frame;
    size_t offset;
    int error_code;
    uint8_t decoded_type;
    uint8_t input_type;
    uint8_t decoded_flags;
    uint8_t input_flags;
    uint32_t decoded_stream_identifier;
    uint32_t input_stream_identifier;
    ft_string decoded_payload;
    ft_string input_payload;

    if (encoded.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (input_frame.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (!input_frame.set_type(0x1))
        return (0);
    if (!input_frame.set_flags(0x5))
        return (0);
    if (!input_frame.set_stream_identifier(3))
        return (0);
    if (!input_frame.set_payload_from_buffer("Hello", 5))
        return (0);
    if (input_frame.get_error() != FT_ERR_SUCCESS)
        return (0);
    error_code = FT_ERR_SUCCESS;
    if (!http2_encode_frame(input_frame, encoded, error_code))
        return (0);
    if (error_code != FT_ERR_SUCCESS)
        return (0);
    if (encoded.get_error() != FT_ERR_SUCCESS)
        return (0);
    offset = 0;
    if (decoded_frame.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (!http2_decode_frame(reinterpret_cast<const unsigned char*>(encoded.c_str()),
            encoded.size(), offset, decoded_frame, error_code))
        return (0);
    if (error_code != FT_ERR_SUCCESS)
        return (0);
    if (!decoded_frame.get_type(decoded_type))
        return (0);
    if (!input_frame.get_type(input_type))
        return (0);
    if (decoded_type != input_type)
        return (0);
    if (!decoded_frame.get_flags(decoded_flags))
        return (0);
    if (!input_frame.get_flags(input_flags))
        return (0);
    if (decoded_flags != input_flags)
        return (0);
    if (!decoded_frame.get_stream_identifier(decoded_stream_identifier))
        return (0);
    if (!input_frame.get_stream_identifier(input_stream_identifier))
        return (0);
    if (decoded_stream_identifier != input_stream_identifier)
        return (0);
    if (!decoded_frame.copy_payload(decoded_payload))
        return (0);
    if (!input_frame.copy_payload(input_payload))
        return (0);
    if (decoded_payload.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (input_payload.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (!(decoded_payload == input_payload))
        return (0);
    return (1);
}

static void *api_http2_frame_roundtrip_worker(void *argument)
{
    api_http2_frame_roundtrip_result *result;

    result = static_cast<api_http2_frame_roundtrip_result *>(argument);
    result->status = api_http2_frame_roundtrip_body();
    return (ft_nullptr);
}

FT_TEST(test_http2_frame_roundtrip)
{
    pthread_t worker_thread;
    api_http2_frame_roundtrip_result worker_result;
    int32_t thread_create_result;
    int32_t thread_join_result;

    worker_result.status = 0;
    thread_create_result = pt_thread_create(&worker_thread, ft_nullptr,
            api_http2_frame_roundtrip_worker, &worker_result);
    if (thread_create_result != 0)
        return (0);
    thread_join_result = pt_thread_timed_join(worker_thread, ft_nullptr, 5000);
    if (thread_join_result != 0)
        (void)pt_thread_detach(worker_thread);
    if (thread_join_result != 0)
        return (0);
    if (worker_result.status != 1)
        return (0);
    return (1);
}

FT_TEST(test_http2_header_compression_roundtrip)
{
    ft_vector<http2_header_field> headers;
    ft_vector<http2_header_field> decoded_headers;
    http2_header_field field_entry;
    ft_string compressed;
    int error_code;
    size_t header_count;
    size_t index;

    if (headers.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (decoded_headers.initialize() != FT_ERR_SUCCESS)
        return (0);

    auto append_header = [&](const char *name, const char *value) -> int
    {
        if (field_entry.initialize() != FT_ERR_SUCCESS)
            return (0);
        if (!field_entry.assign_from_cstr(name, value))
            return (0);
        headers.push_back(field_entry);
        if (headers.get_error() != FT_ERR_SUCCESS)
            return (0);
        if (field_entry.destroy() != FT_ERR_SUCCESS)
            return (0);
        return (1);
    };

    if (!append_header(":method", "GET"))
        return (0);
    if (!append_header(":path", "/resource"))
        return (0);
    if (!append_header("user-agent", "libft-tests"))
        return (0);
    if (!append_header("accept", "*/*"))
        return (0);
    if (compressed.initialize() != FT_ERR_SUCCESS)
        return (0);
    error_code = FT_ERR_SUCCESS;
    if (!http2_compress_headers(headers, compressed, error_code))
        return (0);
    if (error_code != FT_ERR_SUCCESS)
        return (0);
    if (compressed.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (!http2_decompress_headers(compressed, decoded_headers, error_code))
        return (0);
    if (error_code != FT_ERR_SUCCESS)
        return (0);
    header_count = decoded_headers.size();
    if (decoded_headers.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (headers.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (header_count != headers.size())
        return (0);
    index = 0;
    while (index < header_count)
    {
        ft_string decoded_name;
        ft_string decoded_value;
        ft_string original_name;
        ft_string original_value;

        if (decoded_name.initialize() != FT_ERR_SUCCESS)
            return (0);
        if (decoded_value.initialize() != FT_ERR_SUCCESS)
            return (0);
        if (original_name.initialize() != FT_ERR_SUCCESS)
            return (0);
        if (original_value.initialize() != FT_ERR_SUCCESS)
            return (0);
        if (!decoded_headers[index].copy_name(decoded_name))
            return (0);
        if (!headers[index].copy_name(original_name))
            return (0);
        if (decoded_name.get_error() != FT_ERR_SUCCESS)
            return (0);
        if (original_name.get_error() != FT_ERR_SUCCESS)
            return (0);
        if (!(decoded_name == original_name))
            return (0);
        if (!decoded_headers[index].copy_value(decoded_value))
            return (0);
        if (!headers[index].copy_value(original_value))
            return (0);
        if (decoded_value.get_error() != FT_ERR_SUCCESS)
            return (0);
        if (original_value.get_error() != FT_ERR_SUCCESS)
            return (0);
        if (!(decoded_value == original_value))
            return (0);
        index++;
    }
    return (1);
}

FT_TEST(test_http2_stream_manager_concurrent_streams)
{
    http2_stream_manager manager;
    ft_string buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT(manager.open_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.open_stream(3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.append_data(1, "Ping", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.append_data(3, "Pong", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.get_stream_buffer(1, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_error());
    FT_ASSERT(buffer == "Ping");
    FT_ASSERT(manager.get_stream_buffer(3, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_error());
    FT_ASSERT(buffer == "Pong");
    FT_ASSERT(manager.close_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.close_stream(3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    return (1);
}

FT_TEST(test_http2_frame_error_queries_follow_lifecycle_contract)
{
    http2_frame frame;

    g_http2_frame_get_error_returned = FT_FALSE;
    g_http2_frame_get_error_result = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(1, api_request_expect_sigabrt(
        http2_frame_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_http2_frame_get_error_returned);
    g_http2_frame_get_error_str_returned = FT_FALSE;
    g_http2_frame_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, api_request_expect_sigabrt(
        http2_frame_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_http2_frame_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    FT_ASSERT(frame.set_type(0x1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(frame.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}

FT_TEST(test_http2_stream_manager_error_queries_follow_lifecycle_contract)
{
    http2_stream_manager manager;

    g_http2_stream_manager_get_error_returned = FT_FALSE;
    g_http2_stream_manager_get_error_result = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(1, api_request_expect_sigabrt(
        http2_stream_manager_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_http2_stream_manager_get_error_returned);
    g_http2_stream_manager_get_error_str_returned = FT_FALSE;
    g_http2_stream_manager_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, api_request_expect_sigabrt(
        http2_stream_manager_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_http2_stream_manager_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT(manager.open_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(!manager.open_stream(1));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.destroy());
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, manager.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(manager.get_error_str(),
        ft_strerror(FT_ERR_ALREADY_EXISTS)));
    return (1);
}

FT_TEST(test_http2_stream_manager_flow_control)
{
    http2_stream_manager manager;
    ft_string buffer;
    uint32_t window_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT(manager.update_local_initial_window(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.update_remote_initial_window(12));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.open_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(!manager.append_data(1, "0123456789", 10));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, manager.get_error());
    FT_ASSERT(manager.append_data(1, "ABCD", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    window_value = manager.get_local_window(1);
    FT_ASSERT_EQ(static_cast<uint32_t>(4), window_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(!manager.reserve_send_window(1, 16));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, manager.get_error());
    FT_ASSERT(manager.reserve_send_window(1, 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    window_value = manager.get_remote_window(1);
    FT_ASSERT_EQ(static_cast<uint32_t>(6), window_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.append_data(1, "EFGH", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.get_stream_buffer(1, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_error());
    FT_ASSERT(buffer == "ABCDEFGH");
    FT_ASSERT(manager.close_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    return (1);
}

FT_TEST(test_http2_stream_manager_priority_reassignment)
{
    http2_stream_manager manager;
    uint32_t dependency_identifier;
    uint8_t weight_value;
    ft_bool exclusive_flag;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT(manager.open_stream(1));
    FT_ASSERT(manager.open_stream(3));
    FT_ASSERT(manager.open_stream(5));
    FT_ASSERT(manager.update_priority(3, 0, 20, false));
    FT_ASSERT(manager.update_priority(5, 0, 10, false));
    FT_ASSERT(manager.update_priority(3, 0, 25, true));
    FT_ASSERT(manager.get_priority(5, dependency_identifier, weight_value, exclusive_flag));
    FT_ASSERT_EQ(static_cast<uint32_t>(3), dependency_identifier);
    FT_ASSERT(manager.get_priority(3, dependency_identifier, weight_value, exclusive_flag));
    FT_ASSERT_EQ(static_cast<uint32_t>(0), dependency_identifier);
    FT_ASSERT(manager.close_stream(5));
    FT_ASSERT(manager.close_stream(3));
    FT_ASSERT(manager.close_stream(1));
    return (1);
}

FT_TEST(test_http2_frame_copy_move_preserve_state)
{
    http2_frame source;
    http2_frame moved;
    ft_string copied_payload;
    ft_string moved_payload;
    uint8_t type_value;
    uint8_t flags_value;
    uint32_t stream_identifier;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_payload.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT(source.set_type(0x5));
    FT_ASSERT(source.set_flags(0x3));
    FT_ASSERT(source.set_stream_identifier(11));
    FT_ASSERT(source.set_payload_from_buffer("abc", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.set_error(FT_ERR_ALREADY_EXISTS));
    {
        http2_frame copied;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
        FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied._initialised_state);
        FT_ASSERT_EQ(FT_TRUE, copied.is_thread_safe());
        FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, copied.get_error());
        FT_ASSERT(copied.get_type(type_value));
        FT_ASSERT_EQ(static_cast<uint8_t>(0x5), type_value);
        FT_ASSERT(copied.get_flags(flags_value));
        FT_ASSERT_EQ(static_cast<uint8_t>(0x3), flags_value);
        FT_ASSERT(copied.get_stream_identifier(stream_identifier));
        FT_ASSERT_EQ(static_cast<uint32_t>(11), stream_identifier);
        FT_ASSERT(copied.copy_payload(copied_payload));
        FT_ASSERT(copied_payload == "abc");
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(static_cast<http2_frame &&>(source)));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, moved.is_thread_safe());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, moved.get_error());
    FT_ASSERT(moved.copy_payload(moved_payload));
    FT_ASSERT(moved_payload == "abc");
    return (1);
}

FT_TEST(test_http2_frame_destroyed_source_propagates_copy_state)
{
    http2_frame source;
    http2_frame copied;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, source.set_error(FT_ERR_INVALID_OPERATION));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied._initialised_state);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, copied.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(copied.get_error_str(),
        ft_strerror(FT_ERR_INVALID_OPERATION)));
    return (1);
}

FT_TEST(test_http2_stream_manager_copy_preserve_stream_state)
{
    http2_stream_manager source;
    http2_stream_manager copied;
    ft_string buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT(source.open_stream(3));
    FT_ASSERT(source.append_data(3, "Ping", 4));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.set_error(FT_ERR_ALREADY_EXISTS));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, copied.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, copied.get_error());
    FT_ASSERT(copied.get_stream_buffer(3, buffer));
    FT_ASSERT(buffer == "Ping");
    return (1);
}

FT_TEST(test_http2_stream_manager_move_preserve_stream_state)
{
    http2_stream_manager source;
    http2_stream_manager moved;
    ft_string buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT(source.open_stream(3));
    FT_ASSERT(source.append_data(3, "Ping", 4));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, source.set_error(FT_ERR_ALREADY_EXISTS));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(
        static_cast<http2_stream_manager &&>(source)));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, moved.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, moved.get_error());
    FT_ASSERT(moved.get_stream_buffer(3, buffer));
    FT_ASSERT(buffer == "Ping");
    return (1);
}

FT_TEST(test_http2_stream_manager_destroyed_source_propagates_copy_state)
{
    http2_stream_manager source;
    http2_stream_manager copied;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.set_error(FT_ERR_NOT_FOUND));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied._initialised_state);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, copied.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(copied.get_error_str(),
        ft_strerror(FT_ERR_NOT_FOUND)));
    return (1);
}

FT_TEST(test_http2_settings_apply_remote_settings)
{
    http2_stream_manager manager;
    http2_settings_state settings;
    http2_frame frame;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.initialize());
    char payload_bytes[6];
    uint32_t window_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, settings.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(manager.open_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(frame.set_type(0x4));
    FT_ASSERT(frame.set_flags(0x0));
    FT_ASSERT(frame.set_stream_identifier(0));
    payload_bytes[0] = 0x00;
    payload_bytes[1] = 0x04;
    payload_bytes[2] = 0x00;
    payload_bytes[3] = 0x00;
    payload_bytes[4] = 0x04;
    payload_bytes[5] = 0x00;
    FT_ASSERT(frame.set_payload_from_buffer(payload_bytes, 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, frame.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT(settings.apply_remote_settings(frame, manager));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    window_value = manager.get_remote_window(1);
    FT_ASSERT_EQ(static_cast<uint32_t>(1024), window_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    FT_ASSERT_EQ(static_cast<uint32_t>(1024),
            settings.get_initial_remote_window());
    FT_ASSERT(manager.close_stream(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, manager.get_error());
    return (1);
}
#endif

FT_TEST(test_api_request_http2_plain_fallback)
{
    char *body;
    ft_bool used_http2;
    api_transport_hooks hooks;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_string_http2 = api_request_test_string_http2_fallback_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    used_http2 = true;
    body = api_request_string_http2("127.0.0.1", 54338, "GET", "/", ft_nullptr,
            ft_nullptr, ft_nullptr, 1000, &used_http2);
    api_clear_transport_hooks();
    FT_ASSERT(body != ft_nullptr);
    FT_ASSERT(!used_http2);
    FT_ASSERT(ft_strcmp(body, "Hello") == 0);
    cma_free(body);
    return (1);
}

FT_TEST(test_api_request_retry_policy_success)
{
    api_retry_policy retry_policy;
    char *body;
    int status_value;
    api_transport_hooks hooks;

    ft_memset(&hooks, 0, sizeof(hooks));
    hooks.request_string = api_request_test_string_success_hook;
    hooks.user_data = ft_nullptr;
    api_set_transport_hooks(&hooks);
    api_retry_circuit_reset();
    retry_policy.set_max_attempts(3);
    retry_policy.set_initial_delay_ms(10);
    retry_policy.set_max_delay_ms(0);
    retry_policy.set_backoff_multiplier(1);
    status_value = -123;
    body = api_request_string("127.0.0.1", 54339, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 100, &retry_policy);
    api_clear_transport_hooks();
    FT_ASSERT(body != ft_nullptr);
    FT_ASSERT(ft_strlen(body) == 0);
    FT_ASSERT_EQ(200, status_value);
    cma_free(body);
    return (1);
}

FT_TEST(test_api_request_retry_policy_exhaustion)
{
    ft_thread server_thread;
    ft_thread watchdog_thread;
    api_retry_policy retry_policy;
    std::atomic<bool> test_completed;
    std::atomic<bool> test_timed_out;
    char *body;
    int status_value;
    int result;
    const char *failure_expression;

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    test_completed.store(false, std::memory_order_release);
    test_timed_out.store(false, std::memory_order_release);
    watchdog_thread = ft_thread(api_request_retry_exhaustion_watchdog,
            &test_completed, &test_timed_out);
    FT_ASSERT(watchdog_thread.joinable());
    body = ft_nullptr;
    result = 0;
    failure_expression = "test_api_request_retry_policy_exhaustion completed";
    server_thread = ft_thread(api_request_retry_failure_server);
    if (server_thread.joinable() == false)
    {
        failure_expression = "server_thread.joinable() == true";
        goto cleanup;
    }
    api_request_small_delay();
    retry_policy.set_max_attempts(2);
    retry_policy.set_initial_delay_ms(5);
    retry_policy.set_max_delay_ms(0);
    retry_policy.set_backoff_multiplier(1);
    status_value = 777;
    body = api_request_string("127.0.0.1", 54340, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 100, &retry_policy);
    if (test_timed_out.load(std::memory_order_acquire))
    {
        failure_expression = "test_timed_out.load(std::memory_order_acquire) == false";
        goto cleanup;
    }
    if (body)
    {
        failure_expression = "body == ft_nullptr";
        goto cleanup;
    }
    if (status_value != 777)
    {
        failure_expression = "status_value == 777";
        goto cleanup;
    }
    result = 1;

cleanup:
    if (server_thread.joinable())
        server_thread.join();
    if (body)
    {
        cma_free(body);
        body = ft_nullptr;
    }
    test_completed.store(true, std::memory_order_release);
    if (watchdog_thread.joinable())
        watchdog_thread.join();
    if (result == 0)
        ft_test_fail(failure_expression, __FILE__, __LINE__);
    return (result);
}

FT_TEST(test_api_request_retry_policy_timeout)
{
    ft_thread server_thread;
    api_retry_policy retry_policy;
    char *body;
    int status_value;
    int request_errno;

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    server_thread = ft_thread(api_request_retry_timeout_server);
    FT_ASSERT(server_thread.joinable());
    api_request_small_delay();
    retry_policy.set_max_attempts(3);
    retry_policy.set_initial_delay_ms(5);
    retry_policy.set_max_delay_ms(10);
    retry_policy.set_backoff_multiplier(2);
    status_value = -45;
    request_errno = FT_ERR_SUCCESS;
    body = api_request_string("127.0.0.1", 54341, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request_errno);
    server_thread.join();
    FT_ASSERT(body == ft_nullptr);
    FT_ASSERT_EQ(-45, status_value);
    FT_ASSERT(body == ft_nullptr);
    return (1);
}

FT_TEST(test_api_request_circuit_breaker_blocks_after_threshold)
{
    api_retry_policy retry_policy;
    char *body;
    int status_value;
    api_request_circuit_server_context server_context;
    ft_thread server_thread;
    int wait_attempts;

    if (api_request_local_sockets_available() == FT_FALSE)
        return (1);
    api_retry_circuit_reset();
    retry_policy.set_max_attempts(1);
    retry_policy.set_initial_delay_ms(0);
    retry_policy.set_max_delay_ms(0);
    retry_policy.set_backoff_multiplier(1);
    retry_policy.set_circuit_breaker_threshold(2);
    retry_policy.set_circuit_breaker_cooldown_ms(200);
    retry_policy.set_circuit_breaker_half_open_successes(1);
    status_value = 0;
    body = api_request_string("127.0.0.1", 55310, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    FT_ASSERT_EQ(ft_nullptr, body);
    status_value = 0;
    body = api_request_string("127.0.0.1", 55310, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    FT_ASSERT_EQ(ft_nullptr, body);
    status_value = 0;
    body = api_request_string("127.0.0.1", 55310, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    FT_ASSERT_EQ(ft_nullptr, body);
    api_retry_circuit_reset();
    server_context.ready.store(false, std::memory_order_relaxed);
    server_context.start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    server_context.port = 0;
    server_context.responses = 1;
    server_thread = ft_thread(api_request_circuit_success_server,
            &server_context);
    FT_ASSERT_EQ(true, server_thread.joinable());
    wait_attempts = 0;
    while (!server_context.ready.load(std::memory_order_acquire)
        && wait_attempts < 100)
    {
        time_sleep_ms(10);
        wait_attempts++;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        server_context.start_error.load(std::memory_order_acquire));
    status_value = 0;
    body = api_request_string("127.0.0.1", server_context.port, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 200, &retry_policy);
    server_thread.join();
    FT_ASSERT_NEQ(ft_nullptr, body);
    FT_ASSERT_EQ(0, ft_strcmp(body, "OK"));
    cma_free(body);
    FT_ASSERT_EQ(200, status_value);
    return (1);
}

FT_TEST(test_api_request_circuit_breaker_half_open_recovers)
{
    api_retry_policy retry_policy;
    char *body;
    int status_value;
    int request_errno;
    api_request_circuit_server_context server_context;
    ft_thread server_thread;
    int wait_attempts;

    if (api_request_local_sockets_available() == FT_FALSE)
        return (1);
    api_retry_circuit_reset();
    retry_policy.set_max_attempts(1);
    retry_policy.set_initial_delay_ms(0);
    retry_policy.set_max_delay_ms(0);
    retry_policy.set_backoff_multiplier(1);
    retry_policy.set_circuit_breaker_threshold(2);
    retry_policy.set_circuit_breaker_cooldown_ms(100);
    retry_policy.set_circuit_breaker_half_open_successes(1);
    status_value = 0;
    body = api_request_string("127.0.0.1", 55311, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    if (body)
    {
        cma_free(body);
        return (0);
    }
    status_value = 0;
    body = api_request_string("127.0.0.1", 55311, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 50, &retry_policy);
    if (body)
    {
        cma_free(body);
        return (0);
    }
    time_sleep_ms(150);
    server_context.ready.store(false, std::memory_order_relaxed);
    server_context.start_error.store(FT_ERR_SUCCESS, std::memory_order_relaxed);
    server_context.port = 0;
    server_context.responses = 2;
    server_thread = ft_thread(api_request_circuit_success_server,
            &server_context);
    if (server_thread.joinable() == false)
        return (0);
    wait_attempts = 0;
    while (!server_context.ready.load(std::memory_order_acquire)
        && wait_attempts < 100)
    {
        time_sleep_ms(10);
        wait_attempts++;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        server_context.start_error.load(std::memory_order_acquire));
    status_value = 0;
    body = api_request_string("127.0.0.1", server_context.port, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 200, &retry_policy);
    request_errno = FT_ERR_SUCCESS;
    if (!body)
    {
        server_thread.join();
        return (0);
    }
    if (ft_strcmp(body, "OK") != 0)
    {
        cma_free(body);
        server_thread.join();
        return (0);
    }
    cma_free(body);
    if (status_value != 200)
    {
        server_thread.join();
        return (0);
    }
    if (request_errno != FT_ERR_SUCCESS)
    {
        server_thread.join();
        return (0);
    }
    status_value = 0;
    body = api_request_string("127.0.0.1", server_context.port, "GET", "/", ft_nullptr,
            ft_nullptr, &status_value, 200, &retry_policy);
    request_errno = FT_ERR_SUCCESS;
    server_thread.join();
    if (!body)
        return (0);
    if (ft_strcmp(body, "OK") != 0)
    {
        cma_free(body);
        return (0);
    }
    cma_free(body);
    if (status_value != 200)
        return (0);
    if (request_errno != FT_ERR_SUCCESS)
        return (0);
    api_retry_circuit_reset();
    return (1);
}

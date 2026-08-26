#include "../test_internal.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Networking/http_server.hpp"
#include "../../Modules/Networking/socket_class.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Threading/thread.hpp"
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstddef>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Time/time.hpp"
#include <atomic>
#ifndef LIBFT_TEST_BUILD
#endif

#define FT_TEST_REQUIRE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            ft_test_fail(#condition, __FILE__, __LINE__); \
            goto cleanup; \
        } \
    } while (0)

struct http_server_context
{
    ft_http_server *server;
    int result;
};

struct http_server_watchdog_context
{
    ft_http_server *server;
    ft_socket *client_socket;
    std::atomic<bool> *stop_requested;
    std::atomic<bool> *timed_out;
};

static void http_server_run_once(http_server_context *context)
{
    if (context == ft_nullptr)
        return ;
    if (context->server == ft_nullptr)
        return ;
    context->result = context->server->run_once();
    return ;
}

static void http_server_watchdog(http_server_watchdog_context *context)
{
    t_monotonic_time_point start_time;
    t_monotonic_time_point current_time;

    if (context == ft_nullptr)
        return ;
    if (context->server == ft_nullptr || context->stop_requested == ft_nullptr
        || context->timed_out == ft_nullptr)
        return ;
    start_time = time_monotonic_point_now();
    while (context->stop_requested->load() == false)
    {
        current_time = time_monotonic_point_now();
        if (time_monotonic_point_diff_ms(start_time, current_time) >= 10000)
        {
            context->timed_out->store(true);
            if (context->client_socket != ft_nullptr)
                (void)context->client_socket->close_socket();
            (void)context->server->_server_socket.close_socket();
            return ;
        }
        time_sleep_ms(50);
    }
    return ;
}

static int collect_response(int socket_fd, ft_string &response)
{
    char buffer[256];
    ssize_t bytes_received;

    response.clear();
    while (1)
    {
        bytes_received = nw_recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
            break;
        buffer[bytes_received] = '\0';
        response.append(buffer);
    }
    if (response.empty())
        return (0);
    return (1);
}

static ft_bool get_bound_server_port(const ft_http_server &server, uint16_t &port_value)
{
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int32_t file_descriptor;

    file_descriptor = server._server_socket.get_file_descriptor();
    if (file_descriptor < 0)
        return (FT_FALSE);
    address_length = sizeof(address_storage);
    if (getsockname(file_descriptor, reinterpret_cast<struct sockaddr *>(&address_storage),
            &address_length) != 0)
        return (FT_FALSE);
    if (address_storage.ss_family == AF_INET)
    {
        const struct sockaddr_in *ipv4_address;

        ipv4_address = reinterpret_cast<const struct sockaddr_in *>(&address_storage);
        port_value = ntohs(ipv4_address->sin_port);
        return (FT_TRUE);
    }
    if (address_storage.ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *ipv6_address;

        ipv6_address = reinterpret_cast<const struct sockaddr_in6 *>(&address_storage);
        port_value = ntohs(ipv6_address->sin6_port);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static int32_t start_http_server_with_retry(ft_http_server &server)
{
    int32_t attempt_index;
    int32_t start_result;

    attempt_index = 0;
    while (attempt_index < 10)
    {
        start_result = server.start("127.0.0.1", 0);
        if (start_result == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        time_sleep_ms(20);
        attempt_index++;
    }
    return (FT_ERR_INVALID_OPERATION);
}

FT_TEST(test_networking_http_server_get_response)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_http_server server;
    http_server_context context;
    ft_thread server_thread;
    SocketConfig client_configuration;
    ft_socket client_socket;
    const char *request_string;
    ft_string response;
    uint16_t server_port;
    ft_bool thread_started;
    ft_bool client_initialized;
    ft_bool response_initialized;
    ft_bool test_passed;

    thread_started = FT_FALSE;
    client_initialized = FT_FALSE;
    response_initialized = FT_FALSE;
    test_passed = FT_FALSE;
    context.result = FT_ERR_INVALID_STATE;
    FT_TEST_REQUIRE(server.initialize() == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(start_http_server_with_retry(server) == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(get_bound_server_port(server, server_port) == FT_TRUE);
    context.server = &server;
    context.result = -1;
    server_thread = ft_thread(http_server_run_once, &context);
    FT_TEST_REQUIRE(server_thread.joinable());
    thread_started = FT_TRUE;
    FT_TEST_REQUIRE(response.initialize() == FT_ERR_SUCCESS);
    response_initialized = FT_TRUE;
    FT_TEST_REQUIRE(client_configuration.initialize() == FT_ERR_SUCCESS);
    client_configuration._type = SocketType::CLIENT;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1);
    client_configuration._ip[sizeof(client_configuration._ip) - 1] = '\0';
    client_configuration._port = server_port;
    FT_TEST_REQUIRE(client_socket.initialize(client_configuration) == FT_ERR_SUCCESS);
    client_initialized = FT_TRUE;
    request_string = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    FT_TEST_REQUIRE(client_socket.send_all(request_string, ft_strlen(request_string), 0)
        == static_cast<ssize_t>(ft_strlen(request_string)));
    FT_TEST_REQUIRE(collect_response(client_socket.get_file_descriptor(), response) == 1);
    if (thread_started == FT_TRUE)
    {
        server_thread.join();
        thread_started = FT_FALSE;
    }
    FT_TEST_REQUIRE(context.result == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(ft_strnstr(response.c_str(), "HTTP/1.1 200 OK", response.size()) != ft_nullptr);
    FT_TEST_REQUIRE(ft_strnstr(response.c_str(), "GET", response.size()) != ft_nullptr);
    test_passed = FT_TRUE;
cleanup:
    if (response_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, response.destroy());
    if (client_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.destroy());
    if (thread_started == FT_TRUE)
        server_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    if (test_passed == FT_FALSE)
        return (0);
    return (1);
}

FT_TEST(test_networking_http_server_short_write_sets_error)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_http_server server;
    http_server_context context;
    ft_thread server_thread;
    SocketConfig client_configuration;
    ft_socket client_socket;
    const char *request_string;
    int error_code;
    uint16_t server_port;
    ft_bool thread_started;
    ft_bool client_initialized;
    ft_bool test_passed;

    error_code = FT_ERR_INVALID_STATE;
    thread_started = FT_FALSE;
    client_initialized = FT_FALSE;
    test_passed = FT_FALSE;
    context.result = FT_ERR_INVALID_STATE;
    FT_TEST_REQUIRE(server.initialize() == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(start_http_server_with_retry(server) == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(get_bound_server_port(server, server_port) == FT_TRUE);
    context.server = &server;
    context.result = -1;
    server_thread = ft_thread(http_server_run_once, &context);
    FT_TEST_REQUIRE(server_thread.joinable());
    thread_started = FT_TRUE;
    FT_TEST_REQUIRE(client_configuration.initialize() == FT_ERR_SUCCESS);
    client_configuration._type = SocketType::CLIENT;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1);
    client_configuration._ip[sizeof(client_configuration._ip) - 1] = '\0';
    client_configuration._port = server_port;
    FT_TEST_REQUIRE(client_socket.initialize(client_configuration) == FT_ERR_SUCCESS);
    client_initialized = FT_TRUE;
    request_string = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    client_socket.send_all(request_string, ft_strlen(request_string), 0);
    client_socket.close_socket();
    if (thread_started == FT_TRUE)
    {
        server_thread.join();
        thread_started = FT_FALSE;
    }
    FT_TEST_REQUIRE(context.result != FT_ERR_SUCCESS);
    error_code = context.result;
    FT_TEST_REQUIRE(error_code != FT_ERR_SUCCESS);
    test_passed = FT_TRUE;
cleanup:
    if (client_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.destroy());
    if (thread_started == FT_TRUE)
        server_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    if (test_passed == FT_FALSE)
        return (0);
    return (1);
}

FT_TEST(test_networking_http_server_post_echoes_body)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_http_server server;
    http_server_context context;
    ft_thread server_thread;
    SocketConfig client_configuration;
    ft_socket client_socket;
    const char *body_content;
    ft_string request_string;
    char content_length_buffer[32];
    ft_string response;
    const char *status_match;
    const char *body_start;
    size_t body_length;
    uint16_t server_port;
    ft_bool thread_started;
    ft_bool client_initialized;
    ft_bool request_initialized;
    ft_bool response_initialized;
    ft_bool test_passed;

    thread_started = FT_FALSE;
    client_initialized = FT_FALSE;
    request_initialized = FT_FALSE;
    response_initialized = FT_FALSE;
    test_passed = FT_FALSE;
    context.result = FT_ERR_INVALID_STATE;
    FT_TEST_REQUIRE(server.initialize() == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(start_http_server_with_retry(server) == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(get_bound_server_port(server, server_port) == FT_TRUE);
    context.server = &server;
    context.result = -1;
    server_thread = ft_thread(http_server_run_once, &context);
    FT_TEST_REQUIRE(server_thread.joinable());
    thread_started = FT_TRUE;
    FT_TEST_REQUIRE(request_string.initialize() == FT_ERR_SUCCESS);
    request_initialized = FT_TRUE;
    FT_TEST_REQUIRE(response.initialize() == FT_ERR_SUCCESS);
    response_initialized = FT_TRUE;
    FT_TEST_REQUIRE(client_configuration.initialize() == FT_ERR_SUCCESS);
    client_configuration._type = SocketType::CLIENT;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1);
    client_configuration._ip[sizeof(client_configuration._ip) - 1] = '\0';
    client_configuration._port = server_port;
    FT_TEST_REQUIRE(client_socket.initialize(client_configuration) == FT_ERR_SUCCESS);
    client_initialized = FT_TRUE;
    body_content = "payload=echo-body";
    body_length = static_cast<size_t>(ft_strlen(body_content));
    request_string = "POST /echo HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: ";
    std::snprintf(content_length_buffer, sizeof(content_length_buffer), "%zu", body_length);
    request_string.append(content_length_buffer);
    request_string.append("\r\n\r\n");
    request_string.append(body_content);
    FT_TEST_REQUIRE(client_socket.send_all(request_string.c_str(), request_string.size(), 0)
        == static_cast<ssize_t>(request_string.size()));
    FT_TEST_REQUIRE(collect_response(client_socket.get_file_descriptor(), response) == 1);
    if (thread_started == FT_TRUE)
    {
        server_thread.join();
        thread_started = FT_FALSE;
    }
    FT_TEST_REQUIRE(context.result == FT_ERR_SUCCESS);
    status_match = ft_strnstr(response.c_str(), "HTTP/1.1 200 OK", response.size());
    FT_TEST_REQUIRE(status_match != ft_nullptr);
    body_start = ft_strnstr(response.c_str(), "\r\n\r\n", response.size());
    FT_TEST_REQUIRE(body_start != ft_nullptr);
    body_start += 4;
    FT_TEST_REQUIRE(ft_strncmp(body_start, body_content, body_length) == 0);
    test_passed = FT_TRUE;
cleanup:
    if (response_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, response.destroy());
    if (request_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, request_string.destroy());
    if (client_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.destroy());
    if (thread_started == FT_TRUE)
        server_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    if (test_passed == FT_FALSE)
        return (0);
    return (1);
}

FT_TEST(test_networking_http_server_keep_alive_multiple_requests)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_http_server server;
    http_server_context context;
    ft_thread server_thread;
    SocketConfig client_configuration;
    ft_socket client_socket;
    ft_string request_string;
    ft_string response;
    const char *first_response;
    const char *second_response;
    uint16_t server_port;
    ft_bool thread_started;
    ft_bool client_initialized;
    ft_bool request_initialized;
    ft_bool response_initialized;
    ft_bool test_passed;

    thread_started = FT_FALSE;
    client_initialized = FT_FALSE;
    request_initialized = FT_FALSE;
    response_initialized = FT_FALSE;
    test_passed = FT_FALSE;
    context.result = FT_ERR_INVALID_STATE;
    FT_TEST_REQUIRE(server.initialize() == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(server.start("127.0.0.1", 0) == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(get_bound_server_port(server, server_port) == FT_TRUE);
    context.server = &server;
    context.result = -1;
    server_thread = ft_thread(http_server_run_once, &context);
    FT_TEST_REQUIRE(server_thread.joinable());
    thread_started = FT_TRUE;
    FT_TEST_REQUIRE(request_string.initialize() == FT_ERR_SUCCESS);
    request_initialized = FT_TRUE;
    FT_TEST_REQUIRE(response.initialize() == FT_ERR_SUCCESS);
    response_initialized = FT_TRUE;
    FT_TEST_REQUIRE(client_configuration.initialize() == FT_ERR_SUCCESS);
    client_configuration._type = SocketType::CLIENT;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1);
    client_configuration._ip[sizeof(client_configuration._ip) - 1] = '\0';
    client_configuration._port = server_port;
    FT_TEST_REQUIRE(client_socket.initialize(client_configuration) == FT_ERR_SUCCESS);
    client_initialized = FT_TRUE;
    request_string = "GET /first HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    request_string.append("GET /second HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    FT_TEST_REQUIRE(client_socket.send_all(request_string.c_str(), request_string.size(), 0)
        == static_cast<ssize_t>(request_string.size()));
    FT_TEST_REQUIRE(collect_response(client_socket.get_file_descriptor(), response) == 1);
    if (thread_started == FT_TRUE)
    {
        server_thread.join();
        thread_started = FT_FALSE;
    }
    FT_TEST_REQUIRE(context.result == FT_ERR_SUCCESS);
    first_response = ft_strnstr(response.c_str(), "Connection: keep-alive", response.size());
    FT_TEST_REQUIRE(first_response != ft_nullptr);
    second_response = ft_strnstr(response.c_str() + (first_response - response.c_str()) + 1,
        "Connection: close", response.size() - (first_response - response.c_str()) - 1);
    FT_TEST_REQUIRE(second_response != ft_nullptr);
    test_passed = FT_TRUE;
cleanup:
    if (response_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, response.destroy());
    if (request_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, request_string.destroy());
    if (client_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.destroy());
    if (thread_started == FT_TRUE)
        server_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    if (test_passed == FT_FALSE)
        return (0);
    return (1);
}

FT_TEST(test_networking_http_server_thread_safe_run_once)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    ft_http_server server;
    http_server_context run_context;
    http_server_context error_context;
    ft_thread server_thread;
    ft_thread error_thread;
    SocketConfig client_configuration;
    ft_socket client_socket;
    const char *request_string;
    ft_string response;
    ft_bool run_thread_started;
    ft_bool error_thread_started;
    ft_bool watchdog_started;
    ft_bool client_initialized;
    ft_bool response_initialized;
    ft_bool test_passed;
    std::atomic<bool> watchdog_stop_requested;
    std::atomic<bool> watchdog_timed_out;
    http_server_watchdog_context watchdog_context;
    ft_thread watchdog_thread;
    uint16_t server_port;

    run_thread_started = FT_FALSE;
    error_thread_started = FT_FALSE;
    watchdog_started = FT_FALSE;
    client_initialized = FT_FALSE;
    response_initialized = FT_FALSE;
    test_passed = FT_FALSE;
    watchdog_stop_requested.store(false);
    watchdog_timed_out.store(false);
    run_context.result = FT_ERR_INVALID_STATE;
    error_context.result = FT_ERR_INVALID_STATE;
    FT_TEST_REQUIRE(server.initialize() == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(start_http_server_with_retry(server) == FT_ERR_SUCCESS);
    FT_TEST_REQUIRE(get_bound_server_port(server, server_port) == FT_TRUE);
    run_context.server = &server;
    run_context.result = -1;
    error_context.server = &server;
    error_context.result = -1;
    server_thread = ft_thread(http_server_run_once, &run_context);
    FT_TEST_REQUIRE(server_thread.joinable());
    run_thread_started = FT_TRUE;
    usleep(50000);
    error_thread = ft_thread(http_server_run_once, &error_context);
    FT_TEST_REQUIRE(error_thread.joinable());
    error_thread_started = FT_TRUE;
    FT_TEST_REQUIRE(response.initialize() == FT_ERR_SUCCESS);
    response_initialized = FT_TRUE;
    FT_TEST_REQUIRE(client_configuration.initialize() == FT_ERR_SUCCESS);
    client_configuration._type = SocketType::CLIENT;
    std::strncpy(client_configuration._ip, "127.0.0.1",
        sizeof(client_configuration._ip) - 1);
    client_configuration._ip[sizeof(client_configuration._ip) - 1] = '\0';
    client_configuration._port = server_port;
    FT_TEST_REQUIRE(client_socket.initialize(client_configuration) == FT_ERR_SUCCESS);
    client_initialized = FT_TRUE;
    watchdog_context.server = &server;
    watchdog_context.client_socket = &client_socket;
    watchdog_context.stop_requested = &watchdog_stop_requested;
    watchdog_context.timed_out = &watchdog_timed_out;
    watchdog_thread = ft_thread(http_server_watchdog, &watchdog_context);
    FT_TEST_REQUIRE(watchdog_thread.joinable());
    watchdog_started = FT_TRUE;
    request_string = "GET /thread HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    FT_TEST_REQUIRE(client_socket.send_all(request_string, ft_strlen(request_string), 0)
        == static_cast<ssize_t>(ft_strlen(request_string)));
    FT_TEST_REQUIRE(collect_response(client_socket.get_file_descriptor(), response) == 1);
    if (run_thread_started == FT_TRUE)
    {
        server_thread.join();
        run_thread_started = FT_FALSE;
    }
    if (error_thread_started == FT_TRUE)
    {
        error_thread.join();
        error_thread_started = FT_FALSE;
    }
    watchdog_stop_requested.store(true);
    if (watchdog_started == FT_TRUE)
    {
        watchdog_thread.join();
        watchdog_started = FT_FALSE;
    }
    FT_TEST_REQUIRE(run_context.result == FT_ERR_SUCCESS
        || run_context.result == 1
        || error_context.result == FT_ERR_SUCCESS
        || error_context.result == 1);
    FT_TEST_REQUIRE(watchdog_timed_out.load() == false);
    FT_TEST_REQUIRE(ft_strnstr(response.c_str(), "HTTP/1.1 200 OK", response.size()) != ft_nullptr);
    test_passed = FT_TRUE;
cleanup:
    watchdog_stop_requested.store(true);
    if (watchdog_started == FT_TRUE)
        watchdog_thread.join();
    if (response_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, response.destroy());
    if (client_initialized == FT_TRUE)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.destroy());
    if (error_thread_started == FT_TRUE)
        error_thread.join();
    if (run_thread_started == FT_TRUE)
        server_thread.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    if (test_passed == FT_FALSE)
        return (0);
    return (1);
}

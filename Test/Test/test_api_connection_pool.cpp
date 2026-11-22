#include "../test_internal.hpp"
#include "../../Modules/API/api.hpp"
#include "../../Modules/API/api_internal.hpp"
#include "../../Modules/API/api_http_internal.hpp"
#include "../../Modules/Networking/socket_class.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Threading/thread.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <atomic>
#include <cerrno>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

static ft_bool api_pool_local_sockets_available(void)
{
    int32_t socket_fd;
    struct sockaddr_in bind_address;
    int bind_result;

    errno = 0;
    socket_fd = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        if (errno == EPERM || errno == EACCES)
            return (FT_FALSE);
        return (FT_TRUE);
    }
    ft_memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = 0;
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    errno = 0;
    bind_result = nw_bind(socket_fd,
            reinterpret_cast<struct sockaddr *>(&bind_address),
            sizeof(bind_address));
    nw_close(socket_fd);
    if (bind_result < 0 && (errno == EPERM || errno == EACCES))
        return (FT_FALSE);
    return (FT_TRUE);
}

struct api_pool_test_server_context
{
    std::atomic<bool> ready;
    std::atomic<uint16_t> port;
    std::atomic<int> accept_count;
    std::atomic<int> handled_requests;
    std::atomic<int> result;
};

static void api_pool_test_sleep_small(void)
{
#ifdef _WIN32
    Sleep(100);
#else
    usleep(100000);
#endif
    return ;
}

static void api_pool_test_server(api_pool_test_server_context *context)
{
    SocketConfig server_configuration;
    struct sockaddr_storage address_storage;
    socklen_t address_length;
    int client_fd;
    int response_count;
    int32_t configuration_error;
    sockaddr_storage local_address;
    socklen_t local_address_length;

    if (!context)
        return ;
    context->result.store(0);
    context->accept_count.store(0);
    context->handled_requests.store(0);
    configuration_error = server_configuration.initialize();
    if (configuration_error != FT_ERR_SUCCESS)
    {
        context->result.store(configuration_error);
        context->ready.store(true);
        return ;
    }
    server_configuration._type = SocketType::SERVER;
    server_configuration._reuse_address = FT_TRUE;
    ft_strlcpy(server_configuration._ip, "127.0.0.1",
            sizeof(server_configuration._ip));
    server_configuration._port = 0;
    ft_socket server_socket;
    int32_t server_socket_error;

    server_socket_error = server_socket.initialize(server_configuration);
    if (server_socket_error != FT_ERR_SUCCESS)
    {
        context->result.store(server_socket_error);
        context->ready.store(true);
        return ;
    }
    if (server_socket.get_file_descriptor() < 0)
    {
        context->result.store(FT_ERR_INVALID_OPERATION);
        context->ready.store(true);
        return ;
    }
    local_address_length = sizeof(local_address);
    if (getsockname(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr *>(&local_address),
            &local_address_length) != 0
        || local_address.ss_family != AF_INET)
    {
        context->result.store(FT_ERR_SOCKET_BIND_FAILED);
        context->ready.store(true);
        return ;
    }
    context->port.store(ntohs(reinterpret_cast<const struct sockaddr_in *>(
                &local_address)->sin_port));
    context->ready.store(true);
    address_length = sizeof(address_storage);
    client_fd = nw_accept(server_socket.get_file_descriptor(),
            reinterpret_cast<struct sockaddr*>(&address_storage),
            &address_length);
    if (client_fd < 0)
    {
        context->result.store(FT_ERR_IO);
        return ;
    }
    context->accept_count.store(1);
    response_count = 0;
    while (response_count < 2)
    {
        bool connection_active;
        char buffer[1024];
        ssize_t bytes_received;
        ft_string request_storage;
        bool header_complete;
        size_t terminator_match;
        int32_t request_storage_initialization_error;

        request_storage_initialization_error = request_storage.initialize();
        if (request_storage_initialization_error != FT_ERR_SUCCESS)
        {
            context->result.store(request_storage_initialization_error);
            break;
        }
        connection_active = true;
        header_complete = false;
        terminator_match = 0;
        while (connection_active && !header_complete)
        {
            bytes_received = nw_recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0)
            {
                connection_active = false;
                break;
            }
            size_t buffer_index;

            buffer_index = 0;
            while (buffer_index < static_cast<size_t>(bytes_received))
            {
                char current_char;

                current_char = buffer[buffer_index];
                request_storage.append(current_char);
                if (request_storage.get_error() != FT_ERR_SUCCESS)
                {
                    context->result.store(request_storage.get_error());
                    connection_active = false;
                    break;
                }
                if (current_char == '\r')
                {
                    if (terminator_match == 0 || terminator_match == 2)
                        terminator_match++;
                    else
                        terminator_match = 1;
                }
                else if (current_char == '\n')
                {
                    if (terminator_match == 1 || terminator_match == 3)
                        terminator_match++;
                    else
                        terminator_match = 0;
                }
                else
                    terminator_match = 0;
                if (terminator_match == 4)
                {
                    header_complete = true;
                    break;
                }
                buffer_index++;
            }
        }
        if (!connection_active || !header_complete)
            break;
        const char *response;
        size_t response_length;
        size_t total_sent;

        response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nHello";
        if (response_count == 1)
            response = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                "Connection: keep-alive\r\n\r\n2\r\nHe\r\n3\r\nllo\r\n0\r\nName: value\r\n\r\n";
        response_length = ft_strlen(response);
        total_sent = 0;
        while (connection_active && total_sent < response_length)
        {
            ssize_t bytes_sent;

            bytes_sent = nw_send(client_fd, response + total_sent,
                    response_length - total_sent, 0);
            if (bytes_sent <= 0)
            {
                connection_active = false;
                break;
            }
            total_sent += static_cast<size_t>(bytes_sent);
        }
        if (!connection_active)
            break;
        response_count++;
    }
    context->handled_requests.store(response_count);
    if (client_fd >= 0)
        nw_close(client_fd);
    return ;
}

FT_TEST(test_api_connection_pool_reuses_connections)
{
    api_pool_test_server_context context;

    if (api_pool_local_sockets_available() == FT_FALSE)
        return (1);
    context.ready.store(false);
    context.port.store(0);
    context.accept_count.store(0);
    context.handled_requests.store(0);
    context.result.store(0);
    ft_thread server_thread(api_pool_test_server, &context);

    FT_ASSERT(server_thread.joinable());
    int wait_attempts;

    wait_attempts = 0;
    while (!context.ready.load())
    {
        if (wait_attempts > 100)
        {
            server_thread.join();
            FT_ASSERT_EQ(true, false);
        }
        api_pool_test_sleep_small();
        wait_attempts++;
    }
    FT_ASSERT_EQ(0, context.result.load());
    FT_ASSERT_NEQ(static_cast<uint16_t>(0), context.port.load());
    uint16_t test_port;

    test_port = context.port.load();
    api_connection_pool_set_enabled(FT_TRUE);
    api_debug_reset_connection_pool_counters();
    api_connection_pool_flush();
    int first_status;
    char *first_body;

    first_status = 0;
    first_body = api_request_string("127.0.0.1", test_port,
            "GET", "/pool", ft_nullptr, ft_nullptr, &first_status, 2000);
    if (!first_body)
    {
        server_thread.join();
        FT_ASSERT_EQ(0, context.result.load());
    }
    FT_ASSERT_NEQ(ft_nullptr, first_body);
    FT_ASSERT_EQ(200, first_status);
    FT_ASSERT(ft_strncmp(first_body, "Hello", 5) == 0);
    cma_free(first_body);
    int second_status;
    char *second_body;

    second_status = 0;
    second_body = api_request_string("127.0.0.1", test_port,
            "GET", "/pool", ft_nullptr, ft_nullptr, &second_status, 2000);
    if (!second_body)
        server_thread.join();
    FT_ASSERT_NEQ(ft_nullptr, second_body);
    FT_ASSERT_EQ(200, second_status);
    FT_ASSERT(ft_strncmp(second_body, "Hello", 5) == 0);
    cma_free(second_body);
    api_connection_pool_flush();
    server_thread.join();
    FT_ASSERT_EQ(0, context.result.load());
    FT_ASSERT_EQ(static_cast<size_t>(2),
            api_debug_get_connection_pool_acquires());
    FT_ASSERT_EQ(static_cast<size_t>(1),
            api_debug_get_connection_pool_reuses());
    FT_ASSERT_EQ(static_cast<size_t>(1),
            api_debug_get_connection_pool_misses());
    FT_ASSERT_EQ(1, context.accept_count.load());
    FT_ASSERT_EQ(2, context.handled_requests.load());
    api_connection_pool_set_enabled(FT_FALSE);
    return (1);
}

FT_TEST(test_api_connection_pool_disable_store_resets_http2_flag)
{
    api_connection_pool_handle handle;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());

    handle.negotiated_http2 = true;
    api_connection_pool_disable_store(handle);
    FT_ASSERT_EQ(false, handle.negotiated_http2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.destroy());
    return (1);
}

FT_TEST(test_api_connection_pool_formats_port_and_checks_key_allocation)
{
    api_connection_pool_handle first;
    api_connection_pool_handle second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    FT_ASSERT_EQ(FT_FALSE, api_connection_pool_acquire(first, "::1", 65535U,
        api_connection_security_mode::TLS, "identity"));
    FT_ASSERT_EQ(FT_TRUE, first.key == "::1:65535:tls:identity");
    FT_ASSERT_EQ(FT_FALSE, first.should_store);

    constexpr char HOST[] = "allocation-test-host.example";
    cma_set_alloc_limit(1U);
    const ft_bool acquired = api_connection_pool_acquire(second, HOST, 80U,
        api_connection_security_mode::PLAIN, ft_nullptr);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_FALSE, acquired);
    FT_ASSERT_EQ(FT_FALSE, second.should_store);
    FT_ASSERT_EQ(FT_FALSE, api_connection_pool_acquire(second, HOST, 80U,
        api_connection_security_mode::PLAIN, ft_nullptr));
    FT_ASSERT_EQ(FT_TRUE, second.key == "allocation-test-host.example:80:plain:");
    return (1);
}

static int32_t api_pool_store_test_socket(const SocketConfig &configuration,
    uint16_t port, const char *identity)
{
    api_connection_pool_handle handle;
    if (handle.initialize() != FT_ERR_SUCCESS
        || api_connection_pool_acquire(handle, "127.0.0.1", port,
            api_connection_security_mode::PLAIN, identity)
        || !handle.should_store || handle.socket.initialize(configuration) != FT_ERR_SUCCESS)
        return (-1);
    const int32_t descriptor = handle.socket.get_file_descriptor();
    handle.has_socket = FT_TRUE;
    api_connection_pool_mark_idle(handle);
    if (handle.has_socket)
        return (-1);
    return (descriptor);
}

FT_TEST(test_api_connection_pool_unlinks_entries_in_idle_order)
{
    if (!api_pool_local_sockets_available())
        return (1);
    SocketConfig server_configuration;
    SocketConfig client_configuration;
    ft_socket listener;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_configuration.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_configuration.initialize());
    server_configuration._type = SocketType::SERVER;
    ft_strlcpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, listener.initialize(server_configuration));
    sockaddr_in address{};
    socklen_t address_length = sizeof(address);
    FT_ASSERT_EQ(0, getsockname(listener.get_file_descriptor(),
        reinterpret_cast<sockaddr *>(&address), &address_length));
    const uint16_t port = ntohs(address.sin_port);
    client_configuration._type = SocketType::CLIENT;
    client_configuration._port = port;
    client_configuration._recv_timeout = 1000;
    client_configuration._send_timeout = 1000;
    ft_strlcpy(client_configuration._ip, "127.0.0.1", sizeof(client_configuration._ip));

    const ft_bool previous_enabled = api_connection_pool_is_enabled();
    const ft_size_t previous_max_idle = api_connection_pool_get_max_idle();
    const int64_t previous_timeout = api_connection_pool_get_idle_timeout();
    api_connection_pool_flush();
    api_connection_pool_set_enabled(FT_TRUE);
    api_connection_pool_set_max_idle(3U);
    api_connection_pool_set_idle_timeout(0);
    const int32_t first = api_pool_store_test_socket(client_configuration, port, "old");
    const int32_t middle = api_pool_store_test_socket(client_configuration, port, "middle");
    const int32_t newest = api_pool_store_test_socket(client_configuration, port, "new");
    api_connection_pool_handle match;
    const int32_t initialized = match.initialize();
    const ft_bool middle_found = initialized == FT_ERR_SUCCESS
        && api_connection_pool_acquire(match, "127.0.0.1", port,
            api_connection_security_mode::PLAIN, "middle");
    int32_t recovered = -1;
    if (middle_found)
    {
        recovered = match.socket.get_file_descriptor();
        api_connection_pool_evict(match);
    }
    api_connection_pool_set_max_idle(1U);
    const int32_t retained = api_pool_store_test_socket(client_configuration, port, "retained");
    ft_bool old_found = FT_FALSE;
    ft_bool newest_found = FT_FALSE;
    ft_bool unrelated_found = FT_FALSE;
    int32_t expired_descriptor_status = 0;
    if (initialized == FT_ERR_SUCCESS)
    {
        old_found = api_connection_pool_acquire(match, "127.0.0.1", port,
            api_connection_security_mode::PLAIN, "old");
        if (old_found)
            api_connection_pool_evict(match);
        newest_found = api_connection_pool_acquire(match, "127.0.0.1", port,
            api_connection_security_mode::PLAIN, "new");
        if (newest_found)
            api_connection_pool_evict(match);
        api_connection_pool_set_idle_timeout(1);
        time_sleep_ms(25);
        unrelated_found = api_connection_pool_acquire(match, "127.0.0.1", port,
            api_connection_security_mode::PLAIN, "unrelated");
        int32_t socket_error = 0;
        socklen_t error_length = sizeof(socket_error);
        expired_descriptor_status = getsockopt(retained, SOL_SOCKET, SO_ERROR,
            reinterpret_cast<char *>(&socket_error), &error_length);
    }
    api_connection_pool_flush();
    api_connection_pool_set_enabled(previous_enabled);
    api_connection_pool_set_max_idle(previous_max_idle);
    api_connection_pool_set_idle_timeout(previous_timeout);

    FT_ASSERT(first >= 0 && middle >= 0 && newest >= 0 && retained >= 0);
    FT_ASSERT_EQ(FT_TRUE, middle_found);
    FT_ASSERT_EQ(middle, recovered);
    FT_ASSERT_EQ(FT_FALSE, old_found);
    FT_ASSERT_EQ(FT_FALSE, newest_found);
    FT_ASSERT_EQ(FT_FALSE, unrelated_found);
    FT_ASSERT_NEQ(0, expired_descriptor_status);
    return (1);
}

FT_TEST(test_api_connection_pool_handle_key_assignment_keeps_instance_error_clean)
{
    api_connection_pool_handle handle;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    handle.key = "127.0.0.1:8080:http";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.key.get_error());
    FT_ASSERT(handle.key == "127.0.0.1:8080:http");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.destroy());
    return (1);
}

FT_TEST(test_api_pool_handle_copy_failure_keeps_source_and_allows_retry)
{
    api_connection_pool_handle source;
    api_connection_pool_handle copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.key.assign("pool-key", 8U));
    source.should_store = FT_TRUE;
    source.security_mode = api_connection_security_mode::TLS;
    cma_set_alloc_limit(1U);
    const int32_t copy_error = copy.initialize(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copy._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, source.key == "pool-key");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_TRUE, copy.key == "pool-key");
    FT_ASSERT_EQ(FT_TRUE, copy.should_store);
    FT_ASSERT_EQ(FT_FALSE, copy.has_socket);
    FT_ASSERT_EQ(FT_FALSE, copy.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize());
    FT_ASSERT_EQ(FT_TRUE, copy.key.empty());
    return (1);
}

FT_TEST(test_api_pool_handle_move_transfers_live_socket_without_allocation)
{
    if (!api_pool_local_sockets_available())
        return (1);
    SocketConfig configuration;
    ft_socket listener;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.initialize());
    configuration._type = SocketType::SERVER;
    ft_strlcpy(configuration._ip, "127.0.0.1", sizeof(configuration._ip));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, listener.initialize(configuration));
    sockaddr_in address{};
    socklen_t address_length = sizeof(address);
    FT_ASSERT_EQ(0, getsockname(listener.get_file_descriptor(),
        reinterpret_cast<sockaddr *>(&address), &address_length));
    api_connection_pool_handle source;
    api_connection_pool_handle destination;
    api_connection_pool_handle copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    int32_t error_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_TRUE, api_http_prepare_plain_socket(source, "127.0.0.1",
        ntohs(address.sin_port), 1000, error_code));
    const int32_t descriptor = source.socket.get_file_descriptor();
    FT_ASSERT(descriptor >= 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_FALSE, copy.has_socket);
    FT_ASSERT_EQ(FT_TRUE, copy.key == source.key);

    cma_set_alloc_limit(1U);
    const uint32_t moved = destination.move(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(0U, moved);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_FALSE, source.has_socket);
    FT_ASSERT_EQ(FT_TRUE, destination.has_socket);
    FT_ASSERT_EQ(descriptor, destination.socket.get_file_descriptor());
    FT_ASSERT_EQ(FT_TRUE, destination.key == copy.key);
    FT_ASSERT_EQ(0U, destination.move(destination));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    int32_t socket_error = 0;
    socklen_t error_length = sizeof(socket_error);
    FT_ASSERT_NEQ(0, getsockopt(descriptor, SOL_SOCKET, SO_ERROR,
        reinterpret_cast<char *>(&socket_error), &error_length));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_FALSE, destination.has_socket);
    return (1);
}

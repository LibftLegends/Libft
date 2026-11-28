#include "../test_internal.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Networking/socket_handle.hpp"
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct mock_network_send_state
{
    std::string transmitted_data;
    size_t      chunk_size;
    int         call_count;
    int         failure_call_index;
    bool        failure_returns_zero;
};

static mock_network_send_state g_mock_network_send_state = {"", 0, 0, 0, false};

static s_network_sink *g_sink_to_remove = ft_nullptr;
static std::atomic<uint32_t> g_scheduled_health_probes{0U};

static ssize_t logger_count_scheduled_probe(int32_t, const void *,
    ft_size_t length, int32_t)
{
    g_scheduled_health_probes.fetch_add(1U, std::memory_order_release);
    return (static_cast<ssize_t>(length));
}

static ft_bool logger_wait_for_scheduled_probe(uint32_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (g_scheduled_health_probes.load(std::memory_order_acquire) < count)
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return (FT_FALSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return (FT_TRUE);
}

FT_TEST(test_logger_remote_health_wakes_when_interval_changes)
{
    ft_log_enable_remote_health(FT_FALSE);
    g_scheduled_health_probes.store(0U, std::memory_order_relaxed);
    s_network_sink sink;
    sink.socket_fd = 1001;
    sink.send_function = logger_count_scheduled_probe;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.host.initialize("local-health-test"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(ft_network_sink, &sink));
    ft_log_set_remote_health_interval(3600U);
    ft_log_enable_remote_health(FT_TRUE);
    const ft_bool initial_probe = logger_wait_for_scheduled_probe(1U);
    ft_log_set_remote_health_interval(1U);
    const ft_bool updated_probe = logger_wait_for_scheduled_probe(2U);
    ft_log_enable_remote_health(FT_FALSE);
    ft_log_remove_sink(ft_network_sink, &sink);
    ft_log_set_remote_health_interval(30U);
    FT_ASSERT_EQ(FT_TRUE, initial_probe);
    FT_ASSERT_EQ(FT_TRUE, updated_probe);
    return (1);
}

FT_TEST(test_logger_remote_health_serializes_concurrent_worker_controls)
{
    ft_log_enable_remote_health(FT_FALSE);
    const auto toggle = []()
    {
        uint32_t index = 0U;
        while (index < 8U)
        {
            ft_log_enable_remote_health(FT_TRUE);
            ft_log_enable_remote_health(FT_FALSE);
            index += 1U;
        }
        return ;
    };
    std::thread first(toggle);
    std::thread second(toggle);
    first.join();
    second.join();
    ft_log_enable_remote_health(FT_FALSE);
    return (1);
}

static ssize_t mock_partial_send(int socket_fd, const void *buffer,
    ft_size_t length, int flags)
{
    (void)socket_fd;
    (void)flags;

    g_mock_network_send_state.call_count++;
    if (g_mock_network_send_state.failure_call_index > 0
        && g_mock_network_send_state.call_count == g_mock_network_send_state.failure_call_index)
    {
        if (g_mock_network_send_state.failure_returns_zero)
            return (0);
        return (-1);
    }
    size_t allowed_chunk;

    allowed_chunk = g_mock_network_send_state.chunk_size;
    if (allowed_chunk == 0 || allowed_chunk > length)
        allowed_chunk = length;
    g_mock_network_send_state.transmitted_data.append(static_cast<const char *>(buffer), allowed_chunk);
    return (static_cast<ssize_t>(allowed_chunk));
}

static void reset_mock_network_send_state(void)
{
    std::string empty_string;

    g_mock_network_send_state.transmitted_data.clear();
    g_mock_network_send_state.transmitted_data.swap(empty_string);
    g_mock_network_send_state.chunk_size = 0;
    g_mock_network_send_state.call_count = 0;
    g_mock_network_send_state.failure_call_index = 0;
    g_mock_network_send_state.failure_returns_zero = false;
    g_sink_to_remove = ft_nullptr;
    return ;
}

FT_TEST(test_logger_network_sink_prepare_thread_safety_initializes_mutex)
{
    s_network_sink sink;
    ft_bool        lock_acquired;

    sink.socket_fd = 0;
    sink.send_function = ft_nullptr;
    FT_ASSERT_EQ(0, network_sink_prepare_thread_safety(&sink));
    const pt_mutex *first_mutex = sink.mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, network_sink_prepare_thread_safety(&sink));
    FT_ASSERT(sink.mutex == first_mutex);
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, network_sink_lock(&sink, &lock_acquired));
    FT_ASSERT(lock_acquired == FT_TRUE);
    network_sink_unlock(&sink, lock_acquired);
    network_sink_teardown_thread_safety(&sink);
    network_sink_teardown_thread_safety(&sink);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, network_sink_lock(&sink, &lock_acquired));
    FT_ASSERT_EQ(FT_FALSE, lock_acquired);
    return (1);
}

FT_TEST(test_logger_network_sink_lock_blocks_until_release)
{
    s_network_sink            sink;
    ft_bool                   main_lock_acquired;
    std::atomic<bool>         ready;
    std::atomic<bool>         worker_failed;
    std::atomic<long long>    wait_duration_ms;
    std::thread               worker;

    sink.socket_fd = 10;
    sink.send_function = ft_nullptr;
    FT_ASSERT_EQ(0, network_sink_prepare_thread_safety(&sink));
    main_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, network_sink_lock(&sink, &main_lock_acquired));
    FT_ASSERT(main_lock_acquired == FT_TRUE);
    ready.store(false);
    worker_failed.store(false);
    wait_duration_ms.store(0);
    worker = std::thread([&sink, &ready, &worker_failed, &wait_duration_ms]() {
        ft_bool                                   worker_lock_acquired;
        std::chrono::steady_clock::time_point     start_time;
        std::chrono::steady_clock::time_point     end_time;

        worker_lock_acquired = FT_FALSE;
        ready.store(true);
        start_time = std::chrono::steady_clock::now();
        if (network_sink_lock(&sink, &worker_lock_acquired) != 0)
        {
            worker_failed.store(true);
            wait_duration_ms.store(-1);
            return ;
        }
        end_time = std::chrono::steady_clock::now();
        wait_duration_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count());
        if (!worker_lock_acquired)
            worker_failed.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        network_sink_unlock(&sink, worker_lock_acquired);
    });
    while (!ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    network_sink_unlock(&sink, main_lock_acquired);
    worker.join();
    FT_ASSERT(worker_failed.load() == false);
    FT_ASSERT(wait_duration_ms.load() >= 40);
    network_sink_teardown_thread_safety(&sink);
    return (1);
}

static ssize_t mock_send_remove_sink(int socket_fd, const void *buffer,
    ft_size_t length, int flags)
{
    (void)socket_fd;
    (void)buffer;
    (void)length;
    (void)flags;

    if (g_sink_to_remove)
    {
        s_network_sink *sink_to_delete;

        sink_to_delete = g_sink_to_remove;
        g_sink_to_remove = ft_nullptr;
        ft_log_remove_sink(ft_network_sink, sink_to_delete);
        delete sink_to_delete;
    }
    return (-1);
}

static ssize_t mock_successful_send(int socket_fd, const void *buffer,
    ft_size_t length, int flags)
{
    (void)socket_fd;
    (void)buffer;
    (void)flags;

    return (static_cast<ssize_t>(length));
}

FT_TEST(test_logger_network_sink_handles_partial_writes)
{
    s_network_sink sink;
    const char *log_message;
    size_t expected_length;

    reset_mock_network_send_state();
    g_mock_network_send_state.chunk_size = 5;
    sink.socket_fd = 123;
    sink.send_function = mock_partial_send;
    log_message = "partial write message ensures loop execution";
    ft_network_sink(log_message, &sink);
    expected_length = ft_strlen(log_message);
    FT_ASSERT_EQ(123, sink.socket_fd);
    FT_ASSERT(g_mock_network_send_state.call_count > 1);
    FT_ASSERT_EQ(expected_length, g_mock_network_send_state.transmitted_data.size());
    FT_ASSERT(g_mock_network_send_state.transmitted_data == log_message);
    reset_mock_network_send_state();
    return (1);
}

FT_TEST(test_logger_network_sink_handles_send_failure)
{
    s_network_sink sink;
    const char *log_message;
    std::string expected_prefix;
    size_t prefix_length;

    reset_mock_network_send_state();
    g_mock_network_send_state.chunk_size = 6;
    g_mock_network_send_state.failure_call_index = 2;
    sink.socket_fd = 321;
    sink.send_function = mock_partial_send;
    log_message = "network sink failure path";
    ft_network_sink(log_message, &sink);
    prefix_length = g_mock_network_send_state.chunk_size;
    expected_prefix.assign(log_message, prefix_length);
    FT_ASSERT_EQ(-1, sink.socket_fd);
    FT_ASSERT(sink.send_function == ft_nullptr);
    FT_ASSERT_EQ(prefix_length, g_mock_network_send_state.transmitted_data.size());
    FT_ASSERT(g_mock_network_send_state.transmitted_data == expected_prefix);
    reset_mock_network_send_state();
    return (1);
}

FT_TEST(test_logger_network_sink_handles_zero_byte_send)
{
    s_network_sink sink;
    const char *log_message;
    size_t transmitted_length;
    size_t message_length;

    reset_mock_network_send_state();
    g_mock_network_send_state.chunk_size = 7;
    g_mock_network_send_state.failure_call_index = 2;
    g_mock_network_send_state.failure_returns_zero = true;
    sink.socket_fd = 555;
    sink.send_function = mock_partial_send;
    log_message = "network sink zero send failure";
    ft_network_sink(log_message, &sink);
    transmitted_length = g_mock_network_send_state.transmitted_data.size();
    message_length = ft_strlen(log_message);
    FT_ASSERT_EQ(-1, sink.socket_fd);
    FT_ASSERT(sink.send_function == ft_nullptr);
    FT_ASSERT(transmitted_length > 0);
    FT_ASSERT(transmitted_length < message_length);
    reset_mock_network_send_state();
    return (1);
}

static ssize_t mock_oversized_send(int32_t, const void *, ft_size_t length, int32_t)
{
    return (static_cast<ssize_t>(length + 1U));
}

FT_TEST(test_logger_network_sink_rejects_oversized_send_and_closes_socket)
{
    s_network_sink sink;
    const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(descriptor >= 0);
    sink.socket_fd = descriptor;
    sink.send_function = mock_oversized_send;
    const int32_t send_error = ft_network_sink("message", &sink);
    const int32_t descriptor_result = nw_set_nonblocking(descriptor);
    if (descriptor_result == 0)
        (void)nw_close(descriptor);
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, send_error);
    FT_ASSERT_EQ(-1, sink.socket_fd);
    FT_ASSERT_EQ(-1, descriptor_result);
    return (1);
}

FT_TEST(test_logger_remote_health_probe_records_success)
{
    s_network_sink *sink;
    s_log_remote_health status;
    ft_size_t status_count;

    reset_mock_network_send_state();
    g_mock_network_send_state.chunk_size = 8;
    sink = new s_network_sink();
    FT_ASSERT(sink != ft_nullptr);
    sink->socket_fd = 777;
    sink->send_function = mock_partial_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink->host.initialize("127.0.0.1"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink->host.get_error());
    sink->port = 5555;
    sink->use_tcp = true;
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, sink));
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    status_count = 0;
    FT_ASSERT_EQ(0, ft_log_get_remote_health(&status, 1, &status_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), status_count);
    FT_ASSERT(status.reachable);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status.last_error);
    FT_ASSERT(status.host != ft_nullptr);
    FT_ASSERT(ft_strcmp(status.host, "127.0.0.1") == 0);
    FT_ASSERT_EQ(5555, status.port);
    FT_ASSERT(status.use_tcp);
    FT_ASSERT(status.last_check != 0);
    ft_log_remove_sink(ft_network_sink, sink);
    delete sink;
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    status_count = 0;
    FT_ASSERT_EQ(0, ft_log_get_remote_health(ft_nullptr, 0, &status_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), status_count);
    return (1);
}

FT_TEST(test_logger_remote_health_restarts_for_reused_registration_address)
{
    s_network_sink sink;
    s_log_remote_health status{};
    ft_size_t status_count = 0U;

    ft_log_enable_remote_health(FT_FALSE);
    sink.socket_fd = 777;
    sink.send_function = mock_successful_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.host.initialize("first"));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, &sink));
    const int32_t probe_result = ft_log_probe_remote_health();
    const int32_t first_status_result = ft_log_get_remote_health(&status, 1, &status_count);
    const ft_bool first_reachable = status.reachable;
    const ft_size_t first_count = status_count;
    ft_log_remove_sink(ft_network_sink, &sink);

    FT_ASSERT_EQ(0, probe_result);
    FT_ASSERT_EQ(0, first_status_result);
    FT_ASSERT_EQ(1U, first_count);
    FT_ASSERT_EQ(FT_TRUE, first_reachable);
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, &sink));
    const int32_t replacement_status_result = ft_log_get_remote_health(&status, 1, &status_count);
    ft_log_remove_sink(ft_network_sink, &sink);
    FT_ASSERT_EQ(0, replacement_status_result);
    FT_ASSERT_EQ(1U, status_count);
    FT_ASSERT_EQ(FT_FALSE, status.reachable);
    FT_ASSERT_EQ(0, status.last_check);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, status.last_error);
    return (1);
}

FT_TEST(test_logger_remote_sink_delivers_native_udp_messages_and_closes)
{
    ft_socket_handle receiver;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize());
    int32_t receiver_descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(receiver_descriptor >= 0);
    FT_ASSERT(receiver.reset(receiver_descriptor));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr *socket_address = reinterpret_cast<sockaddr *>(&address);
    socklen_t address_size = sizeof(address);
    FT_ASSERT_EQ(0, nw_bind(receiver_descriptor, socket_address, address_size));
    FT_ASSERT_EQ(0, getsockname(receiver_descriptor, socket_address, &address_size));
    FT_ASSERT_EQ(0, nw_set_nonblocking(receiver_descriptor));

    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_INTERNAL, ft_log_set_remote_sink("invalid", ntohs(address.sin_port), FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_set_remote_sink("127.0.0.1", ntohs(address.sin_port), FT_FALSE));
    const auto *sink = static_cast<const s_network_sink *>(g_sinks[0].user_data);
    const int32_t sender_descriptor = sink->socket_fd;
    const int32_t probe_result = ft_log_probe_remote_health();
    ft_log_info("UDP loopback log message");
    ft_log_close();
    const int32_t descriptor_result = nw_set_nonblocking(sender_descriptor);
    if (descriptor_result == 0)
        (void)nw_close(sender_descriptor);
    FT_ASSERT_EQ(0, probe_result);
    FT_ASSERT_EQ(-1, descriptor_result);
    FT_ASSERT_EQ(1, nw_poll(&receiver_descriptor, 1, ft_nullptr, 0, 1000));
    constexpr char EXPECTED_PROBE[] = "[ft_logger] health probe\n";
    char received_probe[sizeof(EXPECTED_PROBE)]{};
    FT_ASSERT_EQ(sizeof(EXPECTED_PROBE) - 1U,
        static_cast<ft_size_t>(nw_recv(receiver_descriptor, received_probe, sizeof(received_probe) - 1U, 0)));
    FT_ASSERT_EQ(0, ft_strcmp(EXPECTED_PROBE, received_probe));
    FT_ASSERT_EQ(1, nw_poll(&receiver_descriptor, 1, ft_nullptr, 0, 1000));
    char received_message[256]{};
    FT_ASSERT(nw_recv(receiver_descriptor, received_message, sizeof(received_message) - 1U, 0) > 0);
    FT_ASSERT(ft_strstr(received_message, "UDP loopback log message") != ft_nullptr);
    return (1);
}

FT_TEST(test_logger_remote_health_probe_records_failure)
{
    s_network_sink *sink;
    s_log_remote_health status;
    ft_size_t status_count;

    reset_mock_network_send_state();
    g_mock_network_send_state.chunk_size = 6;
    g_mock_network_send_state.failure_call_index = 1;
    sink = new s_network_sink();
    FT_ASSERT(sink != ft_nullptr);
    sink->socket_fd = 888;
    sink->send_function = mock_partial_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink->host.initialize("10.0.0.5"));
    sink->port = 6000;
    sink->use_tcp = false;
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, sink));
    int probe_result;
    ft_logger logger;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger.initialize());
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, logger.probe_remote_health());
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, logger.get_error());
    reset_mock_network_send_state();
    g_mock_network_send_state.failure_call_index = 1;
    probe_result = ft_log_probe_remote_health();
    FT_ASSERT_EQ(-1, probe_result);
    status_count = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_get_remote_health(&status, 1, &status_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), status_count);
    FT_ASSERT(!status.reachable);
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, status.last_error);
    FT_ASSERT(status.last_check != 0);
    FT_ASSERT(status.host != ft_nullptr);
    FT_ASSERT(ft_strcmp(status.host, "10.0.0.5") == 0);
    FT_ASSERT_EQ(6000, status.port);
    FT_ASSERT(!status.use_tcp);
    ft_log_remove_sink(ft_network_sink, sink);
    delete sink;
    return (1);
}

FT_TEST(test_logger_remote_health_probe_handles_removed_sink)
{
    s_network_sink *sink;
    s_log_remote_health status;
    ft_size_t status_count;

    reset_mock_network_send_state();
    sink = new s_network_sink();
    FT_ASSERT(sink != ft_nullptr);
    sink->socket_fd = 4321;
    sink->send_function = mock_send_remove_sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink->host.initialize("removed.example.com"));
    sink->port = 9001;
    sink->use_tcp = true;
    g_sink_to_remove = sink;
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, sink));
    FT_ASSERT_EQ(-1, ft_log_probe_remote_health());
    status_count = 0;
    FT_ASSERT_EQ(0, ft_log_get_remote_health(&status, 1, &status_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), status_count);
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    return (1);
}

FT_TEST(test_logger_remote_health_probe_keeps_other_sinks_intact)
{
    s_network_sink *removed_sink;
    s_network_sink *healthy_sink;
    s_log_remote_health status;
    ft_size_t status_count;

    reset_mock_network_send_state();
    removed_sink = new s_network_sink();
    healthy_sink = new s_network_sink();
    FT_ASSERT(removed_sink != ft_nullptr);
    FT_ASSERT(healthy_sink != ft_nullptr);
    removed_sink->socket_fd = 1111;
    removed_sink->send_function = mock_send_remove_sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, removed_sink->host.initialize("failing.example.com"));
    removed_sink->port = 7777;
    removed_sink->use_tcp = true;
    healthy_sink->socket_fd = 2222;
    healthy_sink->send_function = mock_successful_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, healthy_sink->host.initialize("healthy.example.com"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, healthy_sink->host.get_error());
    healthy_sink->port = 8888;
    healthy_sink->use_tcp = false;
    g_sink_to_remove = removed_sink;
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, removed_sink));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, healthy_sink));
    FT_ASSERT_EQ(-1, ft_log_probe_remote_health());
    status_count = 0;
    FT_ASSERT_EQ(0, ft_log_get_remote_health(&status, 1, &status_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), status_count);
    FT_ASSERT(status.host != ft_nullptr);
    FT_ASSERT(ft_strcmp(status.host, "healthy.example.com") == 0);
    FT_ASSERT_EQ(8888, status.port);
    FT_ASSERT(!status.use_tcp);
    FT_ASSERT(status.last_check != 0);
    FT_ASSERT(status.reachable);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status.last_error);
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    ft_log_remove_sink(ft_network_sink, healthy_sink);
    delete healthy_sink;
    return (1);
}

FT_TEST(test_logger_remote_health_enable_toggle)
{
    ft_log_enable_remote_health(false);
    ft_log_enable_remote_health(true);
    ft_log_enable_remote_health(true);
    ft_log_enable_remote_health(false);
    ft_log_enable_remote_health(false);
    return (1);
}

FT_TEST(test_logger_remote_health_interval_updates)
{
    ft_log_enable_remote_health(false);
    ft_log_set_remote_health_interval(0);
    ft_log_set_remote_health_interval(5);
    ft_log_enable_remote_health(true);
    ft_log_set_remote_health_interval(1);
    ft_log_enable_remote_health(false);
    return (1);
}

FT_TEST(test_logger_remote_health_get_requires_capacity)
{
    s_network_sink *first_sink;
    s_network_sink *second_sink;
    s_log_remote_health statuses[1];
    ft_size_t status_count;
    ft_size_t peek_count;

    reset_mock_network_send_state();
    first_sink = new s_network_sink();
    FT_ASSERT(first_sink != ft_nullptr);
    first_sink->socket_fd = 3333;
    first_sink->send_function = mock_successful_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_sink->host.initialize("capacity-one.example"));
    first_sink->port = 1000;
    first_sink->use_tcp = true;
    second_sink = new s_network_sink();
    FT_ASSERT(second_sink != ft_nullptr);
    second_sink->socket_fd = 4444;
    second_sink->send_function = mock_successful_send;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_sink->host.initialize("capacity-two.example"));
    second_sink->port = 2000;
    second_sink->use_tcp = false;
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, first_sink));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_network_sink, second_sink));
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    status_count = 0;
    statuses[0].host = "retain";
    statuses[0].port = 9999;
    statuses[0].use_tcp = true;
    statuses[0].reachable = true;
    statuses[0].last_check = 123;
    statuses[0].last_error = FT_ERR_SUCCESS;
    int get_result;

    get_result = ft_log_get_remote_health(statuses, 1, &status_count);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, get_result);
    FT_ASSERT(status_count >= static_cast<ft_size_t>(2));
    FT_ASSERT(statuses[0].host != ft_nullptr);
    FT_ASSERT(ft_strcmp(statuses[0].host, "retain") == 0);
    FT_ASSERT_EQ(static_cast<unsigned short>(9999), statuses[0].port);
    FT_ASSERT(statuses[0].use_tcp);
    FT_ASSERT(statuses[0].reachable);
    peek_count = 0;
    FT_ASSERT_EQ(0, ft_log_get_remote_health(ft_nullptr, 0, &peek_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), peek_count);
    ft_log_remove_sink(ft_network_sink, first_sink);
    ft_log_remove_sink(ft_network_sink, second_sink);
    delete first_sink;
    delete second_sink;
    FT_ASSERT_EQ(0, ft_log_probe_remote_health());
    return (1);
}

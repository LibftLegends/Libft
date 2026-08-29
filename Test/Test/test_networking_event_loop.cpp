#include "../test_internal.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

static int make_socket_pair(int descriptors[2])
{
    int listener;
    int client;
    int server;
    struct sockaddr_in address;
    socklen_t address_length;

    listener = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return (-1);
    ft_bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (nw_bind(listener, reinterpret_cast<const sockaddr *>(&address),
            sizeof(address)) != 0 || nw_listen(listener, 1) != 0)
    {
        nw_close(listener);
        return (-1);
    }
    address_length = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&address),
            &address_length) != 0)
    {
        nw_close(listener);
        return (-1);
    }
    client = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0 || nw_connect(client,
            reinterpret_cast<const sockaddr *>(&address),
            sizeof(address)) != 0)
    {
        if (client >= 0)
            nw_close(client);
        nw_close(listener);
        return (-1);
    }
    server = nw_accept(listener, ft_nullptr, ft_nullptr);
    nw_close(listener);
    if (server < 0)
    {
        nw_close(client);
        return (-1);
    }
    descriptors[0] = server;
    descriptors[1] = client;
    return (0);
}

static int make_pipe(int descriptors[2])
{
    return (make_socket_pair(descriptors));
}

static ft_bool wait_for_descriptor(event_loop *loop, int32_t file_descriptor,
    uint32_t ready_mask, uint32_t attempts)
{
    event_loop_ready_event event;
    uint32_t event_count;
    uint32_t attempt_index;

    attempt_index = 0U;
    while (attempt_index < attempts)
    {
        event_count = 0U;
        if (event_loop_wait(loop, &event, 1U, &event_count, 0)
            != FT_ERR_SUCCESS)
            return (FT_FALSE);
        if (event_count == 1U && event.file_descriptor == file_descriptor
            && (event.ready_mask & ready_mask) != 0U)
            return (FT_TRUE);
        std::this_thread::yield();
        attempt_index += 1U;
    }
    return (FT_FALSE);
}

static void close_pipe(int descriptors[2])
{
    nw_close(descriptors[0]);
    nw_close(descriptors[1]);
}

static int32_t consume_one_byte(int32_t file_descriptor)
{
    char value;

    return (static_cast<int32_t>(nw_recv(file_descriptor, &value, 1U, 0)));
}

static int write_test_descriptor(int descriptor, const char *value,
    size_t size)
{
    return (static_cast<int>(nw_send(descriptor, value, size, 0)));
}

static void close_socket_pair(int descriptors[2])
{
    close_pipe(descriptors);
}

static void close_one_socket(int descriptor)
{
    if (descriptor >= 0)
        (void)nw_close(descriptor);
}

FT_TEST(test_networking_event_loop_add_socket_reports_allocation_failure)
{
    event_loop loop;
    int descriptors[2];
    int add_result;

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    cma_set_alloc_limit(1);
    add_result = event_loop_add_socket(&loop, descriptors[0], FT_FALSE);
    cma_set_alloc_limit(0);
    if (add_result != FT_ERR_NO_MEMORY || loop.registration_count != 0U)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_remove_socket_sets_errno_when_missing)
{
    event_loop loop;
    int descriptors[2];

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_socket(&loop, descriptors[0], FT_FALSE) != FT_ERR_SUCCESS
        || event_loop_remove_socket(&loop, descriptors[1], FT_FALSE)
            != FT_ERR_INVALID_ARGUMENT
        || loop.read_count != 1 || loop.read_file_descriptors[0] != descriptors[0])
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_init_sets_up_thread_safety)
{
    event_loop loop;

    event_loop_init(&loop);
    if (!loop.thread_safe_enabled || loop.mutex == ft_nullptr
        || loop.wait_mutex == ft_nullptr || loop.wakeup_read_descriptor < 0
        || loop.wakeup_write_descriptor < 0)
    {
        event_loop_clear(&loop);
        return (0);
    }
    event_loop_clear(&loop);
    return (1);
}

FT_TEST(test_networking_event_loop_lock_and_unlock_reset_errno)
{
    event_loop loop;
    ft_bool lock_acquired;
    int lock_result;

    event_loop_init(&loop);
    lock_acquired = FT_FALSE;
    lock_result = event_loop_lock(&loop, &lock_acquired);
    if (lock_result != FT_ERR_SUCCESS || !lock_acquired)
    {
        event_loop_clear(&loop);
        return (0);
    }
    event_loop_unlock(&loop, lock_acquired);
    event_loop_clear(&loop);
    return (1);
}

FT_TEST(test_networking_event_loop_keeps_non_ready_registration)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    int descriptors[2];
    char value;

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_socket(&loop, descriptors[0], FT_FALSE) != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 0) != FT_ERR_SUCCESS
        || event_count != 0U || loop.read_count != 1
        || loop.read_file_descriptors[0] != descriptors[0]
        || write_test_descriptor(descriptors[1], "x", 1U) != 1)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 0) != FT_ERR_SUCCESS
        || event_count != 1U || event.file_descriptor != descriptors[0]
        || (event.ready_mask & EVENT_LOOP_READY_READ) == 0U)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    (void)value;
    (void)consume_one_byte(descriptors[0]);
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_merges_read_write_interest)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    int descriptors[2];

    if (make_socket_pair(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_interest(&loop, descriptors[1],
        EVENT_LOOP_INTEREST_READ)
        != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_socket_pair(descriptors);
        return (0);
    }
    if (event_loop_add_interest(&loop, descriptors[1],
        EVENT_LOOP_INTEREST_WRITE)
        != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_socket_pair(descriptors);
        return (0);
    }
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 0)
        != FT_ERR_SUCCESS
        || event_count != 1U || event.file_descriptor != descriptors[1]
        || (event.ready_mask & EVENT_LOOP_READY_WRITE) == 0U)
    {
        event_loop_clear(&loop);
        close_socket_pair(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_socket_pair(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_interrupt_wakes_waiter)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    std::atomic<int> wait_result;
    std::thread waiter;

    event_loop_init(&loop);
    wait_result.store(99);
    waiter = std::thread([&loop, &event, &event_count, &wait_result]()
    {
        event_count = 0U;
        wait_result.store(event_loop_wait(&loop, &event, 1U, &event_count, -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (event_loop_interrupt(&loop) != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        waiter.join();
        return (0);
    }
    waiter.join();
    event_loop_clear(&loop);
    if (wait_result.load() != FT_ERR_SUCCESS || event_count != 0U)
        return (0);
    return (1);
}

FT_TEST(test_networking_event_loop_add_while_waiting_wakes_and_reports)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    std::atomic<int32_t> wait_result;
    std::atomic<uint32_t> observed_count;
    std::thread waiter;
    int descriptors[2];
    char value;
    uint32_t attempt_index;

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    wait_result.store(FT_ERR_INVALID_STATE);
    observed_count.store(0U);
    waiter = std::thread([&loop, &event, &event_count, &wait_result,
        &observed_count]()
    {
        uint32_t wait_attempt_index;
        int32_t result_code;

        wait_attempt_index = 0U;
        result_code = FT_ERR_SUCCESS;
        event_count = 0U;
        while (wait_attempt_index < 100U)
        {
            result_code = event_loop_wait(&loop, &event, 1U, &event_count,
                -1);
            if (result_code != FT_ERR_SUCCESS || event_count != 0U)
                break ;
            wait_attempt_index += 1U;
        }
        wait_result.store(result_code);
        observed_count.store(event_count);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (event_loop_add_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS
        || write_test_descriptor(descriptors[1], "x", 1U) != 1)
    {
        std::fprintf(stderr, "event-loop add/write failed: read=%d write=%d\\n",
            loop.read_count, loop.write_count);
        (void)event_loop_interrupt(&loop);
        waiter.join();
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    attempt_index = 0U;
    while (attempt_index < 100U && waiter.joinable())
    {
        if (observed_count.load() != 0U || wait_result.load() != FT_ERR_INVALID_STATE)
            break ;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        attempt_index += 1U;
    }
    waiter.join();
    if (wait_result.load() != FT_ERR_SUCCESS || observed_count.load() != 1U
        || event.file_descriptor != descriptors[0]
        || (event.ready_mask & EVENT_LOOP_READY_READ) == 0U)
    {
        std::fprintf(stderr, "event-loop wake result=%d count=%u fd=%d expected=%d mask=%u\\n",
            wait_result.load(), observed_count.load(), event.file_descriptor,
            descriptors[0], event.ready_mask);
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    (void)value;
    (void)consume_one_byte(descriptors[0]);
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_remove_while_waiting_is_safe)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    std::atomic<int32_t> wait_result;
    std::thread waiter;
    int descriptors[2];

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    wait_result.store(FT_ERR_INVALID_STATE);
    waiter = std::thread([&loop, &event, &event_count, &wait_result]()
    {
        event_count = 0U;
        wait_result.store(event_loop_wait(&loop, &event, 1U, &event_count,
            -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (event_loop_remove_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS)
    {
        (void)event_loop_interrupt(&loop);
        waiter.join();
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    (void)event_loop_interrupt(&loop);
    waiter.join();
    if (wait_result.load() != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 0)
        != FT_ERR_SUCCESS || event_count != 0U)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_destroy_wakes_waiter)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    std::atomic<int32_t> wait_result;
    std::thread waiter;

    event_loop_init(&loop);
    wait_result.store(FT_ERR_SUCCESS);
    waiter = std::thread([&loop, &event, &event_count, &wait_result]()
    {
        event_count = 0U;
        wait_result.store(event_loop_wait(&loop, &event, 1U, &event_count,
            -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    event_loop_clear(&loop);
    waiter.join();
    if (wait_result.load() != FT_ERR_INVALID_STATE)
        return (0);
    return (1);
}

FT_TEST(test_networking_event_loop_repeated_waits_keep_registration)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    int descriptors[2];
    char value;

    if (make_pipe(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    if (write_test_descriptor(descriptors[1], "a", 1U) != 1
        || wait_for_descriptor(&loop, descriptors[0], EVENT_LOOP_READY_READ,
            10U) == FT_FALSE)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    (void)value;
    (void)consume_one_byte(descriptors[0]);
    if (write_test_descriptor(descriptors[1], "b", 1U) != 1
        || wait_for_descriptor(&loop, descriptors[0], EVENT_LOOP_READY_READ,
            10U) == FT_FALSE)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    (void)value;
    (void)consume_one_byte(descriptors[0]);
    event_count = 0U;
    if (event_loop_remove_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS
        || event_loop_wait(&loop, &event, 1U, &event_count, 0)
            != FT_ERR_SUCCESS || event_count != 0U)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_pipe(descriptors);
    return (1);
}

FT_TEST(test_networking_event_loop_finite_timeout_is_honoured)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;
    int64_t elapsed_milliseconds;

    event_loop_init(&loop);
    start_time = std::chrono::steady_clock::now();
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 20)
        != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        return (0);
    }
    finish_time = std::chrono::steady_clock::now();
    elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        finish_time - start_time).count();
    event_loop_clear(&loop);
    if (event_count != 0U || elapsed_milliseconds < 10
        || elapsed_milliseconds > 500)
        return (0);
    return (1);
}

FT_TEST(test_networking_event_loop_second_waiter_is_rejected)
{
    event_loop loop;
    event_loop_ready_event first_event;
    event_loop_ready_event second_event;
    uint32_t first_event_count;
    uint32_t second_event_count;
    std::atomic<int32_t> first_result;
    std::thread waiter;

    event_loop_init(&loop);
    first_result.store(FT_ERR_INVALID_STATE);
    waiter = std::thread([&loop, &first_event, &first_event_count,
        &first_result]()
    {
        first_event_count = 0U;
        first_result.store(event_loop_wait(&loop, &first_event, 1U,
            &first_event_count, -1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    second_event_count = 0U;
    if (event_loop_wait(&loop, &second_event, 1U, &second_event_count, 0)
        != FT_ERR_THREAD_BUSY)
    {
        (void)event_loop_interrupt(&loop);
        waiter.join();
        event_loop_clear(&loop);
        return (0);
    }
    (void)event_loop_interrupt(&loop);
    waiter.join();
    event_loop_clear(&loop);
    if (first_result.load() != FT_ERR_SUCCESS)
        return (0);
    return (1);
}

FT_TEST(test_networking_event_loop_peer_close_reports_error_or_hangup)
{
    event_loop loop;
    event_loop_ready_event event;
    uint32_t event_count;
    int descriptors[2];

    if (make_socket_pair(descriptors) != 0)
        return (0);
    event_loop_init(&loop);
    if (event_loop_add_interest(&loop, descriptors[0],
        EVENT_LOOP_INTEREST_READ) != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    close_one_socket(descriptors[1]);
    descriptors[1] = -1;
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 100)
        != FT_ERR_SUCCESS || event_count != 1U
        || event.file_descriptor != descriptors[0]
        || (event.ready_mask & (EVENT_LOOP_READY_ERROR
            | EVENT_LOOP_READY_HANGUP)) == 0U)
    {
        event_loop_clear(&loop);
        close_one_socket(descriptors[0]);
        return (0);
    }
    event_loop_clear(&loop);
    close_one_socket(descriptors[0]);
    return (1);
}

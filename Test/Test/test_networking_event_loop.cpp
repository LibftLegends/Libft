#include "../test_internal.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>

static int make_pipe(int descriptors[2])
{
    return (pipe(descriptors));
}

static void close_pipe(int descriptors[2])
{
    close(descriptors[0]);
    close(descriptors[1]);
}

static int make_socket_pair(int descriptors[2])
{
    return (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors));
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
        || loop.wait_mutex == ft_nullptr || loop.backend_descriptor < 0)
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
        || write(descriptors[1], "x", 1U) != 1)
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
    (void)read(descriptors[0], &value, 1U);
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
        close_pipe(descriptors);
        return (0);
    }
    if (event_loop_add_interest(&loop, descriptors[1],
        EVENT_LOOP_INTEREST_WRITE)
        != FT_ERR_SUCCESS)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_count = 0U;
    if (event_loop_wait(&loop, &event, 1U, &event_count, 0)
        != FT_ERR_SUCCESS
        || event_count != 1U || event.file_descriptor != descriptors[1]
        || (event.ready_mask & EVENT_LOOP_READY_WRITE) == 0U)
    {
        event_loop_clear(&loop);
        close_pipe(descriptors);
        return (0);
    }
    event_loop_clear(&loop);
    close_pipe(descriptors);
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

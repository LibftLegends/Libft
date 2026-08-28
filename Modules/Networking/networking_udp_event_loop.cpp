#include "udp_socket.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t udp_event_loop_wait_internal(event_loop *loop, udp_socket &socket,
                                        ft_bool is_write, int32_t timeout_milliseconds)
{
    int32_t socket_fd;
    int32_t wait_result;
    uint32_t event_count;
    uint32_t interest_mask;
    uint32_t ready_mask;
    event_loop_ready_event event;
    ft_bool descriptor_ready;

    if (loop == ft_nullptr)
        return (-1);
    socket_fd = socket.get_fd();
    if (socket_fd < 0)
        return (-1);
    if (is_write == FT_FALSE)
    {
        interest_mask = EVENT_LOOP_INTEREST_READ;
        ready_mask = EVENT_LOOP_READY_READ;
    }
    else
    {
        interest_mask = EVENT_LOOP_INTEREST_WRITE;
        ready_mask = EVENT_LOOP_READY_WRITE;
    }
    if (event_loop_add_interest(loop, socket_fd, interest_mask) != FT_ERR_SUCCESS)
        return (-1);
    event_count = 0U;
    wait_result = event_loop_wait(loop, &event, 1U, &event_count,
        timeout_milliseconds);
    descriptor_ready = FT_FALSE;
    if (wait_result == FT_ERR_SUCCESS && event_count == 1U
        && event.file_descriptor == socket_fd)
    {
        if ((event.ready_mask & ready_mask) != 0U)
            descriptor_ready = FT_TRUE;
    }
    if (event_loop_remove_interest(loop, socket_fd, interest_mask)
        != FT_ERR_SUCCESS && wait_result == FT_ERR_SUCCESS)
        wait_result = FT_ERR_INVALID_STATE;
    if (wait_result != FT_ERR_SUCCESS)
        return (-1);
    if (!descriptor_ready)
        return (0);
    return (1);
}

int32_t udp_event_loop_wait_read(event_loop *loop, udp_socket &socket, int32_t timeout_milliseconds)
{
    return (udp_event_loop_wait_internal(loop, socket, FT_FALSE, timeout_milliseconds));
}

int32_t udp_event_loop_wait_write(event_loop *loop, udp_socket &socket, int32_t timeout_milliseconds)
{
    return (udp_event_loop_wait_internal(loop, socket, FT_TRUE, timeout_milliseconds));
}

ssize_t udp_event_loop_receive(event_loop *loop, udp_socket &socket, void *buffer, ft_size_t size,
                               int32_t flags, struct sockaddr *source_address,
                               socklen_t *address_length, int32_t timeout_milliseconds)
{
    int32_t wait_result;

    if (size > 0 && buffer == ft_nullptr)
        return (-1);
    wait_result = udp_event_loop_wait_internal(loop, socket, FT_FALSE, timeout_milliseconds);
    if (wait_result < 0)
        return (-1);
    if (wait_result == 0)
        return (0);
    return (socket.receive_from(buffer, size, flags, source_address, address_length));
}

ssize_t udp_event_loop_send(event_loop *loop, udp_socket &socket, const void *data, ft_size_t size,
                            int32_t flags, const struct sockaddr *destination_address,
                            socklen_t address_length, int32_t timeout_milliseconds)
{
    int32_t wait_result;

    if (size > 0 && data == ft_nullptr)
        return (-1);
    wait_result = udp_event_loop_wait_internal(loop, socket, FT_TRUE, timeout_milliseconds);
    if (wait_result < 0)
        return (-1);
    if (wait_result == 0)
        return (0);
    return (socket.send_to(data, size, flags, destination_address, address_length));
}

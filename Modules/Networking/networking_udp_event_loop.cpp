#include "udp_socket.hpp"
#include "networking_event_loop_internal.hpp"
#include "../Basic/class_nullptr.hpp"

static int32_t udp_event_loop_wait_internal(event_loop *loop, udp_socket &socket,
                                        ft_bool is_write, int32_t timeout_milliseconds)
{
    if (loop == ft_nullptr)
        return (-1);
    return (event_loop_wait_for_socket(loop, socket.get_fd(), is_write, timeout_milliseconds));
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

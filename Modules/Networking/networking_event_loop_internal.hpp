#ifndef NETWORKING_EVENT_LOOP_INTERNAL_HPP
# define NETWORKING_EVENT_LOOP_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "networking.hpp"

int32_t event_loop_wait_for_socket(event_loop *loop, int32_t socket_descriptor,
    ft_bool wait_for_write, int32_t timeout_milliseconds) noexcept;

#endif

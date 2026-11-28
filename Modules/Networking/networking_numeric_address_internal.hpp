#ifndef NETWORKING_NUMERIC_ADDRESS_INTERNAL_HPP
# define NETWORKING_NUMERIC_ADDRESS_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "networking.hpp"
#include "../Basic/basic.hpp"

static inline ft_bool networking_configure_numeric_address(struct sockaddr_storage &storage,
    int32_t family, const char *host, uint16_t port)
{
    if (family == AF_INET)
    {
        struct sockaddr_in address{};
        if (host[0] != '\0' && nw_inet_pton(AF_INET, host, &address.sin_addr) != 1)
            return (FT_FALSE);
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
    || defined(__NetBSD__) || defined(__DragonFly__)
        address.sin_len = sizeof(address);
#endif
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        ft_bzero(&storage, sizeof(storage));
        ft_memcpy_fixed<sizeof(address)>(&storage, &address);
        return (FT_TRUE);
    }
    if (family == AF_INET6)
    {
        struct sockaddr_in6 address{};
        if (host[0] != '\0' && nw_inet_pton(AF_INET6, host, &address.sin6_addr) != 1)
            return (FT_FALSE);
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
    || defined(__NetBSD__) || defined(__DragonFly__)
        address.sin6_len = sizeof(address);
#endif
        address.sin6_family = AF_INET6;
        address.sin6_port = htons(port);
        ft_bzero(&storage, sizeof(storage));
        ft_memcpy_fixed<sizeof(address)>(&storage, &address);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

#endif

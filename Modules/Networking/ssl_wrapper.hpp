#ifndef NETWORKING_SSL_WRAPPER_HPP
#define NETWORKING_SSL_WRAPPER_HPP

#include "openssl_support.hpp"
#include "../Basic/basic.hpp"

#if NETWORKING_HAS_OPENSSL
#include <cstddef>

#ifdef _WIN32
# include <basetsd.h>
typedef SSIZE_T ssize_t;
#else
# include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

ssize_t nw_ssl_write(SSL *ssl, const void *buf, ft_size_t len);
ssize_t nw_ssl_read(SSL *ssl, void *buf, ft_size_t len);

#ifdef __cplusplus
}

ssize_t networking_ssl_write_with_status(SSL *ssl, const void *buffer,
    ft_size_t length, int32_t &ssl_error);
ssize_t networking_ssl_read_with_status(SSL *ssl, void *buffer,
    ft_size_t length, int32_t &ssl_error);
#endif

#endif

#endif

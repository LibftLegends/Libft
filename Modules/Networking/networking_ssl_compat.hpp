#ifndef NETWORKING_SSL_COMPAT_HPP
#define NETWORKING_SSL_COMPAT_HPP

#include "openssl_support.hpp"
#include <cstdint>

#if NETWORKING_HAS_OPENSSL
int32_t networking_check_ssl_after_send(SSL *ssl_connection);
#endif

#endif

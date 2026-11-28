#include "ssl_wrapper.hpp"
#include <climits>
#if NETWORKING_HAS_OPENSSL

static ssize_t ssl_translate_result(SSL *ssl, int32_t result, int32_t &ssl_error)
{
    if (result > 0)
    {
        ssl_error = SSL_ERROR_NONE;
        return (result);
    }
    ssl_error = SSL_get_error(ssl, result);
    if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE
        || ssl_error == SSL_ERROR_ZERO_RETURN)
        return (0);
    return (-1);
}

ssize_t networking_ssl_write_with_status(SSL *ssl, const void *buffer,
    ft_size_t length, int32_t &ssl_error)
{
    if (ssl == ft_nullptr || length > static_cast<ft_size_t>(INT_MAX))
    {
        ssl_error = SSL_ERROR_SSL;
        return (-1);
    }
    const int32_t result = SSL_write(ssl, buffer, static_cast<int32_t>(length));
    return (ssl_translate_result(ssl, result, ssl_error));
}

ssize_t networking_ssl_read_with_status(SSL *ssl, void *buffer,
    ft_size_t length, int32_t &ssl_error)
{
    if (ssl == ft_nullptr || length > static_cast<ft_size_t>(INT_MAX))
    {
        ssl_error = SSL_ERROR_SSL;
        return (-1);
    }
    const int32_t result = SSL_read(ssl, buffer, static_cast<int32_t>(length));
    return (ssl_translate_result(ssl, result, ssl_error));
}

extern "C"
{
    ssize_t nw_ssl_write(SSL *ssl, const void *buf, ft_size_t len)
    {
        int32_t ssl_error;
        return (networking_ssl_write_with_status(ssl, buf, len, ssl_error));
    }

    ssize_t nw_ssl_read(SSL *ssl, void *buf, ft_size_t len)
    {
        int32_t ssl_error;
        return (networking_ssl_read_with_status(ssl, buf, len, ssl_error));
    }
}

#endif

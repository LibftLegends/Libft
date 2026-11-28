#include "../test_internal.hpp"
#include "../../Modules/Networking/ssl_wrapper.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if NETWORKING_HAS_OPENSSL
#include <memory>

FT_TEST(test_networking_ssl_wrapper_preserves_retry_and_length_errors)
{
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
    FT_ASSERT(context.get() != ft_nullptr);
    std::unique_ptr<SSL, decltype(&SSL_free)> session(
        SSL_new(context.get()), SSL_free);
    FT_ASSERT(session.get() != ft_nullptr);
    BIO *session_bio = ft_nullptr;
    BIO *peer_bio = ft_nullptr;
    FT_ASSERT_EQ(1, BIO_new_bio_pair(&session_bio, 1U, &peer_bio, 1U));
    std::unique_ptr<BIO, decltype(&BIO_free)> peer(peer_bio, BIO_free);
    SSL_set_bio(session.get(), session_bio, session_bio);
    SSL_set_connect_state(session.get());

    int32_t ssl_error = SSL_ERROR_NONE;
    char byte = 'x';
    constexpr ft_size_t UNREPRESENTABLE_LENGTH = static_cast<ft_size_t>(INT32_MAX) + 1U;
    FT_ASSERT_EQ(-1, networking_ssl_write_with_status(session.get(), &byte,
        UNREPRESENTABLE_LENGTH, ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_SSL, ssl_error);
    FT_ASSERT_EQ(-1, networking_ssl_read_with_status(session.get(), &byte,
        UNREPRESENTABLE_LENGTH, ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_SSL, ssl_error);
    FT_ASSERT_EQ(0, nw_ssl_write(session.get(), &byte, sizeof(byte)));
    FT_ASSERT_EQ(0, networking_ssl_write_with_status(session.get(), &byte,
        sizeof(byte), ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_WANT_WRITE, ssl_error);
    return (1);
}

#endif

#include "../../../Modules/Networking/ssl_wrapper.hpp"
#include <chrono>
#include <cstdio>
#include <memory>

#if !NETWORKING_HAS_OPENSSL
# error This benchmark requires OpenSSL.
#endif

static int32_t measure_retry(SSL *session, ft_bool with_status)
{
    constexpr uint32_t ITERATIONS = 250000U;
    char byte = 0;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        int32_t ssl_error;
        ssize_t received;
        if (with_status != FT_FALSE)
        {
            received = networking_ssl_read_with_status(session, &byte,
                sizeof(byte), ssl_error);
        }
        else
        {
            received = nw_ssl_read(session, &byte, sizeof(byte));
            ssl_error = SSL_get_error(session, static_cast<int32_t>(received));
        }
        if (received != 0 || ssl_error != SSL_ERROR_WANT_READ)
            return (1);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const char *operation = "Legacy wrapper plus caller classification";
    if (with_status != FT_FALSE)
        operation = "Wrapper with status";
    std::printf("%s: %.2f ns per retry\n", operation, elapsed_ns / ITERATIONS);
    return (0);
}

int32_t main()
{
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
    if (!context)
        return (1);
    std::unique_ptr<SSL, decltype(&SSL_free)> session(
        SSL_new(context.get()), SSL_free);
    if (!session)
        return (1);
    BIO *session_bio = ft_nullptr;
    BIO *peer_bio = ft_nullptr;
    if (BIO_new_bio_pair(&session_bio, 0U, &peer_bio, 0U) != 1)
        return (1);
    std::unique_ptr<BIO, decltype(&BIO_free)> peer(peer_bio, BIO_free);
    SSL_set_bio(session.get(), session_bio, session_bio);
    SSL_set_connect_state(session.get());
    if (measure_retry(session.get(), FT_FALSE) != 0
        || measure_retry(session.get(), FT_TRUE) != 0)
        return (1);
    return (0);
}

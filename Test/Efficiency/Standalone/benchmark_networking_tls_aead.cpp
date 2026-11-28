#include "../../../Modules/Networking/networking_tls_aead.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <memory>

#if !NETWORKING_HAS_OPENSSL
# error This benchmark requires OpenSSL.
#endif

static ft_bool finish_handshake(SSL *client, SSL *server)
{
    BIO *client_bio = ft_nullptr;
    BIO *server_bio = ft_nullptr;
    if (BIO_new_bio_pair(&client_bio, 0U, &server_bio, 0U) != 1)
        return (FT_FALSE);
    SSL_set_bio(client, client_bio, client_bio);
    SSL_set_bio(server, server_bio, server_bio);
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    while (SSL_is_init_finished(client) != 1 || SSL_is_init_finished(server) != 1)
    {
        const int32_t client_result = SSL_do_handshake(client);
        if (client_result != 1
            && SSL_get_error(client, client_result) != SSL_ERROR_WANT_READ)
            return (FT_FALSE);
        const int32_t server_result = SSL_do_handshake(server);
        if (server_result != 1
            && SSL_get_error(server, server_result) != SSL_ERROR_WANT_READ)
            return (FT_FALSE);
    }
    return (FT_TRUE);
}

static int32_t measure(SSL *session, ft_bool initialize_contexts)
{
    constexpr uint32_t ITERATIONS = 100000U;
    ft_vector<unsigned char> send_key;
    ft_vector<unsigned char> receive_key;
    ft_vector<unsigned char> send_iv;
    ft_vector<unsigned char> receive_iv;
    if (!networking_tls_export_aead_keys(session, FT_TRUE, send_key, send_iv,
            receive_key, receive_iv))
        return (1);
    ft_size_t initial_allocations = 0U;
    if (cma_get_stats(&initial_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (initialize_contexts != FT_FALSE)
        {
            encryption_aead_context send_context;
            encryption_aead_context receive_context;
            if (!networking_tls_initialize_aead_contexts(session, FT_TRUE,
                    send_context, receive_context, send_iv, receive_iv))
                return (1);
        }
        else if (!networking_tls_export_aead_keys(session, FT_TRUE, send_key,
                send_iv, receive_key, receive_iv))
            return (1);
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t final_allocations = 0U;
    if (cma_get_stats(&final_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    const char *operation = "TLS key export";
    if (initialize_contexts != FT_FALSE)
        operation = "TLS AEAD context initialization";
    std::printf("%s, %.3f us and %.2f CMA allocations per call\n",
        operation, elapsed_us / ITERATIONS,
        static_cast<double>(final_allocations - initial_allocations) / ITERATIONS);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 3)
    {
        std::fprintf(stderr, "Usage: %s <certificate.pem> <private-key.pem>\n",
            arguments[0]);
        return (1);
    }
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        SSL_CTX_new(TLS_method()), SSL_CTX_free);
    if (!context || SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1
        || SSL_CTX_set_max_proto_version(context.get(), TLS1_2_VERSION) != 1
        || SSL_CTX_set_cipher_list(context.get(), "AES128-GCM-SHA256") != 1
        || SSL_CTX_use_certificate_file(context.get(), arguments[1], SSL_FILETYPE_PEM) != 1
        || SSL_CTX_use_PrivateKey_file(context.get(), arguments[2], SSL_FILETYPE_PEM) != 1)
        return (1);
    std::unique_ptr<SSL, decltype(&SSL_free)> client(SSL_new(context.get()), SSL_free);
    std::unique_ptr<SSL, decltype(&SSL_free)> server(SSL_new(context.get()), SSL_free);
    if (!client || !server || !finish_handshake(client.get(), server.get())
        || measure(client.get(), FT_FALSE) != 0 || measure(client.get(), FT_TRUE) != 0)
        return (1);
    return (0);
}

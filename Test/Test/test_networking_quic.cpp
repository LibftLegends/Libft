#include "../test_internal.hpp"
#include "../../Modules/Networking/networking_quic_experimental.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if NETWORKING_HAS_OPENSSL
#include <openssl/ssl.h>
#include <openssl/pem.h>

static const char g_quic_test_certificate[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDCDCCAfCgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwGjEYMBYGA1UEAwwPTGli\n"
"ZnQgVGVzdCBSb290MB4XDTI1MTAyMTEwMDgxNFoXDTI4MDEyNDEwMDgxNFowFDES\n"
"MBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\n"
"AQEAtuaAY54vZfFe422yWlt9xRky2XBPFindiooNhY9gxAOaUdOlaAhoMgXrsuGl\n"
"tCqxxS0Y2WYcTAQ+yXq4DE0QUVuyDgTaoDFQqFdZQh/mSxAiK6nveHgdZMoy5pwa\n"
"TgR5La2qnAQohm9EA1SyG4aByQxkU0EkP/rVqTLFE3m5Bn5n71czGc+zTCb6RrBW\n"
"eViK6FmpUzTcmYnqrQ579T2D/vhhX9Iz1UumqQ+FbFDXgWCVUC8cvSQH/HV/XrEd\n"
"VulRKCaiTfztKFtezVGytiXDbjavTBZvVzY2v/atTykYwVfLVjHbIl7H6fbmVdsT\n"
"a0X4Q4UQKQIUqPcgYe1fkSBopQIDAQABo14wXDAaBgNVHREEEzARgglsb2NhbGhv\n"
"c3SHBH8AAAEwHQYDVR0OBBYEFPxDmhTN56MX5fMD+suMcwYBenSvMB8GA1UdIwQY\n"
"MBaAFJv+YkbgpXmODyp3zRibJT3v+n+9MA0GCSqGSIb3DQEBCwUAA4IBAQBoVq3z\n"
"zxbPcDDCrLDetjWBHo5G0aNlBZH3wpwk7plqzBwGk7yn15DgoHNTJN6gTTTmF5bP\n"
"Bxj4KU3E/WN1H+om1R6atUH5yWFzGAhP/QFpxUoqP1E+vztkE5cIhrUGrn1NOCBm\n"
"hBVeCkrf1ZmY4InMRhLCk3wK1vkiY1dFMeTVGduO2LufSnPm6fg1zUhd8IeYgZve\n"
"gBWvlmx7zSerCLlpY97bKIlSAKwGijCPpVWb3IeVJH+8qd6dIyrcOx1rYvhnF314\n"
"RQXNzrMmTyhM60Wnq+2Vsa/tEXl54tHBJDixYPMJX1ktoZz+CP21KnhuwSDcFs20\n"
"8BEYI1YHVisx6+Dt\n"
"-----END CERTIFICATE-----\n";

static const char g_quic_test_key[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC25oBjni9l8V7j\n"
"bbJaW33FGTLZcE8WKd2Kig2Fj2DEA5pR06VoCGgyBeuy4aW0KrHFLRjZZhxMBD7J\n"
"ergMTRBRW7IOBNqgMVCoV1lCH+ZLECIrqe94eB1kyjLmnBpOBHktraqcBCiGb0QD\n"
"VLIbhoHJDGRTQSQ/+tWpMsUTebkGfmfvVzMZz7NMJvpGsFZ5WIroWalTNNyZieqt\n"
"Dnv1PYP++GFf0jPVS6apD4VsUNeBYJVQLxy9JAf8dX9esR1W6VEoJqJN/O0oW17N\n"
"UbK2JcNuNq9MFm9XNja/9q1PKRjBV8tWMdsiXsfp9uZV2xNrRfhDhRApAhSo9yBh\n"
"7V+RIGilAgMBAAECggEAU9otdn5pQJV+aUo7gI19hcVduK5ys7iR5xgPxvHK7VKx\n"
"h14+8QkZrAVm3+XvsD/6rGlWgST9SCSSHJD0nD8gggxF/4NeMB8r2WHifBxf2n4V\n"
"DGumbFa1MyY4W92peac3XyfFli2Z3rhYOCa2WUQ7cE8MF9c/pU18c0vhamJd34mB\n"
"5e9ECzjFG/dVQ96dmfwgGMfGLcayTUKKVHYZeCKT1OzVsIc3cDY66Gqm9XbpA5BX\n"
"NeWh9gyfyQnNRWUN3QSAEid/4P7UXzeOCc1ikO7k5xT/AizsXuaP6h1jSJZ69Y42\n"
"l+DKSoZ6uL8F6pmgikJEsyep4Njh/9EshjxpmZf2KQKBgQDePPI3Ig9nkzHJ69DR\n"
"BEop7IqAWaV1HYGB+tA3/2oRR7Rr8w3e/S6CEOo+BsNu8PUj10Cu/fBvH5BEZHSb\n"
"OmLYnSbDyV7oS6QW5qABT4w4Pfr5wWFg25ZZH1hZz8aIykENV/8AqXXfm3+dqumU\n"
"RaYUjcsjpfY5rbHgSSpNssmQ5wKBgQDSr6zTzJBBdTNpEprwCms3CgWqIQBmPayh\n"
"0xxa+uL46hMdgngGMKAOTL63m44Rp0Jv5xL/uAXCbXiLDs0wUPS2wJzYqz374aQq\n"
"8A3lbD6q/HtjjMCwy6frQQtFfZBNky/tQzwwr6voowpSLfjQe4CCe0HQOGKeXMQ3\n"
"bZlOCeGskwKBgCQomGQ+B3GbsmMfGieX4hU2JkbBzzjh5xtl2pDx3qkXGJTjw+L/\n"
"IJ0K8YHMCeo4DDqboCuYp5D8+rUV/x7uffyufQsn7WgwPm93cJsl/TN9aiNOwnBG\n"
"6gxwHCjotG23MqRgJ95MtCAApd/aWgzE1EPN2jpfm1NBHorWfKqNp1wLAoGBAJQU\n"
"88V9/1pytOrCL/JNcGQkAC0+utghuyeWiVSNCGDprPvNyuyW2riSSm4kwGs6A2FB\n"
"qcHgpvOsG9CpW6D9FbMpmFRVr+BNDSU8ypeZSEbH5TXoJTteOS0J75EtGvLbZBFA\n"
"c5rIrmx5w/RU6ZAnbgDH/08Jgo9KJoPK3bjTCzJPAoGARqIH0RezMX4qkb1xg/Cb\n"
"qz9lhSzAIN30IWDLZ2TqbSZ8dIOeCQcT/rbWRKrpx0ANbDELErvVZDMbTSVOIhQD\n"
"JFLd/j0tDlqZCwtPKNWdkzNHgALYWM8ByrBHCUzQOJ3jEnaDyOobaxnmUajhLbIy\n"
"I4tPGyBXqN2OMTNsjC0rkek=\n"
"-----END PRIVATE KEY-----\n";

struct quic_test_tls_pair
{
    SSL_CTX *client_context;
    SSL_CTX *server_context;
    SSL *client_ssl;
    SSL *server_ssl;

    quic_test_tls_pair() noexcept
    {
        this->client_context = ft_nullptr;
        this->server_context = ft_nullptr;
        this->client_ssl = ft_nullptr;
        this->server_ssl = ft_nullptr;
        return ;
    }

    ~quic_test_tls_pair() noexcept
    {
        return ;
    }
};

static X509 *quic_test_load_certificate(const char *pem_data)
{
    BIO *bio_handle;
    X509 *certificate;

    if (pem_data == ft_nullptr)
        return (ft_nullptr);
    bio_handle = BIO_new_mem_buf(pem_data, -1);
    if (bio_handle == ft_nullptr)
        return (ft_nullptr);
    certificate = PEM_read_bio_X509(bio_handle, ft_nullptr, ft_nullptr, ft_nullptr);
    BIO_free(bio_handle);
    return (certificate);
}

static EVP_PKEY *quic_test_load_key(const char *pem_data)
{
    BIO *bio_handle;
    EVP_PKEY *key_handle;

    if (pem_data == ft_nullptr)
        return (ft_nullptr);
    bio_handle = BIO_new_mem_buf(pem_data, -1);
    if (bio_handle == ft_nullptr)
        return (ft_nullptr);
    key_handle = PEM_read_bio_PrivateKey(bio_handle, ft_nullptr, ft_nullptr, ft_nullptr);
    BIO_free(bio_handle);
    return (key_handle);
}

static bool quic_test_perform_handshake(SSL *client_ssl, SSL *server_ssl)
{
    bool client_done;
    bool server_done;

    client_done = false;
    server_done = false;
    while (!client_done || !server_done)
    {
        int client_result;
        int server_result;

        client_result = SSL_do_handshake(client_ssl);
        if (client_result == 1)
            client_done = true;
        else
        {
            int client_error;

            client_error = SSL_get_error(client_ssl, client_result);
            if (client_error != SSL_ERROR_WANT_READ && client_error != SSL_ERROR_WANT_WRITE)
                return (false);
        }
        server_result = SSL_do_handshake(server_ssl);
        if (server_result == 1)
            server_done = true;
        else
        {
            int server_error;

            server_error = SSL_get_error(server_ssl, server_result);
            if (server_error != SSL_ERROR_WANT_READ && server_error != SSL_ERROR_WANT_WRITE)
                return (false);
        }
    }
    return (true);
}

static bool quic_test_setup_tls_pair(quic_test_tls_pair &pair)
{
    SSL_library_init();
    const SSL_METHOD *client_method;
    const SSL_METHOD *server_method;

    client_method = TLS_client_method();
    server_method = TLS_server_method();
    pair.client_context = SSL_CTX_new(client_method);
    pair.server_context = SSL_CTX_new(server_method);
    if (pair.client_context == ft_nullptr || pair.server_context == ft_nullptr)
        return (false);
    SSL_CTX_set_min_proto_version(pair.client_context, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(pair.client_context, TLS1_2_VERSION);
    SSL_CTX_set_min_proto_version(pair.server_context, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(pair.server_context, TLS1_2_VERSION);
    if (SSL_CTX_set_cipher_list(pair.client_context, "AES128-GCM-SHA256") != 1)
        return (false);
    if (SSL_CTX_set_cipher_list(pair.server_context, "AES128-GCM-SHA256") != 1)
        return (false);
    SSL_CTX_set_verify(pair.client_context, SSL_VERIFY_NONE, ft_nullptr);
    X509 *certificate;
    EVP_PKEY *key_handle;

    certificate = quic_test_load_certificate(g_quic_test_certificate);
    key_handle = quic_test_load_key(g_quic_test_key);
    if (certificate == ft_nullptr || key_handle == ft_nullptr)
        return (false);
    if (SSL_CTX_use_certificate(pair.server_context, certificate) != 1)
        return (false);
    if (SSL_CTX_use_PrivateKey(pair.server_context, key_handle) != 1)
        return (false);
    X509_free(certificate);
    EVP_PKEY_free(key_handle);
    pair.client_ssl = SSL_new(pair.client_context);
    pair.server_ssl = SSL_new(pair.server_context);
    if (pair.client_ssl == ft_nullptr || pair.server_ssl == ft_nullptr)
        return (false);
    BIO *client_bio;
    BIO *server_bio;

    client_bio = ft_nullptr;
    server_bio = ft_nullptr;
    if (BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1)
        return (false);
    SSL_set_bio(pair.client_ssl, client_bio, client_bio);
    SSL_set_bio(pair.server_ssl, server_bio, server_bio);
    SSL_set_connect_state(pair.client_ssl);
    SSL_set_accept_state(pair.server_ssl);
    if (!quic_test_perform_handshake(pair.client_ssl, pair.server_ssl))
        return (false);
    return (true);
}

static void quic_test_teardown_tls_pair(quic_test_tls_pair &pair)
{
    if (pair.client_ssl != ft_nullptr)
    {
        SSL_free(pair.client_ssl);
        pair.client_ssl = ft_nullptr;
    }
    if (pair.server_ssl != ft_nullptr)
    {
        SSL_free(pair.server_ssl);
        pair.server_ssl = ft_nullptr;
    }
    if (pair.client_context != ft_nullptr)
    {
        SSL_CTX_free(pair.client_context);
        pair.client_context = ft_nullptr;
    }
    if (pair.server_context != ft_nullptr)
    {
        SSL_CTX_free(pair.server_context);
        pair.server_context = ft_nullptr;
    }
    return ;
}

FT_TEST(test_networking_quic_feature_toggle)
{
    FT_ASSERT(networking_quic_disable_experimental());
    FT_ASSERT(!networking_quic_is_experimental_enabled());
    FT_ASSERT(networking_quic_enable_experimental());
    FT_ASSERT(networking_quic_is_experimental_enabled());
    FT_ASSERT(networking_quic_disable_experimental());
    FT_ASSERT(!networking_quic_is_experimental_enabled());
    return (1);
}

FT_TEST(test_networking_quic_configure_requires_feature)
{
    quic_test_tls_pair pair;

    FT_ASSERT(quic_test_setup_tls_pair(pair));
    FT_ASSERT(networking_quic_disable_experimental());
    quic_experimental_session session;
    quic_feature_configuration configuration;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT(!session.configure(pair.client_ssl, configuration, true));
    FT_ASSERT(networking_quic_enable_experimental());
    FT_ASSERT(session.configure(pair.client_ssl, configuration, true));
    quic_test_teardown_tls_pair(pair);
    FT_ASSERT(networking_quic_disable_experimental());
    return (1);
}

FT_TEST(test_networking_quic_encrypt_roundtrip)
{
    quic_test_tls_pair pair;
    int32_t cleanup_error;

    FT_ASSERT(quic_test_setup_tls_pair(pair));
    FT_ASSERT(networking_quic_enable_experimental());
    quic_experimental_session client_session;
    quic_experimental_session server_session;
    quic_feature_configuration client_configuration;
    quic_feature_configuration server_configuration;

    client_configuration.enable_datagram_pacing = true;
    server_configuration.enable_loss_recovery = true;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_session.initialize());
    FT_ASSERT(client_session.configure(pair.client_ssl, client_configuration, true));
    FT_ASSERT(server_session.configure(pair.server_ssl, server_configuration, false));
    quic_datagram_plaintext plaintext;

    static const unsigned char payload[] = {'h', 'e', 'l', 'l', 'o'};
    static const unsigned char associated_data[] = {0x01, 0x02, 0x03};
    plaintext.payload = payload;
    plaintext.payload_length = sizeof(payload);
    plaintext.associated_data = associated_data;
    plaintext.associated_data_length = sizeof(associated_data);
    ft_vector<unsigned char> ciphertext;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.initialize());

    FT_ASSERT(client_session.encrypt_datagram(plaintext, ciphertext));
    ft_vector<unsigned char> decrypted_payload;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, decrypted_payload.initialize());

    FT_ASSERT(server_session.decrypt_datagram(ciphertext, associated_data,
            sizeof(associated_data), decrypted_payload));
    FT_ASSERT_EQ(sizeof(payload), decrypted_payload.size());
    size_t index;

    index = 0;
    while (index < sizeof(payload))
    {
        FT_ASSERT_EQ(static_cast<int>(payload[index]),
            static_cast<int>(decrypted_payload[index]));
        index++;
    }
    unsigned char large_payload[64] = {0xA5};
    plaintext.payload = large_payload;
    plaintext.payload_length = sizeof(large_payload);
    ft_vector<unsigned char> allocation_output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, allocation_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const ft_bool encrypted_with_limit = client_session.encrypt_datagram(
        plaintext, allocation_output);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_FALSE, encrypted_with_limit);
    FT_ASSERT_EQ(0U, allocation_output.size());
    FT_ASSERT(client_session.encrypt_datagram(plaintext, ciphertext));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const ft_bool decrypted_with_limit = server_session.decrypt_datagram(
        ciphertext, associated_data, sizeof(associated_data), allocation_output);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_FALSE, decrypted_with_limit);
    FT_ASSERT_EQ(0U, allocation_output.size());
    FT_ASSERT(server_session.decrypt_datagram(ciphertext, associated_data,
        sizeof(associated_data), ciphertext));
    FT_ASSERT_EQ(sizeof(large_payload), ciphertext.size());
    FT_ASSERT_EQ(0, ft_memcmp(ciphertext.begin(), large_payload,
        sizeof(large_payload)));
    plaintext.payload = ft_nullptr;
    plaintext.payload_length = 0U;
    FT_ASSERT(client_session.encrypt_datagram(plaintext, ciphertext));
    FT_ASSERT(server_session.decrypt_datagram(ciphertext, associated_data,
        sizeof(associated_data), decrypted_payload));
    FT_ASSERT_EQ(0U, decrypted_payload.size());
    quic_feature_configuration echoed_configuration;

    FT_ASSERT(client_session.get_feature_configuration(echoed_configuration));
    FT_ASSERT(echoed_configuration.enable_datagram_pacing);
    FT_ASSERT(!echoed_configuration.enable_loss_recovery);
    cleanup_error = decrypted_payload.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    cleanup_error = ciphertext.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    cleanup_error = server_session.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    cleanup_error = client_session.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    quic_test_teardown_tls_pair(pair);
    FT_ASSERT(networking_quic_disable_experimental());
    return (1);
}

#ifdef LIBFT_TEST_BUILD
FT_TEST(test_networking_quic_nonce_xors_sequence_into_trailing_bytes)
{
    quic_experimental_session session;
    ft_vector<unsigned char> base_iv;
    unsigned char nonce[EVP_MAX_IV_LENGTH];
    const unsigned char expected[] = {
        0xA5, 0xA5, 0xA5, 0xA5, 0xA4, 0xA7,
        0xA6, 0xA1, 0xA0, 0xA3, 0xA2, 0xAD
    };

    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base_iv.initialize());
    base_iv.resize(sizeof(expected), 0xA5U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base_iv.get_error());
    FT_ASSERT_EQ(sizeof(expected), session.prepare_nonce(
        UINT64_C(0x0102030405060708), base_iv, nonce));
    FT_ASSERT_EQ(0, ft_memcmp(expected, nonce, sizeof(expected)));
    FT_ASSERT_EQ(sizeof(expected), session.prepare_nonce(0U, base_iv, nonce));
    FT_ASSERT_EQ(0, ft_memcmp(base_iv.begin(), nonce, sizeof(expected)));
    return (1);
}
#endif
#endif

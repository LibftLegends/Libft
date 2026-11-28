#include "../test_internal.hpp"
#include "../../Modules/Encryption/encryption.hpp"
#include "../../Modules/Networking/networking_tls_aead.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Networking/openssl_support.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#if NETWORKING_HAS_OPENSSL

#include <openssl/ssl.h>
#include <openssl/pem.h>

#ifndef LIBFT_TEST_BUILD
#endif

static const char g_aead_test_server_certificate[] =
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

static const char g_aead_test_server_key[] =
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

static X509 *load_certificate_from_string(const char *pem_string)
{
    BIO *bio_handle;
    X509 *certificate;

    if (pem_string == NULL)
        return (NULL);
    bio_handle = BIO_new_mem_buf(pem_string, -1);
    if (bio_handle == NULL)
        return (NULL);
    certificate = PEM_read_bio_X509(bio_handle, NULL, NULL, NULL);
    BIO_free(bio_handle);
    return (certificate);
}

static EVP_PKEY *load_key_from_string(const char *pem_string)
{
    BIO *bio_handle;
    EVP_PKEY *key_handle;

    if (pem_string == NULL)
        return (NULL);
    bio_handle = BIO_new_mem_buf(pem_string, -1);
    if (bio_handle == NULL)
        return (NULL);
    key_handle = PEM_read_bio_PrivateKey(bio_handle, NULL, NULL, NULL);
    BIO_free(bio_handle);
    return (key_handle);
}

static bool perform_tls_handshake(SSL *client_ssl, SSL *server_ssl)
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

FT_TEST(test_encryption_aead_tls_integration)
{
    SSL_library_init();
    const SSL_METHOD *client_method;
    const SSL_METHOD *server_method;
    SSL_CTX *client_ctx;
    SSL_CTX *server_ctx;
    X509 *server_certificate;
    EVP_PKEY *server_key;

    client_method = TLS_client_method();
    server_method = TLS_server_method();
    client_ctx = SSL_CTX_new(client_method);
    server_ctx = SSL_CTX_new(server_method);
    FT_ASSERT(client_ctx != NULL);
    FT_ASSERT(server_ctx != NULL);
    SSL_CTX_set_min_proto_version(client_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(client_ctx, TLS1_2_VERSION);
    SSL_CTX_set_min_proto_version(server_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(server_ctx, TLS1_2_VERSION);
    FT_ASSERT_EQ(1, SSL_CTX_set_cipher_list(client_ctx, "AES128-GCM-SHA256"));
    FT_ASSERT_EQ(1, SSL_CTX_set_cipher_list(server_ctx, "AES128-GCM-SHA256"));
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, NULL);
    server_certificate = load_certificate_from_string(g_aead_test_server_certificate);
    server_key = load_key_from_string(g_aead_test_server_key);
    FT_ASSERT(server_certificate != NULL);
    FT_ASSERT(server_key != NULL);
    FT_ASSERT_EQ(1, SSL_CTX_use_certificate(server_ctx, server_certificate));
    FT_ASSERT_EQ(1, SSL_CTX_use_PrivateKey(server_ctx, server_key));
    SSL *client_ssl;
    SSL *server_ssl;

    client_ssl = SSL_new(client_ctx);
    server_ssl = SSL_new(server_ctx);
    FT_ASSERT(client_ssl != NULL);
    FT_ASSERT(server_ssl != NULL);
    BIO *client_bio;
    BIO *server_bio;

    client_bio = NULL;
    server_bio = NULL;
    FT_ASSERT_EQ(1, BIO_new_bio_pair(&client_bio, 0, &server_bio, 0));
    SSL_set_bio(client_ssl, client_bio, client_bio);
    SSL_set_bio(server_ssl, server_bio, server_bio);
    SSL_set_connect_state(client_ssl);
    SSL_set_accept_state(server_ssl);
    FT_ASSERT(perform_tls_handshake(client_ssl, server_ssl));
    ft_vector<unsigned char> client_send_key;
    ft_vector<unsigned char> client_send_iv;
    ft_vector<unsigned char> client_receive_key;
    ft_vector<unsigned char> client_receive_iv;

    FT_ASSERT(networking_tls_export_aead_keys(client_ssl, true,
            client_send_key, client_send_iv, client_receive_key, client_receive_iv));
    ft_vector<unsigned char> server_send_key;
    ft_vector<unsigned char> server_send_iv;
    ft_vector<unsigned char> server_receive_key;
    ft_vector<unsigned char> server_receive_iv;

    FT_ASSERT(networking_tls_export_aead_keys(server_ssl, false,
            server_send_key, server_send_iv, server_receive_key, server_receive_iv));
    constexpr ft_size_t KEY_LENGTH = 16U;
    constexpr ft_size_t IV_LENGTH = 12U;
    constexpr ft_size_t DIRECTION_LENGTH = KEY_LENGTH + IV_LENGTH;
    constexpr char EXPORTER_LABEL[] = "EXPORTER-Libft-AEAD";
    unsigned char exported_material[2U * DIRECTION_LENGTH];
    FT_ASSERT_EQ(1, SSL_export_keying_material(client_ssl, exported_material,
        sizeof(exported_material), EXPORTER_LABEL, sizeof(EXPORTER_LABEL) - 1U,
        ft_nullptr, 0U, 0));
    FT_ASSERT_EQ(KEY_LENGTH, client_send_key.size());
    FT_ASSERT_EQ(IV_LENGTH, client_send_iv.size());
    FT_ASSERT_EQ(KEY_LENGTH, client_receive_key.size());
    FT_ASSERT_EQ(IV_LENGTH, client_receive_iv.size());
    FT_ASSERT_EQ(0, ft_memcmp(client_send_key.begin(), exported_material, KEY_LENGTH));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_iv.begin(),
        exported_material + KEY_LENGTH, IV_LENGTH));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_key.begin(),
        exported_material + DIRECTION_LENGTH, KEY_LENGTH));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_iv.begin(),
        exported_material + DIRECTION_LENGTH + KEY_LENGTH, IV_LENGTH));
    FT_ASSERT_EQ(KEY_LENGTH, server_send_key.size());
    FT_ASSERT_EQ(IV_LENGTH, server_send_iv.size());
    FT_ASSERT_EQ(0, ft_memcmp(server_send_key.begin(),
        client_receive_key.begin(), KEY_LENGTH));
    FT_ASSERT_EQ(0, ft_memcmp(server_send_iv.begin(),
        client_receive_iv.begin(), IV_LENGTH));
    FT_ASSERT_EQ(client_send_key.size(), server_receive_key.size());
    FT_ASSERT_EQ(client_send_iv.size(), server_receive_iv.size());
    size_t index;

    index = 0;
    while (index < client_send_key.size())
    {
        FT_ASSERT_EQ(static_cast<int>(client_send_key[index]),
            static_cast<int>(server_receive_key[index]));
        index++;
    }
    index = 0;
    while (index < client_send_iv.size())
    {
        FT_ASSERT_EQ(static_cast<int>(client_send_iv[index]),
            static_cast<int>(server_receive_iv[index]));
        index++;
    }
    encryption_aead_context client_send_context;
    encryption_aead_context client_receive_context;

    FT_ASSERT(networking_tls_initialize_aead_contexts(client_ssl, true,
            client_send_context, client_receive_context,
            client_send_iv, client_receive_iv));
    encryption_aead_context server_send_context;
    encryption_aead_context server_receive_context;

    FT_ASSERT(networking_tls_initialize_aead_contexts(server_ssl, false,
            server_send_context, server_receive_context,
            server_send_iv, server_receive_iv));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_iv.begin(),
        exported_material + KEY_LENGTH, IV_LENGTH));
    FT_ASSERT_EQ(0, ft_memcmp(server_send_iv.begin(),
        exported_material + DIRECTION_LENGTH + KEY_LENGTH, IV_LENGTH));
    unsigned char additional_data[2];

    additional_data[0] = 0x01;
    additional_data[1] = 0x02;
    unsigned char plaintext[5];

    plaintext[0] = 'h';
    plaintext[1] = 'e';
    plaintext[2] = 'l';
    plaintext[3] = 'l';
    plaintext[4] = 'o';
    unsigned char ciphertext[5];
    unsigned char decrypted[5];
    unsigned char authentication_tag[16];
    size_t cipher_length;
    size_t plain_length;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_send_context.update_aad(additional_data, 2));
    cipher_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_send_context.update(plaintext, 5, ciphertext, cipher_length));
    FT_ASSERT_EQ(static_cast<size_t>(5), cipher_length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_send_context.finalize(authentication_tag,
            sizeof(authentication_tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_receive_context.set_tag(authentication_tag, sizeof(authentication_tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_receive_context.update_aad(additional_data, 2));
    plain_length = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_receive_context.update(ciphertext, cipher_length,
            decrypted, plain_length));
    FT_ASSERT_EQ(cipher_length, plain_length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_receive_context.finalize(NULL, 0));
    index = 0;
    while (index < plain_length)
    {
        FT_ASSERT_EQ(static_cast<int>(plaintext[index]), static_cast<int>(decrypted[index]));
        index++;
    }
    int32_t ssl_error = SSL_ERROR_NONE;
    uint8_t incoming_byte = 0U;
    const uint8_t outgoing_byte = 0x6AU;
    FT_ASSERT_EQ(0, networking_ssl_read_with_status(server_ssl,
        &incoming_byte, sizeof(incoming_byte), ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_WANT_READ, ssl_error);
    FT_ASSERT_EQ(1, networking_ssl_write_with_status(client_ssl,
        &outgoing_byte, sizeof(outgoing_byte), ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_NONE, ssl_error);
    FT_ASSERT_EQ(1, networking_ssl_read_with_status(server_ssl,
        &incoming_byte, sizeof(incoming_byte), ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_NONE, ssl_error);
    FT_ASSERT_EQ(outgoing_byte, incoming_byte);
    FT_ASSERT_EQ(0, SSL_shutdown(client_ssl));
    FT_ASSERT_EQ(0, networking_ssl_read_with_status(server_ssl,
        &incoming_byte, sizeof(incoming_byte), ssl_error));
    FT_ASSERT_EQ(SSL_ERROR_ZERO_RETURN, ssl_error);
    SSL_free(client_ssl);
    SSL_free(server_ssl);
    SSL_CTX_free(client_ctx);
    SSL_CTX_free(server_ctx);
    X509_free(server_certificate);
    EVP_PKEY_free(server_key);
    return (1);
}

#endif

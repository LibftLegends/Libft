#include "networking_tls_aead.hpp"
#include "../Basic/basic.hpp"
#include "openssl_support.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#if NETWORKING_HAS_OPENSSL

#include <openssl/crypto.h>

struct networking_tls_aead_material
{
    unsigned char bytes[2U * (EVP_MAX_KEY_LENGTH + EVP_MAX_IV_LENGTH)];
    const unsigned char *send_key;
    const unsigned char *receive_key;
    ft_size_t key_length;
    ft_size_t iv_length;

    networking_tls_aead_material() noexcept;
    ~networking_tls_aead_material() noexcept;
    networking_tls_aead_material(const networking_tls_aead_material &other) = delete;
    networking_tls_aead_material &operator=(const networking_tls_aead_material &other) = delete;
};

networking_tls_aead_material::networking_tls_aead_material() noexcept
    : send_key(ft_nullptr), receive_key(ft_nullptr), key_length(0U), iv_length(0U)
{
    return ;
}

networking_tls_aead_material::~networking_tls_aead_material() noexcept
{
    OPENSSL_cleanse(this->bytes, sizeof(this->bytes));
    return ;
}

static const EVP_CIPHER *networking_tls_resolve_cipher(const SSL_CIPHER *cipher)
{
    if (cipher == NULL)
        return (NULL);
    int32_t cipher_nid;

    cipher_nid = SSL_CIPHER_get_cipher_nid(cipher);
    if (cipher_nid == NID_aes_128_gcm)
        return (EVP_aes_128_gcm());
    if (cipher_nid == NID_aes_256_gcm)
        return (EVP_aes_256_gcm());
#ifdef NID_aes_192_gcm
    if (cipher_nid == NID_aes_192_gcm)
        return (EVP_aes_192_gcm());
#endif
    return (NULL);
}

static ft_bool networking_tls_prepare_buffer(ft_vector<unsigned char> &buffer,
        ft_size_t length)
{
    int32_t initialize_error;

    if (buffer.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = buffer.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (FT_FALSE);
    }
    buffer.resize(length, 0);
    if (buffer.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool networking_tls_export_material(SSL *ssl_session, ft_bool outbound,
    networking_tls_aead_material &material) noexcept
{
    constexpr char LABEL[] = "EXPORTER-Libft-AEAD";

    if (ssl_session == ft_nullptr)
        return (FT_FALSE);
    const EVP_CIPHER *cipher = networking_tls_resolve_cipher(
        SSL_get_current_cipher(ssl_session));
    if (cipher == ft_nullptr)
        return (FT_FALSE);
    const int32_t key_length = EVP_CIPHER_key_length(cipher);
    const int32_t iv_length = EVP_CIPHER_iv_length(cipher);
    if (key_length <= 0 || key_length > EVP_MAX_KEY_LENGTH
        || iv_length <= 0 || iv_length > EVP_MAX_IV_LENGTH)
        return (FT_FALSE);
    material.key_length = static_cast<ft_size_t>(key_length);
    material.iv_length = static_cast<ft_size_t>(iv_length);
    const ft_size_t direction_length = material.key_length + material.iv_length;
    if (SSL_export_keying_material(ssl_session, material.bytes,
            FT_UINT64_TO_NATIVE_SIZE_CAST(2U * direction_length),
            LABEL, sizeof(LABEL) - 1U, ft_nullptr, 0, 0) != 1)
        return (FT_FALSE);
    if (outbound != FT_FALSE)
    {
        material.send_key = material.bytes;
        material.receive_key = material.bytes + direction_length;
    }
    else
    {
        material.send_key = material.bytes + direction_length;
        material.receive_key = material.bytes;
    }
    return (FT_TRUE);
}

ft_bool    networking_tls_export_aead_keys(SSL *ssl_session, ft_bool outbound,
        ft_vector<unsigned char> &send_key,
        ft_vector<unsigned char> &send_iv,
        ft_vector<unsigned char> &receive_key,
        ft_vector<unsigned char> &receive_iv)
{
    networking_tls_aead_material material;
    if (!networking_tls_export_material(ssl_session, outbound, material)
        || !networking_tls_prepare_buffer(send_key, material.key_length)
        || !networking_tls_prepare_buffer(send_iv, material.iv_length)
        || !networking_tls_prepare_buffer(receive_key, material.key_length)
        || !networking_tls_prepare_buffer(receive_iv, material.iv_length))
        return (FT_FALSE);
    ft_memcpy(send_key.begin(), material.send_key, material.key_length);
    ft_memcpy(receive_key.begin(), material.receive_key, material.key_length);
    ft_memcpy(send_iv.begin(), material.send_key + material.key_length,
        material.iv_length);
    ft_memcpy(receive_iv.begin(), material.receive_key + material.key_length,
        material.iv_length);
    return (FT_TRUE);
}

ft_bool    networking_tls_initialize_aead_contexts(SSL *ssl_session, ft_bool outbound,
        encryption_aead_context &send_context,
        encryption_aead_context &receive_context,
        ft_vector<unsigned char> &send_iv,
        ft_vector<unsigned char> &receive_iv)
{
    networking_tls_aead_material material;
    if (!networking_tls_export_material(ssl_session, outbound, material)
        || !networking_tls_prepare_buffer(send_iv, material.iv_length)
        || !networking_tls_prepare_buffer(receive_iv, material.iv_length))
        return (FT_FALSE);
    ft_memcpy(send_iv.begin(), material.send_key + material.key_length,
        material.iv_length);
    ft_memcpy(receive_iv.begin(), material.receive_key + material.key_length,
        material.iv_length);
    if (send_context.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (receive_context.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (send_context.initialize_encrypt(material.send_key, material.key_length,
            send_iv.begin(), material.iv_length) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (receive_context.initialize_decrypt(material.receive_key,
            material.key_length, receive_iv.begin(), material.iv_length) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

#endif

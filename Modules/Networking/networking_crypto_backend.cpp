#include "networking_crypto_backend.hpp"
#include "../Crypto/crypto_aead.hpp"
#include "../Crypto/crypto_primitives.hpp"
#include "../Crypto/crypto_random.hpp"
#include "../Crypto/crypto_session.hpp"
#include "../Errno/errno.hpp"
#include "../Crypto/crypto_x25519.hpp"

networking_crypto_backend::networking_crypto_backend() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _encryption_key()
{
    ft_memset(this->_encryption_key, 0, sizeof(this->_encryption_key));
    return ;
}

networking_crypto_backend::~networking_crypto_backend() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_crypto_backend::initialize(const uint8_t *key,
    ft_size_t key_length) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (key == ft_nullptr || key_length != sizeof(this->_encryption_key))
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memcpy(this->_encryption_key, key, sizeof(this->_encryption_key));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_crypto_backend::derive_session_keys(
    const uint8_t private_key[32], const uint8_t peer_public_key[32],
    const uint8_t *transcript, ft_size_t transcript_length,
    networking_crypto_role role, uint8_t send_key[32], uint8_t receive_key[32],
    uint8_t send_initialization_vector[12],
    uint8_t receive_initialization_vector[12]) noexcept
{
    crypto_session_role session_role;

    session_role = crypto_session_role::CLIENT;
    if (role == networking_crypto_role::SERVER)
        session_role = crypto_session_role::SERVER;
    else if (role != networking_crypto_role::CLIENT)
        return (FT_ERR_INVALID_ARGUMENT);
    return (crypto_derive_session_keys(private_key, peer_public_key,
        transcript, transcript_length, session_role, send_key, receive_key,
        send_initialization_vector, receive_initialization_vector));
}

int32_t networking_crypto_backend::derive_key_update(
    const uint8_t current_key[32], uint64_t next_epoch,
    uint8_t updated_key[32],
    uint8_t updated_initialization_vector[12]) noexcept
{
    return (crypto_derive_key_update(current_key, next_epoch, updated_key,
        updated_initialization_vector));
}

int32_t networking_crypto_backend::public_key(const uint8_t private_key[32],
    uint8_t public_key[32]) noexcept
{
    return (crypto_x25519_public_key(private_key, public_key));
}

int32_t networking_crypto_backend::sha256(const uint8_t *data,
    ft_size_t data_length, uint8_t digest[32]) const noexcept
{
    return (crypto_sha256_hash(data, data_length, digest));
}

int32_t networking_crypto_backend::hmac_sha256(const uint8_t *key,
    ft_size_t key_length, const uint8_t *data, ft_size_t data_length,
    uint8_t digest[32]) const noexcept
{
    return (crypto_hmac_sha256(key, key_length, data, data_length, digest));
}

int32_t networking_crypto_backend::random_bytes(uint8_t *output,
    ft_size_t length) noexcept
{
    return (crypto_random_bytes(output, length));
}

int32_t networking_crypto_backend::wipe(void *data, ft_size_t length) noexcept
{
    if (data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    (void)crypto_secure_wipe(data, length);
    return (FT_ERR_SUCCESS);
}

int32_t networking_crypto_backend::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    (void)crypto_secure_wipe(this->_encryption_key,
        sizeof(this->_encryption_key));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_crypto_backend::move(networking_crypto_backend &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    ft_memcpy(this->_encryption_key, other._encryption_key,
        sizeof(this->_encryption_key));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool networking_crypto_backend::seal(const uint8_t nonce[12],
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *plaintext, ft_size_t plaintext_length,
    ft_vector<uint8_t> &ciphertext, uint8_t authentication_tag[16]) noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    result = crypto_chacha20_poly1305_seal(this->_encryption_key, nonce,
        associated_data, associated_data_length, plaintext, plaintext_length,
        ciphertext, authentication_tag);
    if (result == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool networking_crypto_backend::open(const uint8_t nonce[12],
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *ciphertext, ft_size_t ciphertext_length,
    const uint8_t authentication_tag[16], ft_vector<uint8_t> &plaintext) noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    result = crypto_chacha20_poly1305_open(this->_encryption_key, nonce,
        associated_data, associated_data_length, ciphertext, ciphertext_length,
        authentication_tag, plaintext);
    if (result == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

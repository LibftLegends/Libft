#ifndef CRYPTO_NETWORK_BACKEND_HPP
#define CRYPTO_NETWORK_BACKEND_HPP

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

enum class crypto_network_role : uint8_t
{
    CLIENT = 0U,
    SERVER = 1U
};

/*
 * Libft-owned authenticated-encryption backend.  It deliberately exposes only
 * byte-oriented operations to the transport so a future reviewed backend can
 * replace the implementation without changing packet code.
 */
class crypto_network_backend
{
    private:
        uint8_t _initialised_state;
        uint8_t _encryption_key[32];

    public:
        crypto_network_backend() noexcept;
        crypto_network_backend(const crypto_network_backend &other) noexcept = delete;
        crypto_network_backend(crypto_network_backend &&other) noexcept = delete;
        ~crypto_network_backend() noexcept;

        crypto_network_backend &operator=(const crypto_network_backend &other) noexcept = delete;
        crypto_network_backend &operator=(crypto_network_backend &&other) noexcept = delete;

        int32_t initialize(const uint8_t *key, ft_size_t key_length) noexcept;
        int32_t derive_session_keys(const uint8_t private_key[32],
            const uint8_t peer_public_key[32], const uint8_t *transcript,
            ft_size_t transcript_length, crypto_network_role role,
            uint8_t send_key[32], uint8_t receive_key[32],
            uint8_t send_initialization_vector[12],
            uint8_t receive_initialization_vector[12]) noexcept;
        int32_t derive_key_update(const uint8_t current_key[32],
            uint64_t next_epoch, uint8_t updated_key[32],
            uint8_t updated_initialization_vector[12]) noexcept;
        int32_t public_key(const uint8_t private_key[32],
            uint8_t public_key[32]) noexcept;
        int32_t sha256(const uint8_t *data, ft_size_t data_length,
            uint8_t digest[32]) const noexcept;
        int32_t hmac_sha256(const uint8_t *key, ft_size_t key_length,
            const uint8_t *data, ft_size_t data_length,
            uint8_t digest[32]) const noexcept;
        int32_t random_bytes(uint8_t *output, ft_size_t length) noexcept;
        int32_t wipe(void *data, ft_size_t length) noexcept;
        int32_t destroy() noexcept;
        int32_t move(crypto_network_backend &other) noexcept;
        int32_t swap(crypto_network_backend &other) noexcept;
        ft_bool seal(const uint8_t nonce[12], const uint8_t *associated_data,
            ft_size_t associated_data_length, const uint8_t *plaintext,
            ft_size_t plaintext_length, ft_vector<uint8_t> &ciphertext,
            uint8_t authentication_tag[16]) noexcept;
        ft_bool open(const uint8_t nonce[12], const uint8_t *associated_data,
            ft_size_t associated_data_length, const uint8_t *ciphertext,
            ft_size_t ciphertext_length, const uint8_t authentication_tag[16],
            ft_vector<uint8_t> &plaintext) noexcept;
};

#endif



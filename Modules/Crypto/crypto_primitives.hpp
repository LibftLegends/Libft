#ifndef CRYPTO_PRIMITIVES_HPP
#define CRYPTO_PRIMITIVES_HPP

#include "../Basic/basic.hpp"
#include <cstdint>

int32_t crypto_secure_wipe(void *buffer, ft_size_t buffer_size) noexcept;

class crypto_sha256
{
    private:
        uint8_t _initialised_state;
        uint32_t _hash[8];
        uint8_t _buffer[64];
        ft_size_t _buffer_size;
        uint64_t _message_length_bytes;

        void process_block(const uint8_t block[64]) noexcept;

    public:
        crypto_sha256() noexcept;
        crypto_sha256(const crypto_sha256 &other) noexcept = delete;
        crypto_sha256(crypto_sha256 &&other) noexcept = delete;
        ~crypto_sha256() noexcept;

        crypto_sha256 &operator=(const crypto_sha256 &other) noexcept = delete;
        crypto_sha256 &operator=(crypto_sha256 &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(crypto_sha256 &other) noexcept;
        int32_t update(const void *data, ft_size_t length) noexcept;
        int32_t final(uint8_t digest[32]) noexcept;
};

int32_t crypto_sha256_hash(const void *data, ft_size_t length,
    uint8_t digest[32]) noexcept;

int32_t crypto_hmac_sha256(const uint8_t *key, ft_size_t key_length,
    const void *data, ft_size_t data_length, uint8_t digest[32]) noexcept;

int32_t crypto_hkdf_sha256_extract(const uint8_t *salt, ft_size_t salt_length,
    const uint8_t *input_key_material, ft_size_t input_key_material_length,
    uint8_t pseudorandom_key[32]) noexcept;

int32_t crypto_hkdf_sha256_expand(const uint8_t pseudorandom_key[32],
    const uint8_t *info, ft_size_t info_length, uint8_t *output,
    ft_size_t output_length) noexcept;

#endif

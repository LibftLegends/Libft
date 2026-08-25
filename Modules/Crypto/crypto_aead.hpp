#ifndef CRYPTO_AEAD_HPP
#define CRYPTO_AEAD_HPP

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

int32_t crypto_chacha20_poly1305_seal(const uint8_t key[32],
    const uint8_t nonce[12], const uint8_t *associated_data,
    ft_size_t associated_data_length, const uint8_t *plaintext,
    ft_size_t plaintext_length, ft_vector<uint8_t> &ciphertext,
    uint8_t authentication_tag[16]) noexcept;

int32_t crypto_chacha20_poly1305_open(const uint8_t key[32],
    const uint8_t nonce[12], const uint8_t *associated_data,
    ft_size_t associated_data_length, const uint8_t *ciphertext,
    ft_size_t ciphertext_length, const uint8_t authentication_tag[16],
    ft_vector<uint8_t> &plaintext) noexcept;

#endif

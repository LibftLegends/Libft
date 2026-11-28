#ifndef CRYPTO_POLY1305_HPP
#define CRYPTO_POLY1305_HPP

#include "../Basic/basic.hpp"
#include <cstdint>

int32_t crypto_poly1305_auth(uint8_t tag[16], const uint8_t *message,
    ft_size_t length, const uint8_t key[32]) noexcept;

// Authenticates the RFC 8439 AEAD layout with each span independently padded.
int32_t crypto_poly1305_auth_aead(uint8_t tag[16], const uint8_t key[32],
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *ciphertext, ft_size_t ciphertext_length) noexcept;

#endif

#ifndef CRYPTO_POLY1305_HPP
#define CRYPTO_POLY1305_HPP

#include "../Basic/basic.hpp"
#include <cstdint>

int32_t crypto_poly1305_auth(uint8_t tag[16], const uint8_t *message,
    ft_size_t length, const uint8_t key[32]) noexcept;

#endif

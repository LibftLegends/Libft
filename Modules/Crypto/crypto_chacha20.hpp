#ifndef CRYPTO_CHACHA20_HPP
#define CRYPTO_CHACHA20_HPP

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

void crypto_chacha20_block(const uint8_t key[32], uint32_t counter,
    const uint8_t nonce[12], uint8_t output[64]) noexcept;

int32_t crypto_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
    uint32_t counter, const uint8_t *input, ft_size_t length,
    ft_vector<uint8_t> &output) noexcept;

#endif

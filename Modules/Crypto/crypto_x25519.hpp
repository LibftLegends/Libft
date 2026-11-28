#ifndef CRYPTO_X25519_HPP
#define CRYPTO_X25519_HPP

#include "../Basic/basic.hpp"
#include <cstdint>

int32_t crypto_x25519_public_key(const uint8_t private_key[32],
    uint8_t public_key[32]) noexcept;

int32_t crypto_x25519_shared_secret(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], uint8_t shared_secret[32]) noexcept;

#ifdef LIBFT_TEST_BUILD
void crypto_x25519_test_reduce_limbs(uint32_t output[16], const uint64_t limbs[16]) noexcept;
#endif

#endif

#ifndef CRYPTO_CHACHA20_HPP
#define CRYPTO_CHACHA20_HPP

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

void crypto_chacha20_block(const uint8_t key[32], uint32_t counter,
    const uint8_t nonce[12], uint8_t output[64]) noexcept;

#ifdef LIBFT_TEST_BUILD
void crypto_chacha20_test_quarter_round(uint32_t state[16], uint32_t index_a,
    uint32_t index_b, uint32_t index_c, uint32_t index_d) noexcept;
#endif

/* Uses the requested RFC block counter, including zero. Returns
 * FT_ERR_OUT_OF_RANGE if the request advances past UINT32_MAX. */
int32_t crypto_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
    uint32_t counter, const uint8_t *input, ft_size_t length,
    ft_vector<uint8_t> &output) noexcept;

#endif

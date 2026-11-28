#ifndef CRYPTO_PRIMITIVES_INTERNAL_HPP
#define CRYPTO_PRIMITIVES_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a Libft internal header."
#endif

#include "crypto_primitives.hpp"

struct crypto_hkdf_sha256_label
{
    const uint8_t *info;
    ft_size_t info_length;
    uint8_t *output;
    ft_size_t output_length;
};

// Each label starts a separate HKDF expansion and emits at most one block.
int32_t crypto_hkdf_sha256_expand_labels(const uint8_t pseudorandom_key[32],
    const crypto_hkdf_sha256_label *labels, ft_size_t label_count) noexcept;

#endif

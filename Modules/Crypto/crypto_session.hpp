#ifndef CRYPTO_SESSION_HPP
#define CRYPTO_SESSION_HPP

#include "../Basic/basic.hpp"
#include <cstdint>

enum class crypto_session_role : uint8_t
{
    CLIENT = 0U,
    SERVER = 1U
};

/* transcript is a canonical byte sequence with explicit field lengths.
 * Output buffers must be disjoint from each other and all input ranges;
 * aliasing is rejected before any output buffer is changed. */
int32_t crypto_derive_session_keys(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], const uint8_t *transcript,
    ft_size_t transcript_length, crypto_session_role role,
    uint8_t send_key[32], uint8_t receive_key[32],
    uint8_t send_initialization_vector[12],
    uint8_t receive_initialization_vector[12]) noexcept;

/* Updated outputs are disjoint from each other and current_key. Overlap is
 * rejected without changing any caller buffer. */
int32_t crypto_derive_key_update(const uint8_t current_key[32],
    uint64_t next_epoch, uint8_t updated_key[32],
    uint8_t updated_initialization_vector[12]) noexcept;

#endif

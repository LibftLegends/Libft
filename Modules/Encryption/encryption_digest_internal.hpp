#ifndef ENCRYPTION_DIGEST_INTERNAL_HPP
# define ENCRYPTION_DIGEST_INTERNAL_HPP

# include "../Basic/basic.hpp"

template <ft_size_t HashWords>
struct encryption_digest_state
{
    uint32_t hash[HashWords];
    uint8_t pending[64];
    ft_size_t pending_length;
    uint64_t message_length;
};

using sha1_digest_state = encryption_digest_state<5>;
using sha256_digest_state = encryption_digest_state<8>;

void sha1_digest_initialize(sha1_digest_state &state) noexcept;
void sha1_digest_update(sha1_digest_state &state, const uint8_t *data, ft_size_t length) noexcept;
void sha1_digest_final(sha1_digest_state &state, uint8_t *digest) noexcept;
void sha256_digest_initialize(sha256_digest_state &state) noexcept;
void sha256_digest_update(sha256_digest_state &state, const uint8_t *data, ft_size_t length) noexcept;
void sha256_digest_final(sha256_digest_state &state, uint8_t *digest) noexcept;

template <ft_size_t HashWords, void (*ProcessBlock)(uint32_t *, const uint8_t *) noexcept>
void encryption_digest_update(encryption_digest_state<HashWords> &state,
    const uint8_t *data, ft_size_t length) noexcept
{
    if (length == 0)
        return ;
    constexpr ft_size_t BLOCK_SIZE = sizeof(state.pending);
    state.message_length += length;
    if (state.pending_length != 0)
    {
        ft_size_t copy_size = BLOCK_SIZE - state.pending_length;
        if (copy_size > length)
            copy_size = length;
        ft_memcpy(state.pending + state.pending_length, data, copy_size);
        state.pending_length += copy_size;
        length -= copy_size;
        data += copy_size;
        if (state.pending_length != BLOCK_SIZE)
            return ;
        ProcessBlock(state.hash, state.pending);
        state.pending_length = 0;
    }
    while (length >= BLOCK_SIZE)
    {
        ProcessBlock(state.hash, data);
        length -= BLOCK_SIZE;
        data += BLOCK_SIZE;
    }
    if (length != 0)
    {
        ft_memcpy(state.pending, data, length);
        state.pending_length = length;
    }
    return ;
}

template <ft_size_t HashWords, void (*ProcessBlock)(uint32_t *, const uint8_t *) noexcept>
void encryption_digest_final(encryption_digest_state<HashWords> &state,
    uint8_t *digest) noexcept
{
    constexpr ft_size_t BLOCK_SIZE = sizeof(state.pending);
    constexpr ft_size_t LENGTH_OFFSET = BLOCK_SIZE - sizeof(uint64_t);
    ft_size_t offset = state.pending_length;
    state.pending[offset++] = 0x80;
    if (offset > LENGTH_OFFSET)
    {
        ft_memset(state.pending + offset, 0, BLOCK_SIZE - offset);
        ProcessBlock(state.hash, state.pending);
        offset = 0;
    }
    ft_memset(state.pending + offset, 0, LENGTH_OFFSET - offset);
    uint64_t bit_length = state.message_length * 8U;
    offset = BLOCK_SIZE;
    while (offset > LENGTH_OFFSET)
    {
        state.pending[--offset] = static_cast<uint8_t>(bit_length);
        bit_length >>= 8;
    }
    ProcessBlock(state.hash, state.pending);
    ft_size_t index = 0;
    while (index < HashWords)
    {
        const uint32_t word = state.hash[index];
        digest[index * 4] = static_cast<uint8_t>(word >> 24);
        digest[index * 4 + 1] = static_cast<uint8_t>(word >> 16);
        digest[index * 4 + 2] = static_cast<uint8_t>(word >> 8);
        digest[index * 4 + 3] = static_cast<uint8_t>(word);
        ++index;
    }
    return ;
}

#endif

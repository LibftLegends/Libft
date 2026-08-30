#include "crypto_chacha20.hpp"
#include "../Errno/errno.hpp"

namespace
{
    static uint32_t load_u32_le(const uint8_t *data) noexcept
    {
        return (static_cast<uint32_t>(data[0])
            | (static_cast<uint32_t>(data[1]) << 8U)
            | (static_cast<uint32_t>(data[2]) << 16U)
            | (static_cast<uint32_t>(data[3]) << 24U));
    }

    static void store_u32_le(uint8_t *data, uint32_t value) noexcept
    {
        data[0] = static_cast<uint8_t>(value);
        data[1] = static_cast<uint8_t>(value >> 8U);
        data[2] = static_cast<uint8_t>(value >> 16U);
        data[3] = static_cast<uint8_t>(value >> 24U);
        return ;
    }

    static uint32_t rotate_left(uint32_t value, uint32_t bits) noexcept
    {
        return ((value << bits) | (value >> (32U - bits)));
    }

    static void quarter_round(uint32_t state[16], uint32_t a, uint32_t b,
        uint32_t c, uint32_t d) noexcept
    {
        state[a] += state[b];
        state[d] = rotate_left(state[d] ^ state[a], 16U);
        state[c] += state[d];
        state[b] = rotate_left(state[b] ^ state[c], 12U);
        state[a] += state[b];
        state[d] = rotate_left(state[d] ^ state[a], 8U);
        state[c] += state[d];
        state[b] = rotate_left(state[b] ^ state[c], 7U);
        return ;
    }
}

void crypto_chacha20_block(const uint8_t key[32], uint32_t counter,
    const uint8_t nonce[12], uint8_t output[64]) noexcept
{
    static const uint32_t constants[4] = {
        0x61707865U, 0x3320646eU, 0x79622d32U, 0x6b206574U
    };
    uint32_t state[16];
    uint32_t working[16];
    uint32_t index;
    uint32_t round;

    if (key == ft_nullptr || nonce == ft_nullptr || output == ft_nullptr)
        return ;
    index = 0U;
    while (index < 4U)
    {
        state[index] = constants[index];
        index += 1U;
    }
    index = 0U;
    while (index < 8U)
    {
        state[index + 4U] = load_u32_le(key + index * 4U);
        index += 1U;
    }
    state[12] = counter;
    state[13] = load_u32_le(nonce);
    state[14] = load_u32_le(nonce + 4U);
    state[15] = load_u32_le(nonce + 8U);
    ft_memcpy(working, state, sizeof(working));
    round = 0U;
    while (round < 10U)
    {
        quarter_round(working, 0U, 4U, 8U, 12U);
        quarter_round(working, 1U, 5U, 9U, 13U);
        quarter_round(working, 2U, 6U, 10U, 14U);
        quarter_round(working, 3U, 7U, 11U, 15U);
        quarter_round(working, 0U, 5U, 10U, 15U);
        quarter_round(working, 1U, 6U, 11U, 12U);
        quarter_round(working, 2U, 7U, 8U, 13U);
        quarter_round(working, 3U, 4U, 9U, 14U);
        round += 1U;
    }
    index = 0U;
    while (index < 16U)
    {
        store_u32_le(output + index * 4U, working[index] + state[index]);
        index += 1U;
    }
    ft_memset(state, 0, sizeof(state));
    ft_memset(working, 0, sizeof(working));
    return ;
}

int32_t crypto_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
    uint32_t counter, const uint8_t *input, ft_size_t length,
    ft_vector<uint8_t> &output) noexcept
{
    ft_vector<uint8_t> staging;
    uint8_t block[64];
    ft_size_t offset;
    ft_size_t block_index;
    int32_t result;

    if (key == ft_nullptr || nonce == ft_nullptr
        || (input == ft_nullptr && length != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (counter == 0U && length != 0U)
        return (FT_ERR_OUT_OF_RANGE);
    if (output.is_initialised() != FT_CLASS_STATE_INITIALISED
        && output.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    result = staging.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        output.clear();
        return (result);
    }
    staging.resize(length);
    if (staging.get_error() != FT_ERR_SUCCESS)
    {
        result = staging.get_error();
        (void)staging.destroy();
        output.clear();
        return (result);
    }
    offset = 0U;
    while (offset < length)
    {
        crypto_chacha20_block(key, counter, nonce, block);
        block_index = 0U;
        while (block_index < sizeof(block) && offset + block_index < length)
        {
            staging[offset + block_index] = static_cast<uint8_t>(
                input[offset + block_index] ^ block[block_index]);
            block_index += 1U;
        }
        offset += block_index;
        if (offset < length)
        {
            if (counter == 0xffffffffU)
            {
                (void)staging.destroy();
                output.clear();
                return (FT_ERR_OUT_OF_RANGE);
            }
            counter += 1U;
        }
    }
    ft_memset(block, 0, sizeof(block));
    result = output.move(staging);
    if (result != FT_ERR_SUCCESS)
    {
        (void)staging.destroy();
        output.clear();
    }
    return (result);
}

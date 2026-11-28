#include "crypto_poly1305.hpp"
#include "crypto_primitives.hpp"
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

    static void store_u64_le(uint8_t *data, uint64_t value) noexcept
    {
        store_u32_le(data, static_cast<uint32_t>(value));
        store_u32_le(data + 4U, static_cast<uint32_t>(value >> 32U));
        return ;
    }
}

struct crypto_poly1305_span
{
    const uint8_t *data;
    ft_size_t length;
};

template <ft_bool PadSpans, ft_size_t SpanCount>
static int32_t crypto_poly1305_auth_spans(uint8_t tag[16], const uint8_t key[32],
    const crypto_poly1305_span (&spans)[SpanCount]) noexcept
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t h0;
    uint32_t h1;
    uint32_t h2;
    uint32_t h3;
    uint32_t h4;
    uint32_t pad0;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
    uint8_t block[16];
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint64_t d0;
    uint64_t d1;
    uint64_t d2;
    uint64_t d3;
    uint64_t d4;
    uint32_t carry;
    uint32_t mask;
    uint32_t g0;
    uint32_t g1;
    uint32_t g2;
    uint32_t g3;
    uint32_t g4;
    uint32_t f0;
    uint32_t f1;
    uint32_t f2;
    uint32_t f3;
    uint64_t sum;

    r0 = load_u32_le(key) & 0x3ffffffU;
    r1 = (load_u32_le(key + 3U) >> 2U) & 0x3ffff03U;
    r2 = (load_u32_le(key + 6U) >> 4U) & 0x3ffc0ffU;
    r3 = (load_u32_le(key + 9U) >> 6U) & 0x3f03fffU;
    r4 = (load_u32_le(key + 12U) >> 8U) & 0x00fffffU;
    h0 = 0U;
    h1 = 0U;
    h2 = 0U;
    h3 = 0U;
    h4 = 0U;
    pad0 = load_u32_le(key + 16U);
    pad1 = load_u32_le(key + 20U);
    pad2 = load_u32_le(key + 24U);
    pad3 = load_u32_le(key + 28U);
    ft_size_t span_index = 0U;
    while (span_index < SpanCount)
    {
        const uint8_t *message = spans[span_index].data;
        ft_size_t length = spans[span_index].length;
        while (length != 0U)
        {
            const uint8_t *block_input = message;
            uint32_t high_bit = 1U << 24U;
            if (length < sizeof(block))
            {
                ft_memset(block, 0, sizeof(block));
                ft_memcpy(block, message, length);
                if constexpr (PadSpans == FT_FALSE)
                {
                    block[length] = 1U;
                    high_bit = 0U;
                }
                block_input = block;
            }
            t0 = load_u32_le(block_input) & 0x3ffffffU;
            t1 = (load_u32_le(block_input + 3U) >> 2U) & 0x3ffffffU;
            t2 = (load_u32_le(block_input + 6U) >> 4U) & 0x3ffffffU;
            t3 = (load_u32_le(block_input + 9U) >> 6U) & 0x3ffffffU;
            t4 = (load_u32_le(block_input + 12U) >> 8U) | high_bit;
            h0 += t0;
            h1 += t1;
            h2 += t2;
            h3 += t3;
            h4 += t4;
            d0 = static_cast<uint64_t>(h0) * r0
                + static_cast<uint64_t>(h1) * 5U * r4
                + static_cast<uint64_t>(h2) * 5U * r3
                + static_cast<uint64_t>(h3) * 5U * r2
                + static_cast<uint64_t>(h4) * 5U * r1;
            d1 = static_cast<uint64_t>(h0) * r1
                + static_cast<uint64_t>(h1) * r0
                + static_cast<uint64_t>(h2) * 5U * r4
                + static_cast<uint64_t>(h3) * 5U * r3
                + static_cast<uint64_t>(h4) * 5U * r2;
            d2 = static_cast<uint64_t>(h0) * r2
                + static_cast<uint64_t>(h1) * r1
                + static_cast<uint64_t>(h2) * r0
                + static_cast<uint64_t>(h3) * 5U * r4
                + static_cast<uint64_t>(h4) * 5U * r3;
            d3 = static_cast<uint64_t>(h0) * r3
                + static_cast<uint64_t>(h1) * r2
                + static_cast<uint64_t>(h2) * r1
                + static_cast<uint64_t>(h3) * r0
                + static_cast<uint64_t>(h4) * 5U * r4;
            d4 = static_cast<uint64_t>(h0) * r4
                + static_cast<uint64_t>(h1) * r3
                + static_cast<uint64_t>(h2) * r2
                + static_cast<uint64_t>(h3) * r1
                + static_cast<uint64_t>(h4) * r0;
            carry = static_cast<uint32_t>(d0 >> 26U);
            h0 = static_cast<uint32_t>(d0) & 0x3ffffffU;
            d1 += carry;
            carry = static_cast<uint32_t>(d1 >> 26U);
            h1 = static_cast<uint32_t>(d1) & 0x3ffffffU;
            d2 += carry;
            carry = static_cast<uint32_t>(d2 >> 26U);
            h2 = static_cast<uint32_t>(d2) & 0x3ffffffU;
            d3 += carry;
            carry = static_cast<uint32_t>(d3 >> 26U);
            h3 = static_cast<uint32_t>(d3) & 0x3ffffffU;
            d4 += carry;
            carry = static_cast<uint32_t>(d4 >> 26U);
            h4 = static_cast<uint32_t>(d4) & 0x3ffffffU;
            h0 += carry * 5U;
            carry = h0 >> 26U;
            h0 &= 0x3ffffffU;
            h1 += carry;
            if (length < sizeof(block))
                break ;
            message += sizeof(block);
            length -= sizeof(block);
        }
        span_index += 1U;
    }
    carry = h1 >> 26U;
    h1 &= 0x3ffffffU;
    h2 += carry;
    carry = h2 >> 26U;
    h2 &= 0x3ffffffU;
    h3 += carry;
    carry = h3 >> 26U;
    h3 &= 0x3ffffffU;
    h4 += carry;
    carry = h4 >> 26U;
    h4 &= 0x3ffffffU;
    h0 += carry * 5U;
    carry = h0 >> 26U;
    h0 &= 0x3ffffffU;
    h1 += carry;
    g0 = h0 + 5U;
    carry = g0 >> 26U;
    g0 &= 0x3ffffffU;
    g1 = h1 + carry;
    carry = g1 >> 26U;
    g1 &= 0x3ffffffU;
    g2 = h2 + carry;
    carry = g2 >> 26U;
    g2 &= 0x3ffffffU;
    g3 = h3 + carry;
    carry = g3 >> 26U;
    g3 &= 0x3ffffffU;
    g4 = h4 + carry - (1U << 26U);
    mask = (g4 >> 31U) - 1U;
    g0 = (g0 & mask) | (h0 & ~mask);
    g1 = (g1 & mask) | (h1 & ~mask);
    g2 = (g2 & mask) | (h2 & ~mask);
    g3 = (g3 & mask) | (h3 & ~mask);
    g4 = (g4 & mask) | (h4 & ~mask);
    sum = static_cast<uint64_t>(g0 | (g1 << 26U)) + pad0;
    f0 = static_cast<uint32_t>(sum);
    carry = static_cast<uint32_t>(sum >> 32U);
    sum = static_cast<uint64_t>((g1 >> 6U) | (g2 << 20U))
        + pad1 + carry;
    f1 = static_cast<uint32_t>(sum);
    carry = static_cast<uint32_t>(sum >> 32U);
    sum = static_cast<uint64_t>((g2 >> 12U) | (g3 << 14U))
        + pad2 + carry;
    f2 = static_cast<uint32_t>(sum);
    carry = static_cast<uint32_t>(sum >> 32U);
    sum = static_cast<uint64_t>((g3 >> 18U) | (g4 << 8U))
        + pad3 + carry;
    f3 = static_cast<uint32_t>(sum);
    store_u32_le(tag, f0);
    store_u32_le(tag + 4U, f1);
    store_u32_le(tag + 8U, f2);
    store_u32_le(tag + 12U, f3);
    (void)crypto_secure_wipe(block, sizeof(block));
    (void)crypto_secure_wipe(&r0, sizeof(r0));
    (void)crypto_secure_wipe(&r1, sizeof(r1));
    (void)crypto_secure_wipe(&r2, sizeof(r2));
    (void)crypto_secure_wipe(&r3, sizeof(r3));
    (void)crypto_secure_wipe(&r4, sizeof(r4));
    (void)crypto_secure_wipe(&h0, sizeof(h0));
    (void)crypto_secure_wipe(&h1, sizeof(h1));
    (void)crypto_secure_wipe(&h2, sizeof(h2));
    (void)crypto_secure_wipe(&h3, sizeof(h3));
    (void)crypto_secure_wipe(&h4, sizeof(h4));
    (void)crypto_secure_wipe(&pad0, sizeof(pad0));
    (void)crypto_secure_wipe(&pad1, sizeof(pad1));
    (void)crypto_secure_wipe(&pad2, sizeof(pad2));
    (void)crypto_secure_wipe(&pad3, sizeof(pad3));
    (void)crypto_secure_wipe(&t0, sizeof(t0));
    (void)crypto_secure_wipe(&t1, sizeof(t1));
    (void)crypto_secure_wipe(&t2, sizeof(t2));
    (void)crypto_secure_wipe(&t3, sizeof(t3));
    (void)crypto_secure_wipe(&t4, sizeof(t4));
    (void)crypto_secure_wipe(&d0, sizeof(d0));
    (void)crypto_secure_wipe(&d1, sizeof(d1));
    (void)crypto_secure_wipe(&d2, sizeof(d2));
    (void)crypto_secure_wipe(&d3, sizeof(d3));
    (void)crypto_secure_wipe(&d4, sizeof(d4));
    (void)crypto_secure_wipe(&carry, sizeof(carry));
    (void)crypto_secure_wipe(&mask, sizeof(mask));
    (void)crypto_secure_wipe(&g0, sizeof(g0));
    (void)crypto_secure_wipe(&g1, sizeof(g1));
    (void)crypto_secure_wipe(&g2, sizeof(g2));
    (void)crypto_secure_wipe(&g3, sizeof(g3));
    (void)crypto_secure_wipe(&g4, sizeof(g4));
    (void)crypto_secure_wipe(&f0, sizeof(f0));
    (void)crypto_secure_wipe(&f1, sizeof(f1));
    (void)crypto_secure_wipe(&f2, sizeof(f2));
    (void)crypto_secure_wipe(&f3, sizeof(f3));
    (void)crypto_secure_wipe(&sum, sizeof(sum));
    return (FT_ERR_SUCCESS);
}

int32_t crypto_poly1305_auth(uint8_t tag[16], const uint8_t *message,
    ft_size_t length, const uint8_t key[32]) noexcept
{
    if (tag == ft_nullptr || key == ft_nullptr
        || (message == ft_nullptr && length != 0U))
    {
        if (tag != ft_nullptr)
            (void)crypto_secure_wipe(tag, 16U);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    const crypto_poly1305_span spans[] = {{message, length}};
    return (crypto_poly1305_auth_spans<FT_FALSE>(tag, key, spans));
}

int32_t crypto_poly1305_auth_aead(uint8_t tag[16], const uint8_t key[32],
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *ciphertext, ft_size_t ciphertext_length) noexcept
{
    if (tag == ft_nullptr || key == ft_nullptr
        || (associated_data == ft_nullptr && associated_data_length != 0U)
        || (ciphertext == ft_nullptr && ciphertext_length != 0U))
    {
        if (tag != ft_nullptr)
            (void)crypto_secure_wipe(tag, 16U);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    uint8_t lengths[16];
    ft_size_t padded_associated_length;
    ft_size_t padded_ciphertext_length;
    ft_size_t lengths_offset;
    ft_size_t total_length;
    if (ft_align_up_checked(associated_data_length, 16U,
            &padded_associated_length) != FT_ERR_SUCCESS
        || ft_align_up_checked(ciphertext_length, 16U,
            &padded_ciphertext_length) != FT_ERR_SUCCESS
        || ft_size_add_checked(padded_associated_length, padded_ciphertext_length,
            &lengths_offset) != FT_ERR_SUCCESS
        || ft_size_add_checked(lengths_offset, sizeof(lengths), &total_length)
            != FT_ERR_SUCCESS)
    {
        (void)crypto_secure_wipe(tag, 16U);
        return (FT_ERR_OUT_OF_RANGE);
    }
    store_u64_le(lengths, associated_data_length);
    store_u64_le(lengths + 8U, ciphertext_length);
    const crypto_poly1305_span spans[] = {
        {associated_data, associated_data_length},
        {ciphertext, ciphertext_length},
        {lengths, sizeof(lengths)}
    };
    return (crypto_poly1305_auth_spans<FT_TRUE>(tag, key, spans));
}

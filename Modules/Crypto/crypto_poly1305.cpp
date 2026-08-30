#include "crypto_poly1305.hpp"
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
}

int32_t crypto_poly1305_auth(uint8_t tag[16], const uint8_t *message,
    ft_size_t length, const uint8_t key[32]) noexcept
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

    if (tag == ft_nullptr || key == ft_nullptr
        || (message == ft_nullptr && length != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
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
    while (length >= 16U)
    {
        t0 = load_u32_le(message) & 0x3ffffffU;
        t1 = (load_u32_le(message + 3U) >> 2U) & 0x3ffffffU;
        t2 = (load_u32_le(message + 6U) >> 4U) & 0x3ffffffU;
        t3 = (load_u32_le(message + 9U) >> 6U) & 0x3ffffffU;
        t4 = (load_u32_le(message + 12U) >> 8U) | (1U << 24U);
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
        message += 16U;
        length -= 16U;
    }
    if (length != 0U)
    {
        ft_memset(block, 0, sizeof(block));
        ft_memcpy(block, message, length);
        block[length] = 1U;
        t0 = load_u32_le(block) & 0x3ffffffU;
        t1 = (load_u32_le(block + 3U) >> 2U) & 0x3ffffffU;
        t2 = (load_u32_le(block + 6U) >> 4U) & 0x3ffffffU;
        t3 = (load_u32_le(block + 9U) >> 6U) & 0x3ffffffU;
        t4 = load_u32_le(block + 12U) >> 8U;
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
    ft_memset(block, 0, sizeof(block));
    return (FT_ERR_SUCCESS);
}

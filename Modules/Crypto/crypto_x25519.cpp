#include "crypto_x25519.hpp"
#include "../Errno/errno.hpp"

namespace
{
    static const uint32_t CRYPTO_FIELD_LIMB_COUNT = 16U;
    static const uint32_t CRYPTO_FIELD_BASE = 65536U;
    static const uint32_t CRYPTO_FIELD_MODULUS[16] = {
        65517U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U,
        65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 32767U
    };
    static const uint8_t CRYPTO_X25519_INVERSE_EXPONENT[32] = {
        0xebU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
        0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
        0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
        0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0x7fU
    };

    static void wipe(void *data, ft_size_t length) noexcept
    {
        volatile uint8_t *bytes;
        ft_size_t index;

        bytes = static_cast<volatile uint8_t *>(data);
        index = 0U;
        while (index < length)
        {
            bytes[index] = 0U;
            index += 1U;
        }
        return ;
    }

    static void field_zero(uint32_t value[16]) noexcept
    {
        ft_memset(value, 0, sizeof(uint32_t) * CRYPTO_FIELD_LIMB_COUNT);
        return ;
    }

    static void field_one(uint32_t value[16]) noexcept
    {
        field_zero(value);
        value[0] = 1U;
        return ;
    }

    static void field_copy(uint32_t destination[16],
        const uint32_t source[16]) noexcept
    {
        ft_memcpy(destination, source,
            sizeof(uint32_t) * CRYPTO_FIELD_LIMB_COUNT);
        return ;
    }

    static void field_reduce(uint64_t value[16]) noexcept
    {
        uint32_t pass;
        uint32_t index;
        uint64_t carry;

        pass = 0U;
        while (pass < 4U)
        {
            carry = 0U;
            index = 0U;
            while (index < CRYPTO_FIELD_LIMB_COUNT)
            {
                value[index] += carry;
                carry = value[index] >> 16U;
                value[index] &= 0xffffU;
                index += 1U;
            }
            value[0] += carry * 38U;
            pass += 1U;
        }
        return ;
    }

    static void field_normalize(uint32_t value[16]) noexcept
    {
        uint64_t wide[16];
        uint32_t index;
        uint32_t borrow;
        uint64_t difference;
        uint32_t mask;
        uint64_t candidate[16];

        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            wide[index] = value[index];
            index += 1U;
        }
        field_reduce(wide);
        borrow = 0U;
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            difference = wide[index] - CRYPTO_FIELD_MODULUS[index]
                - borrow;
            borrow = static_cast<uint32_t>(difference >> 63U);
            difference += static_cast<uint64_t>(borrow) * CRYPTO_FIELD_BASE;
            candidate[index] = difference & 0xffffU;
            index += 1U;
        }
        mask = 0U - (1U - borrow);
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            value[index] = static_cast<uint32_t>((candidate[index] & mask)
                | (wide[index] & ~static_cast<uint64_t>(mask)));
            index += 1U;
        }
        return ;
    }

    static void field_add(uint32_t output[16], const uint32_t left[16],
        const uint32_t right[16]) noexcept
    {
        uint64_t wide[16];
        uint32_t index;

        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            wide[index] = static_cast<uint64_t>(left[index]) + right[index];
            index += 1U;
        }
        field_reduce(wide);
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index] = static_cast<uint32_t>(wide[index]);
            index += 1U;
        }
        field_normalize(output);
        return ;
    }

    static void field_subtract(uint32_t output[16], const uint32_t left[16],
        const uint32_t right[16]) noexcept
    {
        uint64_t value;
        uint32_t borrow;
        uint32_t mask;
        uint32_t index;
        uint64_t wide[16];

        borrow = 0U;
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            value = static_cast<uint64_t>(left[index]) - right[index]
                - borrow;
            borrow = static_cast<uint32_t>(value >> 63U);
            value += static_cast<uint64_t>(borrow) * CRYPTO_FIELD_BASE;
            wide[index] = value & 0xffffU;
            index += 1U;
        }
        mask = 0U - borrow;
        index = 0U;
        borrow = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            value = wide[index]
                + (static_cast<uint64_t>(CRYPTO_FIELD_MODULUS[index]) & mask)
                + borrow;
            borrow = static_cast<uint32_t>(value >> 16U);
            output[index] = static_cast<uint32_t>(value & 0xffffU);
            index += 1U;
        }
        field_normalize(output);
        return ;
    }

    static void field_multiply(uint32_t output[16], const uint32_t left[16],
        const uint32_t right[16]) noexcept
    {
        uint64_t product[31];
        uint64_t wide[16];
        uint32_t left_index;
        uint32_t right_index;
        uint32_t index;

        ft_memset(product, 0, sizeof(product));
        left_index = 0U;
        while (left_index < CRYPTO_FIELD_LIMB_COUNT)
        {
            right_index = 0U;
            while (right_index < CRYPTO_FIELD_LIMB_COUNT)
            {
                product[left_index + right_index]
                    += static_cast<uint64_t>(left[left_index])
                    * right[right_index];
                right_index += 1U;
            }
            left_index += 1U;
        }
        index = 30U;
        while (index >= CRYPTO_FIELD_LIMB_COUNT)
        {
            product[index - CRYPTO_FIELD_LIMB_COUNT]
                += product[index] * 38U;
            if (index == CRYPTO_FIELD_LIMB_COUNT)
                break ;
            index -= 1U;
        }
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            wide[index] = product[index];
            index += 1U;
        }
        field_reduce(wide);
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index] = static_cast<uint32_t>(wide[index]);
            index += 1U;
        }
        field_normalize(output);
        wipe(product, sizeof(product));
        wipe(wide, sizeof(wide));
        return ;
    }

    static void field_square(uint32_t output[16], const uint32_t value[16]) noexcept
    {
        field_multiply(output, value, value);
        return ;
    }

    static void field_decode(uint32_t output[16], const uint8_t input[32]) noexcept
    {
        uint32_t index;

        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index] = static_cast<uint32_t>(input[index * 2U])
                | (static_cast<uint32_t>(input[index * 2U + 1U]) << 8U);
            index += 1U;
        }
        output[15] &= 0x7fffU;
        field_normalize(output);
        return ;
    }

    static void field_encode(uint8_t output[32], const uint32_t input[16]) noexcept
    {
        uint32_t value[16];
        uint32_t index;

        field_copy(value, input);
        field_normalize(value);
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index * 2U] = static_cast<uint8_t>(value[index]);
            output[index * 2U + 1U] = static_cast<uint8_t>(value[index] >> 8U);
            index += 1U;
        }
        output[31] &= 0x7fU;
        wipe(value, sizeof(value));
        return ;
    }

    static void conditional_swap(uint32_t left[16], uint32_t right[16],
        uint32_t swap) noexcept
    {
        uint32_t mask;
        uint32_t difference;
        uint32_t index;

        mask = 0U - swap;
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            difference = (left[index] ^ right[index]) & mask;
            left[index] ^= difference;
            right[index] ^= difference;
            index += 1U;
        }
        return ;
    }

    static void field_inverse(uint32_t output[16], const uint32_t input[16]) noexcept
    {
        uint32_t result[16];
        uint32_t base[16];
        uint32_t squared[16];
        int32_t bit_index;
        uint32_t byte_index;
        uint32_t bit_offset;
        uint32_t bit;

        field_one(result);
        field_copy(base, input);
        bit_index = 254;
        while (bit_index >= 0)
        {
            field_square(squared, result);
            field_copy(result, squared);
            byte_index = static_cast<uint32_t>(bit_index) / 8U;
            bit_offset = static_cast<uint32_t>(bit_index) % 8U;
            bit = (CRYPTO_X25519_INVERSE_EXPONENT[byte_index]
                >> bit_offset) & 1U;
            if (bit != 0U)
            {
                field_multiply(squared, result, base);
                field_copy(result, squared);
            }
            bit_index -= 1;
        }
        field_copy(output, result);
        wipe(result, sizeof(result));
        wipe(base, sizeof(base));
        wipe(squared, sizeof(squared));
        return ;
    }

    static int32_t scalar_multiply(const uint8_t scalar_input[32],
        const uint8_t public_input[32], uint8_t output[32]) noexcept
    {
        uint8_t scalar[32];
        uint32_t x1[16];
        uint32_t x2[16];
        uint32_t z2[16];
        uint32_t x3[16];
        uint32_t z3[16];
        uint32_t a[16];
        uint32_t aa[16];
        uint32_t b[16];
        uint32_t bb[16];
        uint32_t e[16];
        uint32_t c[16];
        uint32_t d[16];
        uint32_t da[16];
        uint32_t cb[16];
        uint32_t temp0[16];
        uint32_t temp1[16];
        uint32_t a24[16];
        uint32_t inverse[16];
        uint32_t swap;
        uint32_t bit;
        int32_t bit_index;
        uint32_t index;

        if (scalar_input == ft_nullptr || public_input == ft_nullptr
            || output == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        ft_memcpy(scalar, scalar_input, sizeof(scalar));
        scalar[0] &= 248U;
        scalar[31] &= 127U;
        scalar[31] |= 64U;
        field_decode(x1, public_input);
        field_one(x2);
        field_zero(z2);
        field_copy(x3, x1);
        field_one(z3);
        swap = 0U;
        field_zero(a24);
        a24[0] = 121665U;
        bit_index = 254;
        while (bit_index >= 0)
        {
            bit = (scalar[static_cast<uint32_t>(bit_index) / 8U]
                >> (static_cast<uint32_t>(bit_index) % 8U)) & 1U;
            swap ^= bit;
            conditional_swap(x2, x3, swap);
            conditional_swap(z2, z3, swap);
            swap = bit;
            field_add(a, x2, z2);
            field_square(aa, a);
            field_subtract(b, x2, z2);
            field_square(bb, b);
            field_subtract(e, aa, bb);
            field_add(c, x3, z3);
            field_subtract(d, x3, z3);
            field_multiply(da, d, a);
            field_multiply(cb, c, b);
            field_add(temp0, da, cb);
            field_square(x3, temp0);
            field_subtract(temp0, da, cb);
            field_square(temp1, temp0);
            field_multiply(z3, x1, temp1);
            field_multiply(x2, aa, bb);
            field_multiply(temp0, a24, e);
            field_add(temp0, aa, temp0);
            field_multiply(z2, e, temp0);
            bit_index -= 1;
        }
        conditional_swap(x2, x3, swap);
        conditional_swap(z2, z3, swap);
        field_inverse(inverse, z2);
        field_multiply(temp0, x2, inverse);
        field_encode(output, temp0);
        index = 0U;
        bit = 0U;
        while (index < sizeof(output[0]) * 32U)
        {
            bit |= output[index];
            index += 1U;
        }
        wipe(scalar, sizeof(scalar));
        wipe(x1, sizeof(x1));
        wipe(x2, sizeof(x2));
        wipe(z2, sizeof(z2));
        wipe(x3, sizeof(x3));
        wipe(z3, sizeof(z3));
        wipe(a, sizeof(a));
        wipe(aa, sizeof(aa));
        wipe(b, sizeof(b));
        wipe(bb, sizeof(bb));
        wipe(e, sizeof(e));
        wipe(c, sizeof(c));
        wipe(d, sizeof(d));
        wipe(da, sizeof(da));
        wipe(cb, sizeof(cb));
        wipe(temp0, sizeof(temp0));
        wipe(temp1, sizeof(temp1));
        wipe(a24, sizeof(a24));
        wipe(inverse, sizeof(inverse));
        if (bit == 0U)
            return (FT_ERR_PERMISSION_DENIED);
        return (FT_ERR_SUCCESS);
    }
}

int32_t crypto_x25519_public_key(const uint8_t private_key[32],
    uint8_t public_key[32]) noexcept
{
    uint8_t basepoint[32];

    if (private_key == ft_nullptr || public_key == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memset(basepoint, 0, sizeof(basepoint));
    basepoint[0] = 9U;
    return (scalar_multiply(private_key, basepoint, public_key));
}

int32_t crypto_x25519_shared_secret(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], uint8_t shared_secret[32]) noexcept
{
    return (scalar_multiply(private_key, peer_public_key, shared_secret));
}

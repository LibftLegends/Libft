#include "crypto_x25519.hpp"
#include "crypto_primitives.hpp"
#include "../Errno/errno.hpp"

namespace
{
    constexpr uint32_t CRYPTO_FIELD_LIMB_COUNT = 16U;
    constexpr uint32_t CRYPTO_FIELD_MODULUS[16] = {
        65517U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U,
        65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 65535U, 32767U
    };

    static void wipe(void *data, ft_size_t length) noexcept
    {
        (void)crypto_secure_wipe(data, length);
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
        // Fold each carry at bit 255 using 2^255 = 19 mod p. Input limbs are
        // below 2^42, so the first fold contributes less than 2^33. A second
        // pass can fold at most 19, and if it does, the third cannot overflow.
        while (pass < 3U)
        {
            carry = 0U;
            index = 0U;
            while (index + 1U < CRYPTO_FIELD_LIMB_COUNT)
            {
                value[index] += carry;
                carry = value[index] >> 16U;
                value[index] &= 0xffffU;
                index += 1U;
            }
            value[index] += carry;
            carry = value[index] >> 15U;
            value[index] &= 0x7fffU;
            value[0] += carry * 19U;
            pass += 1U;
        }
        return ;
    }

    static void field_canonicalize(uint32_t output[16], const uint64_t wide[16]) noexcept
    {
        uint32_t index;
        uint32_t borrow;
        uint64_t difference;
        uint32_t mask;
        uint32_t candidate[16];

        // The input is below 2^255, so subtracting p once is sufficient.
        borrow = 0U;
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            difference = wide[index] - CRYPTO_FIELD_MODULUS[index]
                - borrow;
            borrow = static_cast<uint32_t>(difference >> 63U);
            candidate[index] = static_cast<uint32_t>(difference & 0xffffU);
            index += 1U;
        }
        mask = 0U - (1U - borrow);
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index] = static_cast<uint32_t>((candidate[index] & mask)
                | (wide[index] & ~static_cast<uint64_t>(mask)));
            index += 1U;
        }
        return ;
    }

    static void field_store_reduced(uint32_t output[16], uint64_t wide[16]) noexcept
    {
        field_reduce(wide);
        field_canonicalize(output, wide);
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
        field_store_reduced(output, wide);
        wipe(wide, sizeof(wide));
        return ;
    }

    static void field_subtract(uint32_t output[16], const uint32_t left[16],
        const uint32_t right[16]) noexcept
    {
        uint64_t value;
        uint32_t borrow;
        uint32_t mask;
        uint32_t index;

        borrow = 0U;
        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            value = static_cast<uint64_t>(left[index]) - right[index]
                - borrow;
            borrow = static_cast<uint32_t>(value >> 63U);
            output[index] = static_cast<uint32_t>(value & 0xffffU);
            index += 1U;
        }
        mask = 0U - borrow;
        index = 0U;
        borrow = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            value = output[index]
                + (static_cast<uint64_t>(CRYPTO_FIELD_MODULUS[index]) & mask)
                + borrow;
            borrow = static_cast<uint32_t>(value >> 16U);
            output[index] = static_cast<uint32_t>(value & 0xffffU);
            index += 1U;
        }
        return ;
    }

    static void field_finish_product(uint32_t output[16], uint64_t product[31]) noexcept
    {
        // In radix 2^16, the high half folds by 2^256 = 38 modulo 2^255 - 19.
        uint32_t index = CRYPTO_FIELD_LIMB_COUNT;
        while (index < 2U * CRYPTO_FIELD_LIMB_COUNT - 1U)
        {
            product[index - CRYPTO_FIELD_LIMB_COUNT] += product[index] * 38U;
            index += 1U;
        }
        field_store_reduced(output, product);
        return ;
    }

    static void field_multiply(uint32_t output[16], const uint32_t left[16],
        const uint32_t right[16]) noexcept
    {
        uint64_t product[31];
        uint32_t left_index;
        uint32_t right_index;

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
        field_finish_product(output, product);
        wipe(product, sizeof(product));
        return ;
    }

    static void field_square(uint32_t output[16], const uint32_t value[16]) noexcept
    {
        uint64_t product[31] = {};
        uint32_t left_index = 0U;
        while (left_index < CRYPTO_FIELD_LIMB_COUNT)
        {
            const uint64_t limb = value[left_index];
            product[2U * left_index] += limb * limb;
            const uint64_t doubled_limb = 2U * limb;
            uint32_t right_index = left_index + 1U;
            while (right_index < CRYPTO_FIELD_LIMB_COUNT)
            {
                product[left_index + right_index] += doubled_limb * value[right_index];
                right_index += 1U;
            }
            left_index += 1U;
        }
        field_finish_product(output, product);
        wipe(product, sizeof(product));
        return ;
    }

    static void field_multiply_ladder_constant(uint32_t output[16],
        const uint32_t value[16]) noexcept
    {
        constexpr uint64_t LADDER_CONSTANT = 121665U;
        uint64_t wide[16];
        uint32_t index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            wide[index] = value[index] * LADDER_CONSTANT;
            index += 1U;
        }
        field_store_reduced(output, wide);
        wipe(wide, sizeof(wide));
        return ;
    }

    static void field_decode(uint32_t output[16], const uint8_t input[32]) noexcept
    {
        uint64_t wide[16];
        uint32_t index;

        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            wide[index] = static_cast<uint32_t>(input[index * 2U])
                | (static_cast<uint32_t>(input[index * 2U + 1U]) << 8U);
            index += 1U;
        }
        wide[15] &= 0x7fffU;
        field_canonicalize(output, wide);
        return ;
    }

    static void field_encode(uint8_t output[32], const uint32_t input[16]) noexcept
    {
        uint32_t index;

        index = 0U;
        while (index < CRYPTO_FIELD_LIMB_COUNT)
        {
            output[index * 2U] = static_cast<uint8_t>(input[index]);
            output[index * 2U + 1U] = static_cast<uint8_t>(input[index] >> 8U);
            index += 1U;
        }
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

    static void field_square_repeated(uint32_t output[16], const uint32_t input[16],
        uint32_t count) noexcept
    {
        field_square(output, input);
        while (count > 1U)
        {
            field_square(output, output);
            count -= 1U;
        }
        return ;
    }

    static void field_inverse(uint32_t output[16], const uint32_t input[16]) noexcept
    {
        uint32_t power_eleven[16];
        uint32_t power_five_bits[16];
        uint32_t power_ten_bits[16];
        uint32_t power_fifty_bits[16];
        uint32_t intermediate[16];
        uint32_t squared[16];

        field_square(intermediate, input);                         // z^2
        field_square_repeated(power_five_bits, intermediate, 2U);   // z^8
        field_multiply(power_five_bits, power_five_bits, input);    // z^9
        field_multiply(power_eleven, power_five_bits, intermediate);
        field_square(intermediate, power_eleven);                  // z^22
        field_multiply(power_five_bits, intermediate, power_five_bits);

        // Extend z^(2^n - 1) with blocks of n one-bits.
        field_square_repeated(power_ten_bits, power_five_bits, 5U);
        field_multiply(power_ten_bits, power_ten_bits, power_five_bits);
        field_square_repeated(intermediate, power_ten_bits, 10U);
        field_multiply(intermediate, intermediate, power_ten_bits); // 20 bits
        field_square_repeated(power_fifty_bits, intermediate, 20U);
        field_multiply(power_fifty_bits, power_fifty_bits, intermediate); // 40 bits
        field_square_repeated(power_fifty_bits, power_fifty_bits, 10U);
        field_multiply(power_fifty_bits, power_fifty_bits, power_ten_bits);
        field_square_repeated(intermediate, power_fifty_bits, 50U);
        field_multiply(intermediate, intermediate, power_fifty_bits); // 100 bits
        field_square_repeated(squared, intermediate, 100U);
        field_multiply(intermediate, squared, intermediate);        // 200 bits
        field_square_repeated(squared, intermediate, 50U);
        field_multiply(intermediate, squared, power_fifty_bits);    // 250 bits
        field_square_repeated(squared, intermediate, 5U);
        // (2^250 - 1) * 2^5 + 11 = 2^255 - 21 = p - 2.
        field_multiply(output, squared, power_eleven);

        wipe(power_eleven, sizeof(power_eleven));
        wipe(power_five_bits, sizeof(power_five_bits));
        wipe(power_ten_bits, sizeof(power_ten_bits));
        wipe(power_fifty_bits, sizeof(power_fifty_bits));
        wipe(intermediate, sizeof(intermediate));
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
        uint32_t inverse[16];
        uint32_t swap;
        uint32_t bit;
        int32_t bit_index;
        uint32_t index;

        if (scalar_input == ft_nullptr || public_input == ft_nullptr
            || output == ft_nullptr)
        {
            if (output != ft_nullptr)
                wipe(output, 32U);
            return (FT_ERR_INVALID_ARGUMENT);
        }
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
            field_multiply_ladder_constant(temp0, e);
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
    {
        if (public_key != ft_nullptr)
            wipe(public_key, 32U);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    ft_memset(basepoint, 0, sizeof(basepoint));
    basepoint[0] = 9U;
    {
        int32_t result;

        result = scalar_multiply(private_key, basepoint, public_key);
        wipe(basepoint, sizeof(basepoint));
        return (result);
    }
}

int32_t crypto_x25519_shared_secret(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], uint8_t shared_secret[32]) noexcept
{
    return (scalar_multiply(private_key, peer_public_key, shared_secret));
}

#ifdef LIBFT_TEST_BUILD
void crypto_x25519_test_reduce_limbs(uint32_t output[16], const uint64_t limbs[16]) noexcept
{
    uint64_t wide[16];
    ft_memcpy_fixed<sizeof(wide)>(wide, limbs);
    field_store_reduced(output, wide);
    return ;
}
#endif

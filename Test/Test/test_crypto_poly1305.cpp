#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_poly1305.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_crypto_poly1305_rfc_vector_and_key_message_mutations)
{
    const uint8_t key[32] = {
        0x85U, 0xd6U, 0xbeU, 0x78U, 0x57U, 0x55U, 0x6dU, 0x33U,
        0x7fU, 0x44U, 0x52U, 0xfeU, 0x42U, 0xd5U, 0x06U, 0xa8U,
        0x01U, 0x03U, 0x80U, 0x8aU, 0xfbU, 0x0dU, 0xb2U, 0xfdU,
        0x4aU, 0xbfU, 0xf6U, 0xafU, 0x41U, 0x49U, 0xf5U, 0x1bU};
    const uint8_t expected[16] = {
        0xa8U, 0x06U, 0x1dU, 0xc1U, 0x30U, 0x51U, 0x36U, 0xc6U,
        0xc2U, 0x2bU, 0x8bU, 0xafU, 0x0cU, 0x01U, 0x27U, 0xa9U};
    const uint8_t message[] = "Cryptographic Forum Research Group";
    uint8_t message_bytes[sizeof(message) - 1U];
    uint8_t mutated_key[32];
    uint8_t first_tag[16];
    uint8_t second_tag[16];
    uint32_t byte_index = 0U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(first_tag, message,
        sizeof(message) - 1U, key));
    FT_ASSERT_EQ(0, ft_memcmp(first_tag, expected, sizeof(expected)));
    ft_memcpy(message_bytes, message, sizeof(message_bytes));
    while (byte_index < sizeof(message_bytes))
    {
        message_bytes[byte_index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(second_tag,
            message_bytes, sizeof(message_bytes), key));
        FT_ASSERT(ft_memcmp(first_tag, second_tag, sizeof(first_tag)) != 0);
        message_bytes[byte_index] ^= 1U;
        byte_index = byte_index + 1U;
    }
    ft_memcpy(mutated_key, key, sizeof(mutated_key));
    /* The first half is clamped before use; mutate the effective pad bytes. */
    byte_index = 16U;
    while (byte_index < sizeof(mutated_key))
    {
        mutated_key[byte_index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(second_tag,
            message, sizeof(message) - 1U, mutated_key));
        FT_ASSERT(ft_memcmp(first_tag, second_tag, sizeof(first_tag)) != 0);
        mutated_key[byte_index] ^= 1U;
        byte_index = byte_index + 1U;
    }
    return (1);
}

FT_TEST(test_crypto_poly1305_boundary_lengths_and_clamping)
{
    const ft_size_t lengths[9] = {0U, 1U, 15U, 16U, 17U, 31U, 32U, 33U,
        64U};
    uint8_t key[32];
    uint8_t equivalent_key[32];
    uint8_t message[64];
    uint8_t tag[16];
    uint8_t changed_tag[16];
    ft_size_t length_index = 0U;
    uint32_t byte_index;

    byte_index = 0U;
    while (byte_index < sizeof(key))
    {
        key[byte_index] = static_cast<uint8_t>(byte_index * 29U + 0xffU);
        byte_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(message))
    {
        message[byte_index] = static_cast<uint8_t>(0xffU - byte_index);
        byte_index += 1U;
    }
    while (length_index < 9U)
    {
        ft_size_t length = lengths[length_index];

        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(tag, message,
            length, key));
        byte_index = 0U;
        while (byte_index < length)
        {
            message[byte_index] ^= 1U;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(changed_tag,
                message, length, key));
            FT_ASSERT(ft_memcmp(tag, changed_tag, sizeof(tag)) != 0);
            message[byte_index] ^= 1U;
            byte_index += 1U;
        }
        length_index += 1U;
    }
    ft_memset(key, 0xff, sizeof(key));
    ft_memcpy(equivalent_key, key, sizeof(key));
    equivalent_key[3U] ^= 0xf0U;
    equivalent_key[7U] ^= 0xf0U;
    equivalent_key[11U] ^= 0xf0U;
    equivalent_key[15U] ^= 0xf0U;
    equivalent_key[4U] ^= 0x03U;
    equivalent_key[8U] ^= 0x03U;
    equivalent_key[12U] ^= 0x03U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(tag, message,
        sizeof(message), key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(changed_tag, message,
        sizeof(message), equivalent_key));
    FT_ASSERT_EQ(0, ft_memcmp(tag, changed_tag, sizeof(tag)));
    equivalent_key[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(changed_tag, message,
        sizeof(message), equivalent_key));
    FT_ASSERT(ft_memcmp(tag, changed_tag, sizeof(tag)) != 0);
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_chacha20.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Crypto/crypto_poly1305.hpp"
#include "../../Modules/Crypto/crypto_session.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_bool crypto_boundary_equal(const uint8_t *left,
    const uint8_t *right, ft_size_t length) noexcept
{
    ft_size_t index = 0U;

    while (index < length)
    {
        if (left[index] != right[index])
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

FT_TEST(test_crypto_sha256_length_boundaries_and_lifecycle)
{
    uint8_t message[129];
    uint8_t one_shot[32];
    uint8_t incremental[32];
    crypto_sha256 context;
    ft_size_t lengths[9] = {1U, 55U, 56U, 63U, 64U, 65U, 127U,
        128U, 129U};
    uint32_t length_index = 0U;
    ft_size_t index = 0U;

    while (index < sizeof(message))
    {
        message[index] = static_cast<uint8_t>(index * 13U + 7U);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.update(message, 1U));
    while (length_index < 9U)
    {
        ft_size_t length = lengths[length_index];

        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            crypto_sha256_hash(message, length, one_shot));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
        index = 0U;
        while (index < length)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(message + index, 1U));
            index += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(incremental));
        FT_ASSERT(crypto_boundary_equal(one_shot, incremental, 32U));
        FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.final(incremental));
        length_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crypto_sha256_hash(ft_nullptr, 1U, one_shot));
    return (1);
}

FT_TEST(test_crypto_hkdf_boundaries_and_invalid_arguments)
{
    uint8_t key[32] = {0U};
    uint8_t output[33] = {0U};
    ft_size_t lengths[5] = {0U, 1U, 31U, 32U, 33U};
    uint32_t index = 0U;

    while (index < 5U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(key,
            ft_nullptr, 0U, output, lengths[index]));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(key,
        ft_nullptr, 0U, output, 8161U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_hkdf_sha256_expand(key,
        ft_nullptr, 1U, output, 1U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_hkdf_sha256_expand(key,
        ft_nullptr, 0U, ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(ft_nullptr, 0U,
        ft_nullptr, 0U, output));
    return (1);
}

FT_TEST(test_crypto_chacha20_boundary_arguments_and_counter_wrap)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {0U};
    const uint8_t input[65] = {0U};
    ft_vector<uint8_t> output;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_chacha20_xor(key, nonce,
        1U, ft_nullptr, 1U, output));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_chacha20_xor(key, nonce,
        0U, input, 1U, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce,
        0xffffffffU, input, 64U, output));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_chacha20_xor(key, nonce,
        0xffffffffU, input, 65U, output));
    FT_ASSERT_EQ(0U, output.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}

FT_TEST(test_crypto_aead_tag_mutation_matrix)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {1U};
    const uint8_t associated_data[2] = {2U, 3U};
    const uint8_t plaintext[40] = {4U};
    uint8_t tag[16];
    uint8_t mutated_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext_output;
    uint32_t bit_index = 0U;
    uint32_t byte_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext, sizeof(plaintext),
        ciphertext, tag));
    while (bit_index < 128U)
    {
        ft_memcpy(mutated_tag, tag, sizeof(mutated_tag));
        mutated_tag[bit_index / 8U] ^= static_cast<uint8_t>(
            1U << (bit_index % 8U));
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
            crypto_chacha20_poly1305_open(key, nonce, associated_data,
                sizeof(associated_data), &ciphertext[0], ciphertext.size(),
                mutated_tag, plaintext_output));
        FT_ASSERT_EQ(0U, plaintext_output.size());
        bit_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < ciphertext.size())
    {
        ft_vector<uint8_t> mutated_ciphertext;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, mutated_ciphertext.initialize());
        uint32_t copy_index = 0U;
        while (copy_index < ciphertext.size())
        {
            uint8_t value = ciphertext[copy_index];
            if (copy_index == byte_index)
                value ^= 1U;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, mutated_ciphertext.push_back(value));
            copy_index += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
            crypto_chacha20_poly1305_open(key, nonce, associated_data,
                sizeof(associated_data), &mutated_ciphertext[0],
                mutated_ciphertext.size(), tag, plaintext_output));
        FT_ASSERT_EQ(0U, plaintext_output.size());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, mutated_ciphertext.destroy());
        byte_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext_output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_x25519_low_order_inputs_are_rejected)
{
    const uint8_t private_key[32] = {7U};
    uint8_t low_order_public[32] = {0U};
    uint8_t shared_secret[32];

    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        crypto_x25519_shared_secret(private_key, low_order_public,
            shared_secret));
    ft_memset(low_order_public, 0, sizeof(low_order_public));
    low_order_public[0] = 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        crypto_x25519_shared_secret(private_key, low_order_public,
            shared_secret));
    return (1);
}

FT_TEST(test_crypto_poly1305_boundary_lengths_and_mutations)
{
    const uint8_t key[32] = {
        0x85U, 0xd6U, 0xbeU, 0x78U, 0x57U, 0x55U, 0x6dU, 0x33U,
        0x7fU, 0x44U, 0x52U, 0xfeU, 0x42U, 0xd5U, 0x06U, 0xa8U,
        0x01U, 0x03U, 0x80U, 0x8aU, 0xfbU, 0x0dU, 0xb2U, 0xfdU,
        0x4aU, 0xbfU, 0xf6U, 0xafU, 0x41U, 0x49U, 0xf5U, 0x1bU
    };
    uint8_t message[33] = {0U};
    uint8_t first_tag[16];
    uint8_t second_tag[16];
    const uint8_t *message_data;
    ft_size_t lengths[9] = {0U, 1U, 15U, 16U, 17U, 31U, 32U, 33U,
        sizeof(message)};
    uint32_t length_index = 0U;

    while (length_index < 9U)
    {
        ft_size_t length = lengths[length_index];

        message_data = ft_nullptr;
        if (length > 0U)
            message_data = message;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(first_tag,
            message_data, length, key));
        if (length > 0U)
        {
            message[length - 1U] ^= 1U;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(second_tag,
                message, length, key));
            FT_ASSERT(!crypto_boundary_equal(first_tag, second_tag, 16U));
            message[length - 1U] ^= 1U;
        }
        length_index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_aead_rejects_metadata_and_truncation_mutations)
{
    const uint8_t key[32] = {1U};
    const uint8_t nonce[12] = {2U};
    const uint8_t associated_data[7] = {3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const uint8_t plaintext[17] = {10U};
    uint8_t mutated_key[32];
    uint8_t mutated_nonce[12];
    uint8_t mutated_associated_data[7];
    uint8_t tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext_output;
    ft_size_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext, sizeof(plaintext),
        ciphertext, tag));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext_output.initialize());
    ft_memcpy(mutated_key, key, sizeof(mutated_key));
    mutated_key[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        mutated_key, nonce, associated_data, sizeof(associated_data),
        &ciphertext[0], ciphertext.size(), tag, plaintext_output));
    FT_ASSERT_EQ(0U, plaintext_output.size());
    ft_memcpy(mutated_nonce, nonce, sizeof(mutated_nonce));
    mutated_nonce[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        key, mutated_nonce, associated_data, sizeof(associated_data),
        &ciphertext[0], ciphertext.size(), tag, plaintext_output));
    ft_memcpy(mutated_associated_data, associated_data,
        sizeof(mutated_associated_data));
    mutated_associated_data[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        key, nonce, mutated_associated_data, sizeof(mutated_associated_data),
        &ciphertext[0], ciphertext.size(), tag, plaintext_output));
    index = 0U;
    while (index < ciphertext.size())
    {
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
            key, nonce, associated_data, sizeof(associated_data),
            &ciphertext[0], index, tag, plaintext_output));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plaintext_output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_session_key_update_epochs_are_distinct)
{
    const uint8_t key[32] = {9U};
    uint8_t first_key[32];
    uint8_t second_key[32];
    uint8_t first_iv[12];
    uint8_t second_iv[12];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_key_update(key, 1U,
        first_key, first_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_key_update(key, 2U,
        second_key, second_iv));
    FT_ASSERT(!crypto_boundary_equal(first_key, second_key, 32U));
    FT_ASSERT(!crypto_boundary_equal(first_iv, second_iv, 12U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_derive_key_update(
        ft_nullptr, 1U, first_key, first_iv));
    return (1);
}

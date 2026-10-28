#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_crypto_aead_empty_and_block_boundary_roundtrips)
{
    const uint8_t key[32] = {0x11U};
    const uint8_t nonce[12] = {0x22U};
    const ft_size_t lengths[8] = {0U, 1U, 15U, 16U, 17U, 63U, 64U, 65U};
    uint8_t associated_data[65];
    uint8_t plaintext[65];
    uint8_t tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> recovered;
    uint32_t byte_index = 0U;
    uint32_t associated_index = 0U;
    uint32_t plaintext_index;

    while (byte_index < sizeof(associated_data))
    {
        associated_data[byte_index] = static_cast<uint8_t>(byte_index + 7U);
        plaintext[byte_index] = static_cast<uint8_t>(byte_index * 3U + 1U);
        byte_index = byte_index + 1U;
    }
    while (associated_index < 8U)
    {
        plaintext_index = 0U;
        while (plaintext_index < 8U)
        {
            ft_size_t associated_length = lengths[associated_index];
            ft_size_t plaintext_length = lengths[plaintext_index];
            const uint8_t *associated_data_pointer = ft_nullptr;
            const uint8_t *plaintext_pointer = ft_nullptr;
            const uint8_t *ciphertext_pointer = ft_nullptr;

            if (associated_length != 0U)
                associated_data_pointer = associated_data;
            if (plaintext_length != 0U)
                plaintext_pointer = plaintext;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key,
                nonce, associated_data_pointer, associated_length,
                plaintext_pointer, plaintext_length, ciphertext, tag));
            FT_ASSERT_EQ(plaintext_length, ciphertext.size());
            if (ciphertext.size() != 0U)
                ciphertext_pointer = &ciphertext[0];
            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_open(key,
                nonce, associated_data_pointer, associated_length,
                ciphertext_pointer, ciphertext.size(), tag, recovered));
            FT_ASSERT_EQ(plaintext_length, recovered.size());
            if (plaintext_length != 0U)
                FT_ASSERT_EQ(0, ft_memcmp(plaintext, &recovered[0],
                    plaintext_length));
            plaintext_index = plaintext_index + 1U;
        }
        associated_index = associated_index + 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.destroy());
    return (1);
}

FT_TEST(test_crypto_aead_authentication_mutation_matrix)
{
    uint8_t key[32];
    uint8_t changed_key[32];
    uint8_t nonce[12];
    uint8_t changed_nonce[12];
    uint8_t associated_data[19];
    uint8_t changed_associated_data[19];
    uint8_t plaintext[33];
    uint8_t original_tag[16];
    uint8_t changed_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> opened;
    ft_size_t index;
    uint32_t bit_index;

    index = 0U;
    while (index < sizeof(key))
    {
        key[index] = static_cast<uint8_t>(index * 13U + 5U);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(nonce))
    {
        nonce[index] = static_cast<uint8_t>(index * 7U + 9U);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(associated_data))
    {
        associated_data[index] = static_cast<uint8_t>(index * 17U + 3U);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(plaintext))
    {
        plaintext[index] = static_cast<uint8_t>(index * 23U + 11U);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext), ciphertext, original_tag));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.initialize());
    bit_index = 0U;
    while (bit_index < 128U)
    {
        ft_memcpy(changed_tag, original_tag, sizeof(changed_tag));
        changed_tag[bit_index / 8U] ^= static_cast<uint8_t>(
            1U << (bit_index % 8U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.push_back(0xeeU));
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
            key, nonce, associated_data, sizeof(associated_data),
            &ciphertext[0U], ciphertext.size(), changed_tag, opened));
        FT_ASSERT_EQ(0U, opened.size());
        bit_index += 1U;
    }
    index = 0U;
    while (index < ciphertext.size())
    {
        uint8_t original_byte = ciphertext[index];

        ciphertext[index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
            key, nonce, associated_data, sizeof(associated_data),
            &ciphertext[0U], ciphertext.size(), original_tag, opened));
        FT_ASSERT_EQ(0U, opened.size());
        ciphertext[index] = original_byte;
        index += 1U;
    }
    ft_memcpy(changed_associated_data, associated_data,
        sizeof(changed_associated_data));
    index = 0U;
    while (index < sizeof(changed_associated_data))
    {
        changed_associated_data[index] ^= 1U;
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
            key, nonce, changed_associated_data, sizeof(changed_associated_data),
            &ciphertext[0U], ciphertext.size(), original_tag, opened));
        FT_ASSERT_EQ(0U, opened.size());
        changed_associated_data[index] ^= 1U;
        index += 1U;
    }
    ft_memcpy(changed_key, key, sizeof(changed_key));
    changed_key[0U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        changed_key, nonce, associated_data, sizeof(associated_data),
        &ciphertext[0U], ciphertext.size(), original_tag, opened));
    FT_ASSERT_EQ(0U, opened.size());
    ft_memcpy(changed_nonce, nonce, sizeof(changed_nonce));
    changed_nonce[0U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        key, changed_nonce, associated_data, sizeof(associated_data),
        &ciphertext[0U], ciphertext.size(), original_tag, opened));
    FT_ASSERT_EQ(0U, opened.size());
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
        key, nonce, associated_data, sizeof(associated_data) - 1U,
        &ciphertext[0U], ciphertext.size(), original_tag, opened));
    FT_ASSERT_EQ(0U, opened.size());
    index = 0U;
    while (index < ciphertext.size())
    {
        const uint8_t *ciphertext_pointer = ft_nullptr;

        if (index != 0U)
            ciphertext_pointer = &ciphertext[0U];
        FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(
            key, nonce, associated_data, sizeof(associated_data),
            ciphertext_pointer, index, original_tag, opened));
        FT_ASSERT_EQ(0U, opened.size());
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

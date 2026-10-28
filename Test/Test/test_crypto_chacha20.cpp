#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_chacha20.hpp"
#include "../../Modules/Errno/errno.hpp"

static uint8_t crypto_test_hex_nibble(char value) noexcept
{
    if (value >= '0' && value <= '9')
        return (static_cast<uint8_t>(value - '0'));
    if (value >= 'a' && value <= 'f')
        return (static_cast<uint8_t>(value - 'a' + 10));
    return (0U);
}

FT_TEST(test_crypto_chacha20_block_and_counter_boundaries)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {0U};
    const ft_size_t lengths[4] = {1U, 63U, 64U, 65U};
    const uint32_t counters[3] = {0U, 1U, UINT32_MAX - 1U};
    uint8_t input[65];
    uint32_t length_index = 0U;
    uint32_t counter_index = 0U;
    uint32_t byte_index = 0U;
    ft_vector<uint8_t> encrypted;
    ft_vector<uint8_t> recovered;

    while (byte_index < sizeof(input))
    {
        input[byte_index] = static_cast<uint8_t>(byte_index * 17U + 3U);
        byte_index = byte_index + 1U;
    }
    while (counter_index < 3U)
    {
        length_index = 0U;
        while (length_index < 4U)
        {
            ft_size_t length = lengths[length_index];

            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce,
                counters[counter_index], input, length, encrypted));
            FT_ASSERT_EQ(length, encrypted.size());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce,
                counters[counter_index], &encrypted[0], length, recovered));
            FT_ASSERT_EQ(length, recovered.size());
            FT_ASSERT_EQ(0, ft_memcmp(input, &recovered[0], length));
            length_index = length_index + 1U;
        }
        counter_index = counter_index + 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce,
        UINT32_MAX, input, 64U, recovered));
    FT_ASSERT_EQ(64U, recovered.size());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_chacha20_xor(key, nonce,
        UINT32_MAX, input, 65U, recovered));
    FT_ASSERT_EQ(0U, recovered.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encrypted.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovered.destroy());
    return (1);
}

FT_TEST(test_crypto_chacha20_rfc8439_multiblock_vector)
{
    const char plaintext[] = "Ladies and Gentlemen of the class of '99: If I "
        "could offer you only one tip for the future, sunscreen would be it.";
    const char expected_hex[] =
        "6e2e359a2568f98041ba0728dd0d6981"
        "e97e7aec1d4360c20a27afccfd9fae0b"
        "f91b65c5524733ab8f593dabcd62b357"
        "1639d624e65152ab8f530c359f0861d8"
        "07ca0dbf500d6a6156a38e088a22b65e"
        "52bc514d16ccf806818ce91ab7793736"
        "5af90bbf74a35be6b40b8eedf2785e42"
        "874d";
    uint8_t key[32];
    uint8_t nonce[12] = {0U};
    ft_vector<uint8_t> ciphertext;
    ft_size_t index;

    index = 0U;
    while (index < sizeof(key))
    {
        key[index] = static_cast<uint8_t>(index);
        index += 1U;
    }
    nonce[7] = 0x4aU;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce, 1U,
        reinterpret_cast<const uint8_t *>(plaintext), sizeof(plaintext) - 1U,
        ciphertext));
    FT_ASSERT_EQ((sizeof(expected_hex) - 1U) / 2U, ciphertext.size());
    index = 0U;
    while (index < ciphertext.size())
    {
        uint8_t expected_byte = static_cast<uint8_t>(
            (crypto_test_hex_nibble(expected_hex[index * 2U]) << 4U)
            | crypto_test_hex_nibble(expected_hex[index * 2U + 1U]));

        FT_ASSERT_EQ(expected_byte, ciphertext[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce, 1U,
        &ciphertext[0], ciphertext.size(), ciphertext));
    FT_ASSERT_EQ(sizeof(plaintext) - 1U, ciphertext.size());
    FT_ASSERT_EQ(0, ft_memcmp(&ciphertext[0], plaintext,
        sizeof(plaintext) - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_chacha20_rfc8439_quarter_round_and_block_vectors)
{
    uint32_t quarter_round_state[16] = {0U};
    const uint32_t quarter_round_expected[4] = {
        0xea2a92f4U, 0xcb1cf8ceU, 0x4581472eU, 0x5881c4bbU};
    uint8_t key[32];
    const uint8_t nonce[12] = {
        0x00U, 0x00U, 0x00U, 0x09U, 0x00U, 0x00U,
        0x00U, 0x4aU, 0x00U, 0x00U, 0x00U, 0x00U};
    const uint8_t expected_block[64] = {
        0x10U, 0xf1U, 0xe7U, 0xe4U, 0xd1U, 0x3bU, 0x59U, 0x15U,
        0x50U, 0x0fU, 0xddU, 0x1fU, 0xa3U, 0x20U, 0x71U, 0xc4U,
        0xc7U, 0xd1U, 0xf4U, 0xc7U, 0x33U, 0xc0U, 0x68U, 0x03U,
        0x04U, 0x22U, 0xaaU, 0x9aU, 0xc3U, 0xd4U, 0x6cU, 0x4eU,
        0xd2U, 0x82U, 0x64U, 0x46U, 0x07U, 0x9fU, 0xaaU, 0x09U,
        0x14U, 0xc2U, 0xd7U, 0x05U, 0xd9U, 0x8bU, 0x02U, 0xa2U,
        0xb5U, 0x12U, 0x9cU, 0xd1U, 0xdeU, 0x16U, 0x4eU, 0xb9U,
        0xcbU, 0xd0U, 0x83U, 0xe8U, 0xa2U, 0x50U, 0x3cU, 0x4e};
    uint8_t block[64];
    uint32_t index;

    quarter_round_state[0] = 0x11111111U;
    quarter_round_state[1] = 0x01020304U;
    quarter_round_state[2] = 0x9b8d6f43U;
    quarter_round_state[3] = 0x01234567U;
    crypto_chacha20_test_quarter_round(quarter_round_state, 0U, 1U, 2U, 3U);
    FT_ASSERT_EQ(quarter_round_expected[0], quarter_round_state[0]);
    FT_ASSERT_EQ(quarter_round_expected[1], quarter_round_state[1]);
    FT_ASSERT_EQ(quarter_round_expected[2], quarter_round_state[2]);
    FT_ASSERT_EQ(quarter_round_expected[3], quarter_round_state[3]);
    index = 0U;
    while (index < sizeof(key))
    {
        key[index] = static_cast<uint8_t>(index);
        index += 1U;
    }
    crypto_chacha20_block(key, 1U, nonce, block);
    FT_ASSERT_EQ(0, ft_memcmp(block, expected_block, sizeof(expected_block)));
    return (1);
}

FT_TEST(test_crypto_chacha20_zero_length_input)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {0U};
    ft_vector<uint8_t> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce, 0U,
        ft_nullptr, 0U, output));
    FT_ASSERT_EQ(0U, output.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}


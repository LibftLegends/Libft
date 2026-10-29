#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Errno/errno.hpp"

static int32_t crypto_hmac_test_decode_hex(const char *hexadecimal,
    uint8_t *output, ft_size_t output_capacity,
    ft_size_t *output_length) noexcept
{
    ft_size_t index = 0U;
    ft_size_t length = 0U;

    if (hexadecimal == ft_nullptr || output_length == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    while (hexadecimal[index] != '\0')
    {
        uint8_t high_nibble;
        uint8_t low_nibble;
        char high_character = hexadecimal[index];
        char low_character = hexadecimal[index + 1U];

        if (low_character == '\0' || length >= output_capacity
            || output == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        if (high_character >= '0' && high_character <= '9')
            high_nibble = static_cast<uint8_t>(high_character - '0');
        else if (high_character >= 'a' && high_character <= 'f')
            high_nibble = static_cast<uint8_t>(high_character - 'a' + 10);
        else
            return (FT_ERR_INVALID_ARGUMENT);
        if (low_character >= '0' && low_character <= '9')
            low_nibble = static_cast<uint8_t>(low_character - '0');
        else if (low_character >= 'a' && low_character <= 'f')
            low_nibble = static_cast<uint8_t>(low_character - 'a' + 10);
        else
            return (FT_ERR_INVALID_ARGUMENT);
        output[length] = static_cast<uint8_t>((high_nibble << 4U)
            | low_nibble);
        length += 1U;
        index += 2U;
    }
    *output_length = length;
    return (FT_ERR_SUCCESS);
}

static int32_t crypto_hmac_test_vector(const uint8_t *key,
    ft_size_t key_length, const uint8_t *data, ft_size_t data_length,
    const char *expected_hexadecimal, ft_size_t expected_length) noexcept
{
    uint8_t digest[32];
    uint8_t expected[32];
    ft_size_t decoded_length = 0U;
    int32_t result;

    result = crypto_hmac_test_decode_hex(expected_hexadecimal, expected,
        sizeof(expected), &decoded_length);
    if (result == FT_ERR_SUCCESS && decoded_length != expected_length)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
        result = crypto_hmac_sha256(key, key_length, data, data_length,
            digest);
    if (result == FT_ERR_SUCCESS
        && ft_memcmp(digest, expected, expected_length) != 0)
        result = FT_ERR_INVALID_STATE;
    (void)crypto_secure_wipe(digest, sizeof(digest));
    (void)crypto_secure_wipe(expected, sizeof(expected));
    return (result);
}

FT_TEST(test_crypto_hmac_hkdf_key_and_salt_boundaries)
{
    uint8_t key[128];
    uint8_t data[129];
    uint8_t digest[32];
    uint8_t empty_salt_prk[32];
    uint8_t explicit_zero_salt_prk[32];
    uint8_t zero_salt[32];
    ft_size_t key_lengths[9] = {0U, 1U, 31U, 32U, 63U, 64U, 65U, 127U,
        128U};
    uint32_t key_index = 0U;
    ft_size_t index = 0U;

    while (index < sizeof(key))
    {
        key[index] = static_cast<uint8_t>(index * 19U + 17U);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(data))
    {
        data[index] = static_cast<uint8_t>(index * 23U + 9U);
        index += 1U;
    }
    ft_memset(zero_salt, 0, sizeof(zero_salt));
    while (key_index < 9U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(key,
            key_lengths[key_index], data, sizeof(data), digest));
        key_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(ft_nullptr, 0U,
        data, sizeof(data), empty_salt_prk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(zero_salt,
        sizeof(zero_salt), data, sizeof(data), explicit_zero_salt_prk));
    FT_ASSERT_EQ(0, ft_memcmp(empty_salt_prk, explicit_zero_salt_prk,
        sizeof(empty_salt_prk)));
    return (1);
}

FT_TEST(test_crypto_hmac_sha256_rfc4231_known_answers)
{
    uint8_t key[131];
    uint8_t data[128];
    uint32_t index;
    int32_t result;

    ft_memset(key, 0x0b, 20U);
    result = crypto_hmac_test_vector(key, 20U,
        reinterpret_cast<const uint8_t *>("Hi There"), 8U,
        "b0344c61d8db38535ca8afceaf0bf12b"
        "881dc200c9833da726e9376c2e32cff7", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    result = crypto_hmac_test_vector(
        reinterpret_cast<const uint8_t *>("Jefe"), 4U,
        reinterpret_cast<const uint8_t *>("what do ya want for nothing?"),
        28U, "5bdcc146bf60754e6a042426089575c7"
        "5a003f089d2739839dec58b964ec3843", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    ft_memset(key, 0xaa, 20U);
    ft_memset(data, 0xdd, 50U);
    result = crypto_hmac_test_vector(key, 20U, data, 50U,
        "773ea91e36800e46854db8ebd09181a7"
        "2959098b3ef8c122d9635514ced565fe", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    index = 0U;
    while (index < 25U)
    {
        key[index] = static_cast<uint8_t>(index + 1U);
        index += 1U;
    }
    ft_memset(data, 0xcd, 50U);
    result = crypto_hmac_test_vector(key, 25U, data, 50U,
        "82558a389a443c0ea4cc819899f2083a"
        "85f0faa3e578f8077a2e3ff46729665b", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    ft_memset(key, 0x0c, 20U);
    result = crypto_hmac_test_vector(key, 20U,
        reinterpret_cast<const uint8_t *>("Test With Truncation"), 20U,
        "a3b6167473100ee06e0c796c2955552b", 16U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    ft_memset(key, 0xaa, sizeof(key));
    result = crypto_hmac_test_vector(key, sizeof(key),
        reinterpret_cast<const uint8_t *>(
            "Test Using Larger Than Block-Size Key - Hash Key First"),
        54U, "60e431591ee0b67f0d8a26aacbf5b77f"
        "8e0bc6213728c5140546040f0ee37f54", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = crypto_hmac_test_vector(key, sizeof(key),
        reinterpret_cast<const uint8_t *>("This is a test using a larger than "
            "block-size key and a larger than block-size data. The key needs "
            "to be hashed before being used by the HMAC algorithm."),
        152U, "9b09ffa71b942fcb27635fbcd5b0e944"
        "bfdc63644f0713938a7f51535c3a35e2", 32U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    return (1);
}

FT_TEST(test_crypto_hkdf_sha256_rfc5869_known_answers)
{
    uint8_t input_key_material[22];
    uint8_t salt[13];
    uint8_t info[10];
    uint8_t pseudorandom_key[32];
    uint8_t expected_prk[32];
    uint8_t output[42];
    uint8_t expected_output[42];
    ft_size_t decoded_length;
    uint32_t index;
    int32_t result;

    ft_memset(input_key_material, 0x0b, sizeof(input_key_material));
    index = 0U;
    while (index < sizeof(salt))
    {
        salt[index] = static_cast<uint8_t>(index);
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(info))
    {
        info[index] = static_cast<uint8_t>(0xf0U + index);
        index += 1U;
    }
    result = crypto_hkdf_sha256_extract(salt, sizeof(salt),
        input_key_material, sizeof(input_key_material), pseudorandom_key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "077709362c2e32df0ddc3f0dc47bba63"
        "90b6c73bb50f9c3122ec844ad7c2b3e5", expected_prk,
        sizeof(expected_prk), &decoded_length));
    FT_ASSERT_EQ(sizeof(expected_prk), decoded_length);
    FT_ASSERT_EQ(0, ft_memcmp(pseudorandom_key, expected_prk,
        sizeof(expected_prk)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
        pseudorandom_key, info, sizeof(info), output, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "3cb25f25faacd57a90434f64d0362f2a"
        "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
        "34007208d5b887185865", expected_output,
        sizeof(expected_output), &decoded_length));
    FT_ASSERT_EQ(sizeof(expected_output), decoded_length);
    FT_ASSERT_EQ(0, ft_memcmp(output, expected_output, sizeof(output)));

    ft_memset(info, 0, sizeof(info));
    result = crypto_hkdf_sha256_extract(ft_nullptr, 0U,
        input_key_material, sizeof(input_key_material), pseudorandom_key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "19ef24a32c717b167f33a91d6f648bdf"
        "96596776afdb6377ac434c1c293ccb04", expected_prk,
        sizeof(expected_prk), &decoded_length));
    FT_ASSERT_EQ(0, ft_memcmp(pseudorandom_key, expected_prk,
        sizeof(expected_prk)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "8da4e775a563c18f715f802a063c5a31"
        "b8a11f5c5ee1879ec3454e5f3c738d2d"
        "9d201395faa4b61a96c8", expected_output,
        sizeof(expected_output), &decoded_length));
    FT_ASSERT_EQ(0, ft_memcmp(output, expected_output, sizeof(output)));
    return (1);
}

FT_TEST(test_crypto_hkdf_sha256_rfc5869_long_known_answer)
{
    uint8_t input_key_material[80];
    uint8_t salt[80];
    uint8_t info[80];
    uint8_t pseudorandom_key[32];
    uint8_t expected_prk[32];
    uint8_t output[82];
    uint8_t expected_output[82];
    ft_size_t decoded_length;
    uint32_t index;

    index = 0U;
    while (index < 80U)
    {
        input_key_material[index] = static_cast<uint8_t>(index);
        salt[index] = static_cast<uint8_t>(0x60U + index);
        info[index] = static_cast<uint8_t>(0xb0U + index);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(salt,
        sizeof(salt), input_key_material, sizeof(input_key_material),
        pseudorandom_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "06a6b88c5853361a06104c9ceb35b45c"
        "ef760014904671014a193f40c15fc244", expected_prk,
        sizeof(expected_prk), &decoded_length));
    FT_ASSERT_EQ(sizeof(expected_prk), decoded_length);
    FT_ASSERT_EQ(0, ft_memcmp(pseudorandom_key, expected_prk,
        sizeof(expected_prk)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
        pseudorandom_key, info, sizeof(info), output, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_test_decode_hex(
        "b11e398dc80327a1c8e7f78c596a4934"
        "4f012eda2d4efad8a050cc4c19afa97c"
        "59045a99cac7827271cb41c65e590e09"
        "da3275600c2f09b8367793a9aca3db71"
        "cc30c58179ec3e87c14c01d5c1f3434f"
        "1d87", expected_output, sizeof(expected_output), &decoded_length));
    FT_ASSERT_EQ(sizeof(expected_output), decoded_length);
    FT_ASSERT_EQ(0, ft_memcmp(output, expected_output, sizeof(output)));
    return (1);
}

FT_TEST(test_crypto_hkdf_sha256_output_length_boundaries)
{
    const ft_size_t output_lengths[7] = {0U, 1U, 31U, 32U, 33U,
        8159U, 8160U};
    uint8_t pseudorandom_key[32];
    uint8_t output[8161];
    uint32_t length_index = 0U;

    ft_memset(pseudorandom_key, 0x5a, sizeof(pseudorandom_key));
    while (length_index < 7U)
    {
        ft_memset(output, 0xa5, sizeof(output));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
            pseudorandom_key, ft_nullptr, 0U, output,
            output_lengths[length_index]));
        if (output_lengths[length_index] == 0U)
            FT_ASSERT_EQ(0xa5U, output[0]);
        length_index += 1U;
    }
    ft_memset(output, 0xa5, sizeof(output));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, 8161U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output,
        ~static_cast<ft_size_t>(0U)));
    length_index = 0U;
    while (length_index < sizeof(output))
    {
        FT_ASSERT_EQ(0xa5U, output[length_index]);
        length_index += 1U;
    }
    return (1);
}

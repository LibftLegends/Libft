#include "../test_internal.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_chacha20.hpp"
#include "../../Modules/Crypto/crypto_poly1305.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Crypto/crypto_session.hpp"
#include "../../Modules/Crypto/crypto_random.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"
#include "crypto_test_hooks.hpp"
#include "../../Modules/Networking/openssl_support.hpp"

#if NETWORKING_HAS_OPENSSL
# include <openssl/evp.h>
#endif

static ft_bool crypto_bytes_equal(const uint8_t *actual, const uint8_t *expected,
    ft_size_t length) noexcept
{
    ft_size_t index;

    index = 0U;
    while (index < length)
    {
        if (expected[index] != actual[index])
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

#if NETWORKING_HAS_OPENSSL
static ft_bool crypto_openssl_sha256(const uint8_t *data, ft_size_t length,
    uint8_t digest[32]) noexcept
{
    EVP_MD_CTX *context;
    unsigned int digest_length;
    int result;

    context = EVP_MD_CTX_new();
    if (context == ft_nullptr)
        return (FT_FALSE);
    result = EVP_DigestInit_ex(context, EVP_sha256(), ft_nullptr);
    if (result == 1)
        result = EVP_DigestUpdate(context, data, length);
    digest_length = 0U;
    if (result == 1)
        result = EVP_DigestFinal_ex(context, digest, &digest_length);
    EVP_MD_CTX_free(context);
    if (result != 1 || digest_length != 32U)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool crypto_openssl_hmac_sha256(const uint8_t *key, ft_size_t key_length,
    const uint8_t *data, ft_size_t data_length, uint8_t digest[32]) noexcept
{
    EVP_PKEY *key_handle;
    EVP_MD_CTX *context;
    size_t digest_length;
    int result;

    key_handle = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, ft_nullptr, key,
        static_cast<int>(key_length));
    if (key_handle == ft_nullptr)
        return (FT_FALSE);
    context = EVP_MD_CTX_new();
    if (context == ft_nullptr)
    {
        EVP_PKEY_free(key_handle);
        return (FT_FALSE);
    }
    result = EVP_DigestSignInit(context, ft_nullptr, EVP_sha256(),
        ft_nullptr, key_handle);
    if (result == 1)
        result = EVP_DigestSignUpdate(context, data, data_length);
    digest_length = 32U;
    if (result == 1)
        result = EVP_DigestSignFinal(context, digest, &digest_length);
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key_handle);
    if (result != 1 || digest_length != 32U)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool crypto_openssl_aead(const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t *associated_data, ft_size_t associated_data_length,
    const uint8_t *plaintext, ft_size_t plaintext_length, uint8_t *ciphertext,
    uint8_t tag[16]) noexcept
{
    EVP_CIPHER_CTX *context;
    int output_length;
    int final_length;
    int result;

    context = EVP_CIPHER_CTX_new();
    if (context == ft_nullptr)
        return (FT_FALSE);
    result = EVP_EncryptInit_ex(context, EVP_chacha20_poly1305(), ft_nullptr,
        ft_nullptr, ft_nullptr);
    if (result == 1)
        result = EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_SET_IVLEN, 12,
            ft_nullptr);
    if (result == 1)
        result = EVP_EncryptInit_ex(context, ft_nullptr, ft_nullptr, key, nonce);
    output_length = 0;
    if (result == 1 && associated_data_length != 0U)
        result = EVP_EncryptUpdate(context, ft_nullptr, &output_length,
            associated_data, static_cast<int>(associated_data_length));
    if (result == 1)
        result = EVP_EncryptUpdate(context, ciphertext, &output_length,
            plaintext, static_cast<int>(plaintext_length));
    final_length = 0;
    if (result == 1)
        result = EVP_EncryptFinal_ex(context, ciphertext + output_length,
            &final_length);
    if (result == 1)
        result = EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(context);
    if (result != 1 || final_length != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool crypto_openssl_x25519(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], uint8_t shared_secret[32]) noexcept
{
    EVP_PKEY *private_handle;
    EVP_PKEY *peer_handle;
    EVP_PKEY_CTX *context;
    size_t output_length;
    int result;

    private_handle = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519,
        ft_nullptr, private_key, 32U);
    peer_handle = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519,
        ft_nullptr, peer_public_key, 32U);
    if (private_handle == ft_nullptr || peer_handle == ft_nullptr)
    {
        EVP_PKEY_free(private_handle);
        EVP_PKEY_free(peer_handle);
        return (FT_FALSE);
    }
    context = EVP_PKEY_CTX_new(private_handle, ft_nullptr);
    if (context == ft_nullptr)
    {
        EVP_PKEY_free(private_handle);
        EVP_PKEY_free(peer_handle);
        return (FT_FALSE);
    }
    result = EVP_PKEY_derive_init(context);
    if (result == 1)
        result = EVP_PKEY_derive_set_peer(context, peer_handle);
    output_length = 32U;
    if (result == 1)
        result = EVP_PKEY_derive(context, shared_secret, &output_length);
    EVP_PKEY_CTX_free(context);
    EVP_PKEY_free(private_handle);
    EVP_PKEY_free(peer_handle);
    if (result != 1 || output_length != 32U)
        return (FT_FALSE);
    return (FT_TRUE);
}
#endif

FT_TEST(test_crypto_sha256_empty_and_incremental_vectors)
{
    uint8_t digest[32];
    const uint8_t expected_empty[32] = {
        0xe3U, 0xb0U, 0xc4U, 0x42U, 0x98U, 0xfcU, 0x1cU, 0x14U,
        0x9aU, 0xfbU, 0xf4U, 0xc8U, 0x99U, 0x6fU, 0xb9U, 0x24U,
        0x27U, 0xaeU, 0x41U, 0xe4U, 0x64U, 0x9bU, 0x93U, 0x4cU,
        0xa4U, 0x95U, 0x99U, 0x1bU, 0x78U, 0x52U, 0xb8U, 0x55U
    };
    const uint8_t expected_abc[32] = {
        0xbaU, 0x78U, 0x16U, 0xbfU, 0x8fU, 0x01U, 0xcfU, 0xeaU,
        0x41U, 0x41U, 0x40U, 0xdeU, 0x5dU, 0xaeU, 0x22U, 0x23U,
        0xb0U, 0x03U, 0x61U, 0xa3U, 0x96U, 0x17U, 0x7aU, 0x9cU,
        0xb4U, 0x10U, 0xffU, 0x61U, 0xf2U, 0x00U, 0x15U, 0xadU
    };
    crypto_sha256 context;
    const char abc[] = "abc";

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(ft_nullptr, 0U, digest));
    FT_ASSERT(crypto_bytes_equal(digest, expected_empty, sizeof(digest)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(abc, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(abc + 1, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(abc + 2, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(digest));
    FT_ASSERT(crypto_bytes_equal(digest, expected_abc, sizeof(digest)));
    return (1);
}

FT_TEST(test_crypto_hmac_sha256_rfc4231_vector)
{
    uint8_t key[20];
    uint8_t digest[32];
    const uint8_t message[] = "Hi There";
    const uint8_t expected[32] = {
        0xb0U, 0x34U, 0x4cU, 0x61U, 0xd8U, 0xdbU, 0x38U, 0x53U,
        0x5cU, 0xa8U, 0xafU, 0xceU, 0xafU, 0x0bU, 0xf1U, 0x2bU,
        0x88U, 0x1dU, 0xc2U, 0x00U, 0xc9U, 0x83U, 0x3dU, 0xa7U,
        0x26U, 0xe9U, 0x37U, 0x6cU, 0x2eU, 0x32U, 0xcfU, 0xf7U
    };
    ft_size_t index;

    index = 0U;
    while (index < sizeof(key))
    {
        key[index] = 0x0bU;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(key, sizeof(key), message,
        sizeof(message) - 1U, digest));
    FT_ASSERT(crypto_bytes_equal(digest, expected, sizeof(digest)));
    return (1);
}

FT_TEST(test_crypto_hkdf_sha256_rfc5869_vector)
{
    uint8_t input_key_material[22];
    const uint8_t salt[13] = {
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U,
        0x07U, 0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU
    };
    const uint8_t info[10] = {
        0xf0U, 0xf1U, 0xf2U, 0xf3U, 0xf4U,
        0xf5U, 0xf6U, 0xf7U, 0xf8U, 0xf9U
    };
    const uint8_t expected_prk[32] = {
        0x07U, 0x77U, 0x09U, 0x36U, 0x2cU, 0x2eU, 0x32U, 0xdfU,
        0x0dU, 0xdcU, 0x3fU, 0x0dU, 0xc4U, 0x7bU, 0xbaU, 0x63U,
        0x90U, 0xb6U, 0xc7U, 0x3bU, 0xb5U, 0x0fU, 0x9cU, 0x31U,
        0x22U, 0xecU, 0x84U, 0x4aU, 0xd7U, 0xc2U, 0xb3U, 0xe5U
    };
    const uint8_t expected_okm[42] = {
        0x3cU, 0xb2U, 0x5fU, 0x25U, 0xfaU, 0xacU, 0xd5U, 0x7aU,
        0x90U, 0x43U, 0x4fU, 0x64U, 0xd0U, 0x36U, 0x2fU, 0x2aU,
        0x2dU, 0x2dU, 0x0aU, 0x90U, 0xcfU, 0x1aU, 0x5aU, 0x4cU,
        0x5dU, 0xb0U, 0x2dU, 0x56U, 0xecU, 0xc4U, 0xc5U, 0xbfU,
        0x34U, 0x00U, 0x72U, 0x08U, 0xd5U, 0xb8U, 0x87U, 0x18U,
        0x58U, 0x65U
    };
    uint8_t pseudorandom_key[32];
    uint8_t output[42];
    ft_size_t index;

    index = 0U;
    while (index < sizeof(input_key_material))
    {
        input_key_material[index] = 0x0bU;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(salt,
        sizeof(salt), input_key_material, sizeof(input_key_material),
        pseudorandom_key));
    FT_ASSERT(crypto_bytes_equal(pseudorandom_key, expected_prk,
        sizeof(pseudorandom_key)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(pseudorandom_key,
        info, sizeof(info), output, sizeof(output)));
    FT_ASSERT(crypto_bytes_equal(output, expected_okm, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, 255U * 32U + 1U));
    return (1);
}

FT_TEST(test_crypto_hmac_sha256_rfc4231_remaining_vectors)
{
    const uint8_t case_two_key[] = {'J', 'e', 'f', 'e'};
    const uint8_t case_two_data[] = "what do ya want for nothing?";
    const uint8_t case_two_expected[32] = {
        0x5bU, 0xdcU, 0xc1U, 0x46U, 0xbfU, 0x60U, 0x75U, 0x4eU,
        0x6aU, 0x04U, 0x24U, 0x26U, 0x08U, 0x95U, 0x75U, 0xc7U,
        0x5aU, 0x00U, 0x3fU, 0x08U, 0x9dU, 0x27U, 0x39U, 0x83U,
        0x9dU, 0xecU, 0x58U, 0xb9U, 0x64U, 0xecU, 0x38U, 0x43U
    };
    uint8_t case_three_key[20];
    uint8_t case_three_data[50];
    const uint8_t case_three_expected[32] = {
        0x77U, 0x3eU, 0xa9U, 0x1eU, 0x36U, 0x80U, 0x0eU, 0x46U,
        0x85U, 0x4dU, 0xb8U, 0xebU, 0xd0U, 0x91U, 0x81U, 0xa7U,
        0x29U, 0x59U, 0x09U, 0x8bU, 0x3eU, 0xf8U, 0xc1U, 0x22U,
        0xd9U, 0x63U, 0x55U, 0x14U, 0xceU, 0xd5U, 0x65U, 0xfeU
    };
    uint8_t case_four_key[25];
    uint8_t case_four_data[50];
    const uint8_t case_four_expected[32] = {
        0x82U, 0x55U, 0x8aU, 0x38U, 0x9aU, 0x44U, 0x3cU, 0x0eU,
        0xa4U, 0xccU, 0x81U, 0x98U, 0x99U, 0xf2U, 0x08U, 0x3aU,
        0x85U, 0xf0U, 0xfaU, 0xa3U, 0xe5U, 0x78U, 0xf8U, 0x07U,
        0x7aU, 0x2eU, 0x3fU, 0xf4U, 0x67U, 0x29U, 0x66U, 0x5bU
    };
    uint8_t case_six_key[131];
    const uint8_t case_six_data[] =
        "Test Using Larger Than Block-Size Key - Hash Key First";
    const uint8_t case_six_expected[32] = {
        0x60U, 0xe4U, 0x31U, 0x59U, 0x1eU, 0xe0U, 0xb6U, 0x7fU,
        0x0dU, 0x8aU, 0x26U, 0xaaU, 0xcbU, 0xf5U, 0xb7U, 0x7fU,
        0x8eU, 0x0bU, 0xc6U, 0x21U, 0x37U, 0x28U, 0xc5U, 0x14U,
        0x05U, 0x46U, 0x04U, 0x0fU, 0x0eU, 0xe3U, 0x7fU, 0x54U
    };
    const uint8_t case_seven_data[] =
        "This is a test using a larger than block-size key and a larger "
        "than block-size data. The key needs to be hashed before being "
        "used by the HMAC algorithm.";
    const uint8_t case_seven_expected[32] = {
        0x9bU, 0x09U, 0xffU, 0xa7U, 0x1bU, 0x94U, 0x2fU, 0xcbU,
        0x27U, 0x63U, 0x5fU, 0xbcU, 0xd5U, 0xb0U, 0xe9U, 0x44U,
        0xbfU, 0xdcU, 0x63U, 0x64U, 0x4fU, 0x07U, 0x13U, 0x93U,
        0x8aU, 0x7fU, 0x51U, 0x53U, 0x5cU, 0x3aU, 0x35U, 0xe2U
    };
    uint8_t digest[32];
    ft_size_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(case_two_key,
        sizeof(case_two_key), case_two_data, sizeof(case_two_data) - 1U,
        digest));
    FT_ASSERT(crypto_bytes_equal(digest, case_two_expected, sizeof(digest)));
    ft_memset(case_three_key, 0xaaU, sizeof(case_three_key));
    ft_memset(case_three_data, 0xddU, sizeof(case_three_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(case_three_key,
        sizeof(case_three_key), case_three_data, sizeof(case_three_data),
        digest));
    FT_ASSERT(crypto_bytes_equal(digest, case_three_expected,
        sizeof(digest)));
    index = 0U;
    while (index < sizeof(case_four_key))
    {
        case_four_key[index] = static_cast<uint8_t>(index + 1U);
        index += 1U;
    }
    ft_memset(case_four_data, 0xcdU, sizeof(case_four_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(case_four_key,
        sizeof(case_four_key), case_four_data, sizeof(case_four_data),
        digest));
    FT_ASSERT(crypto_bytes_equal(digest, case_four_expected,
        sizeof(digest)));
    ft_memset(case_six_key, 0xaaU, sizeof(case_six_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(case_six_key,
        sizeof(case_six_key), case_six_data, sizeof(case_six_data) - 1U,
        digest));
    FT_ASSERT(crypto_bytes_equal(digest, case_six_expected, sizeof(digest)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(case_six_key,
        sizeof(case_six_key), case_seven_data, sizeof(case_seven_data) - 1U,
        digest));
    FT_ASSERT(crypto_bytes_equal(digest, case_seven_expected,
        sizeof(digest)));
    return (1);
}

FT_TEST(test_crypto_hkdf_sha256_rfc5869_additional_vectors)
{
    uint8_t case_two_input[80];
    uint8_t case_two_salt[80];
    uint8_t case_two_info[80];
    const uint8_t case_two_expected_prk[32] = {
        0x06U, 0xa6U, 0xb8U, 0x8cU, 0x58U, 0x53U, 0x36U, 0x1aU,
        0x06U, 0x10U, 0x4cU, 0x9cU, 0xebU, 0x35U, 0xb4U, 0x5cU,
        0xefU, 0x76U, 0x00U, 0x14U, 0x90U, 0x46U, 0x71U, 0x01U,
        0x4aU, 0x19U, 0x3fU, 0x40U, 0xc1U, 0x5fU, 0xc2U, 0x44U
    };
    const uint8_t case_two_expected_output[82] = {
        0xb1U, 0x1eU, 0x39U, 0x8dU, 0xc8U, 0x03U, 0x27U, 0xa1U,
        0xc8U, 0xe7U, 0xf7U, 0x8cU, 0x59U, 0x6aU, 0x49U, 0x34U,
        0x4fU, 0x01U, 0x2eU, 0xdaU, 0x2dU, 0x4eU, 0xfaU, 0xd8U,
        0xa0U, 0x50U, 0xccU, 0x4cU, 0x19U, 0xafU, 0xa9U, 0x7cU,
        0x59U, 0x04U, 0x5aU, 0x99U, 0xcaU, 0xc7U, 0x82U, 0x72U,
        0x71U, 0xcbU, 0x41U, 0xc6U, 0x5eU, 0x59U, 0x0eU, 0x09U,
        0xdaU, 0x32U, 0x75U, 0x60U, 0x0cU, 0x2fU, 0x09U, 0xb8U,
        0x36U, 0x77U, 0x93U, 0xa9U, 0xacU, 0xa3U, 0xdbU, 0x71U,
        0xccU, 0x30U, 0xc5U, 0x81U, 0x79U, 0xecU, 0x3eU, 0x87U,
        0xc1U, 0x4cU, 0x01U, 0xd5U, 0xc1U, 0xf3U, 0x43U, 0x4fU,
        0x1dU, 0x87U
    };
    uint8_t case_three_input[22];
    const uint8_t case_three_expected_prk[32] = {
        0x19U, 0xefU, 0x24U, 0xa3U, 0x2cU, 0x71U, 0x7bU, 0x16U,
        0x7fU, 0x33U, 0xa9U, 0x1dU, 0x6fU, 0x64U, 0x8bU, 0xdfU,
        0x96U, 0x59U, 0x67U, 0x76U, 0xafU, 0xdbU, 0x63U, 0x77U,
        0xacU, 0x43U, 0x4cU, 0x1cU, 0x29U, 0x3cU, 0xcbU, 0x04U
    };
    const uint8_t case_three_expected_output[42] = {
        0x8dU, 0xa4U, 0xe7U, 0x75U, 0xa5U, 0x63U, 0xc1U, 0x8fU,
        0x71U, 0x5fU, 0x80U, 0x2aU, 0x06U, 0x3cU, 0x5aU, 0x31U,
        0xb8U, 0xa1U, 0x1fU, 0x5cU, 0x5eU, 0xe1U, 0x87U, 0x9eU,
        0xc3U, 0x45U, 0x4eU, 0x5fU, 0x3cU, 0x73U, 0x8dU, 0x2dU,
        0x9dU, 0x20U, 0x13U, 0x95U, 0xfaU, 0xa4U, 0xb6U, 0x1aU,
        0x96U, 0xc8U
    };
    uint8_t pseudorandom_key[32];
    uint8_t output[82];
    ft_size_t index;

    index = 0U;
    while (index < sizeof(case_two_input))
    {
        case_two_input[index] = static_cast<uint8_t>(index);
        case_two_salt[index] = static_cast<uint8_t>(0x60U + index);
        case_two_info[index] = static_cast<uint8_t>(0xb0U + index);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(case_two_salt,
        sizeof(case_two_salt), case_two_input, sizeof(case_two_input),
        pseudorandom_key));
    FT_ASSERT(crypto_bytes_equal(pseudorandom_key, case_two_expected_prk,
        sizeof(pseudorandom_key)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(pseudorandom_key,
        case_two_info, sizeof(case_two_info), output, sizeof(output)));
    FT_ASSERT(crypto_bytes_equal(output, case_two_expected_output,
        sizeof(output)));
    ft_memset(case_three_input, 0x0bU, sizeof(case_three_input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_extract(ft_nullptr, 0U,
        case_three_input, sizeof(case_three_input), pseudorandom_key));
    FT_ASSERT(crypto_bytes_equal(pseudorandom_key, case_three_expected_prk,
        sizeof(pseudorandom_key)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(pseudorandom_key,
        ft_nullptr, 0U, output, sizeof(case_three_expected_output)));
    FT_ASSERT(crypto_bytes_equal(output, case_three_expected_output,
        sizeof(case_three_expected_output)));
    return (1);
}

FT_TEST(test_crypto_chacha20_and_poly1305_rfc8439_vectors)
{
    const uint8_t key[32] = {
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU,
        0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U,
        0x18U, 0x19U, 0x1aU, 0x1bU, 0x1cU, 0x1dU, 0x1eU, 0x1fU
    };
    const uint8_t nonce[12] = {
        0x00U, 0x00U, 0x00U, 0x09U, 0x00U, 0x00U, 0x00U, 0x4aU,
        0x00U, 0x00U, 0x00U, 0x00U
    };
    const uint8_t expected_block[64] = {
        0x10U, 0xf1U, 0xe7U, 0xe4U, 0xd1U, 0x3bU, 0x59U, 0x15U,
        0x50U, 0x0fU, 0xddU, 0x1fU, 0xa3U, 0x20U, 0x71U, 0xc4U,
        0xc7U, 0xd1U, 0xf4U, 0xc7U, 0x33U, 0xc0U, 0x68U, 0x03U,
        0x04U, 0x22U, 0xaaU, 0x9aU, 0xc3U, 0xd4U, 0x6cU, 0x4eU,
        0xd2U, 0x82U, 0x64U, 0x46U, 0x07U, 0x9fU, 0xaaU, 0x09U,
        0x14U, 0xc2U, 0xd7U, 0x05U, 0xd9U, 0x8bU, 0x02U, 0xa2U,
        0xb5U, 0x12U, 0x9cU, 0xd1U, 0xdeU, 0x16U, 0x4eU, 0xb9U,
        0xcbU, 0xd0U, 0x83U, 0xe8U, 0xa2U, 0x50U, 0x3cU, 0x4eU
    };
    const uint8_t poly_key[32] = {
        0x85U, 0xd6U, 0xbeU, 0x78U, 0x57U, 0x55U, 0x6dU, 0x33U,
        0x7fU, 0x44U, 0x52U, 0xfeU, 0x42U, 0xd5U, 0x06U, 0xa8U,
        0x01U, 0x03U, 0x80U, 0x8aU, 0xfbU, 0x0dU, 0xb2U, 0xfdU,
        0x4aU, 0xbfU, 0xf6U, 0xafU, 0x41U, 0x49U, 0xf5U, 0x1bU
    };
    const uint8_t poly_message[] = "Cryptographic Forum Research Group";
    const uint8_t expected_tag[16] = {
        0xa8U, 0x06U, 0x1dU, 0xc1U, 0x30U, 0x51U, 0x36U, 0xc6U,
        0xc2U, 0x2bU, 0x8bU, 0xafU, 0x0cU, 0x01U, 0x27U, 0xa9U
    };
    uint8_t block[64];
    uint8_t tag[16];

    crypto_chacha20_block(key, 1U, nonce, block);
    FT_ASSERT(crypto_bytes_equal(block, expected_block, sizeof(block)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(tag, poly_message,
        sizeof(poly_message) - 1U, poly_key));
    FT_ASSERT(crypto_bytes_equal(tag, expected_tag, sizeof(tag)));
    return (1);
}

FT_TEST(test_crypto_x25519_rfc7748_key_agreement)
{
    const uint8_t alice_private_key[32] = {
        0x77U, 0x07U, 0x6dU, 0x0aU, 0x73U, 0x18U, 0xa5U, 0x7dU,
        0x3cU, 0x16U, 0xc1U, 0x72U, 0x51U, 0xb2U, 0x66U, 0x45U,
        0xdfU, 0x4cU, 0x2fU, 0x87U, 0xebU, 0xc0U, 0x99U, 0x2aU,
        0xb1U, 0x77U, 0xfbU, 0xa5U, 0x1dU, 0xb9U, 0x2cU, 0x2aU
    };
    const uint8_t bob_private_key[32] = {
        0x5dU, 0xabU, 0x08U, 0x7eU, 0x62U, 0x4aU, 0x8aU, 0x4bU,
        0x79U, 0xe1U, 0x7fU, 0x8bU, 0x83U, 0x80U, 0x0eU, 0xe6U,
        0x6fU, 0x3bU, 0xb1U, 0x29U, 0x26U, 0x18U, 0xb6U, 0xfdU,
        0x1cU, 0x2fU, 0x8bU, 0x27U, 0xffU, 0x88U, 0xe0U, 0xebU
    };
    const uint8_t expected_alice_public_key[32] = {
        0x85U, 0x20U, 0xf0U, 0x09U, 0x89U, 0x30U, 0xa7U, 0x54U,
        0x74U, 0x8bU, 0x7dU, 0xdcU, 0xb4U, 0x3eU, 0xf7U, 0x5aU,
        0x0dU, 0xbfU, 0x3aU, 0x0dU, 0x26U, 0x38U, 0x1aU, 0xf4U,
        0xebU, 0xa4U, 0xa9U, 0x8eU, 0xaaU, 0x9bU, 0x4eU, 0x6aU
    };
    const uint8_t expected_bob_public_key[32] = {
        0xdeU, 0x9eU, 0xdbU, 0x7dU, 0x7bU, 0x7dU, 0xc1U, 0xb4U,
        0xd3U, 0x5bU, 0x61U, 0xc2U, 0xecU, 0xe4U, 0x35U, 0x37U,
        0x3fU, 0x83U, 0x43U, 0xc8U, 0x5bU, 0x78U, 0x67U, 0x4dU,
        0xadU, 0xfcU, 0x7eU, 0x14U, 0x6fU, 0x88U, 0x2bU, 0x4fU
    };
    uint8_t alice_public_key[32];
    uint8_t bob_public_key[32];
    uint8_t alice_shared_secret[32];
    uint8_t bob_shared_secret[32];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(alice_private_key,
        alice_public_key));
    FT_ASSERT(crypto_bytes_equal(alice_public_key,
        expected_alice_public_key, sizeof(alice_public_key)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(bob_private_key,
        bob_public_key));
    FT_ASSERT(crypto_bytes_equal(bob_public_key,
        expected_bob_public_key, sizeof(bob_public_key)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(alice_private_key,
        bob_public_key, alice_shared_secret));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(bob_private_key,
        alice_public_key, bob_shared_secret));
    FT_ASSERT(crypto_bytes_equal(alice_shared_secret, bob_shared_secret,
        sizeof(alice_shared_secret)));
    ft_memset(bob_public_key, 0, sizeof(bob_public_key));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        crypto_x25519_shared_secret(alice_private_key, bob_public_key,
            alice_shared_secret));
    return (1);
}

FT_TEST(test_crypto_x25519_rfc7748_thousand_iterations)
{
    const uint8_t expected[32] = {
        0x68U, 0x4cU, 0xf5U, 0x9bU, 0xa8U, 0x33U, 0x09U, 0x55U,
        0x28U, 0x00U, 0xefU, 0x56U, 0x6fU, 0x2fU, 0x4dU, 0x3cU,
        0x1cU, 0x38U, 0x87U, 0xc4U, 0x93U, 0x60U, 0xe3U, 0x87U,
        0x5fU, 0x2eU, 0xb9U, 0x4dU, 0x99U, 0x53U, 0x2cU, 0x51U
    };
    uint8_t scalar[32] = {9U};
    uint8_t u_coordinate[32] = {9U};
    uint8_t old_scalar[32];
    uint8_t result[32];
    uint32_t iteration;

    iteration = 0U;
    while (iteration < 1000U)
    {
        ft_memcpy(old_scalar, scalar, sizeof(old_scalar));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(
            scalar, u_coordinate, result));
        ft_memcpy(scalar, result, sizeof(scalar));
        ft_memcpy(u_coordinate, old_scalar, sizeof(u_coordinate));
        iteration += 1U;
    }
    FT_ASSERT(crypto_bytes_equal(scalar, expected, sizeof(expected)));
    return (1);
}

FT_TEST(test_crypto_session_derivation_separates_directions)
{
    const uint8_t alice_private_key[32] = {
        0x77U, 0x07U, 0x6dU, 0x0aU, 0x73U, 0x18U, 0xa5U, 0x7dU,
        0x3cU, 0x16U, 0xc1U, 0x72U, 0x51U, 0xb2U, 0x66U, 0x45U,
        0xdfU, 0x4cU, 0x2fU, 0x87U, 0xebU, 0xc0U, 0x99U, 0x2aU,
        0xb1U, 0x77U, 0xfbU, 0xa5U, 0x1dU, 0xb9U, 0x2cU, 0x2aU
    };
    const uint8_t bob_private_key[32] = {
        0x5dU, 0xabU, 0x08U, 0x7eU, 0x62U, 0x4aU, 0x8aU, 0x4bU,
        0x79U, 0xe1U, 0x7fU, 0x8bU, 0x83U, 0x80U, 0x0eU, 0xe6U,
        0x6fU, 0x3bU, 0xb1U, 0x29U, 0x26U, 0x18U, 0xb6U, 0xfdU,
        0x1cU, 0x2fU, 0x8bU, 0x27U, 0xffU, 0x88U, 0xe0U, 0xebU
    };
    const uint8_t transcript[] = "handshake transcript v1";
    uint8_t alice_public_key[32];
    uint8_t bob_public_key[32];
    uint8_t alice_send_key[32];
    uint8_t alice_receive_key[32];
    uint8_t bob_send_key[32];
    uint8_t bob_receive_key[32];
    uint8_t alice_send_iv[12];
    uint8_t alice_receive_iv[12];
    uint8_t bob_send_iv[12];
    uint8_t bob_receive_iv[12];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(alice_private_key,
        alice_public_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(bob_private_key,
        bob_public_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(alice_private_key,
        bob_public_key, transcript, sizeof(transcript) - 1U,
        crypto_session_role::CLIENT, alice_send_key, alice_receive_key,
        alice_send_iv, alice_receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_derive_session_keys(bob_private_key,
        alice_public_key, transcript, sizeof(transcript) - 1U,
        crypto_session_role::SERVER, bob_send_key, bob_receive_key,
        bob_send_iv, bob_receive_iv));
    FT_ASSERT(crypto_bytes_equal(alice_send_key, bob_receive_key,
        sizeof(alice_send_key)));
    FT_ASSERT(crypto_bytes_equal(alice_receive_key, bob_send_key,
        sizeof(alice_receive_key)));
    FT_ASSERT(crypto_bytes_equal(alice_send_iv, bob_receive_iv,
        sizeof(alice_send_iv)));
    FT_ASSERT(crypto_bytes_equal(alice_receive_iv, bob_send_iv,
        sizeof(alice_receive_iv)));
    FT_ASSERT(!crypto_bytes_equal(alice_send_key, alice_receive_key,
        sizeof(alice_send_key)));
    return (1);
}

FT_TEST(test_crypto_chacha20_poly1305_rfc8439_aead_vector)
{
    const uint8_t key[32] = {
        0x80U, 0x81U, 0x82U, 0x83U, 0x84U, 0x85U, 0x86U, 0x87U,
        0x88U, 0x89U, 0x8aU, 0x8bU, 0x8cU, 0x8dU, 0x8eU, 0x8fU,
        0x90U, 0x91U, 0x92U, 0x93U, 0x94U, 0x95U, 0x96U, 0x97U,
        0x98U, 0x99U, 0x9aU, 0x9bU, 0x9cU, 0x9dU, 0x9eU, 0x9fU
    };
    const uint8_t nonce[12] = {
        0x07U, 0x00U, 0x00U, 0x00U, 0x40U, 0x41U, 0x42U, 0x43U,
        0x44U, 0x45U, 0x46U, 0x47U
    };
    const uint8_t associated_data[12] = {
        0x50U, 0x51U, 0x52U, 0x53U, 0xc0U, 0xc1U, 0xc2U, 0xc3U,
        0xc4U, 0xc5U, 0xc6U, 0xc7U
    };
    const uint8_t plaintext[] =
        "Ladies and Gentlemen of the class of '99: If I could offer you only "
        "one tip for the future, sunscreen would be it.";
    const uint8_t expected_ciphertext[114] = {
        0xd3U, 0x1aU, 0x8dU, 0x34U, 0x64U, 0x8eU, 0x60U, 0xdbU,
        0x7bU, 0x86U, 0xafU, 0xbcU, 0x53U, 0xefU, 0x7eU, 0xc2U,
        0xa4U, 0xadU, 0xedU, 0x51U, 0x29U, 0x6eU, 0x08U, 0xfeU,
        0xa9U, 0xe2U, 0xb5U, 0xa7U, 0x36U, 0xeeU, 0x62U, 0xd6U,
        0x3dU, 0xbeU, 0xa4U, 0x5eU, 0x8cU, 0xa9U, 0x67U, 0x12U,
        0x82U, 0xfaU, 0xfbU, 0x69U, 0xdaU, 0x92U, 0x72U, 0x8bU,
        0x1aU, 0x71U, 0xdeU, 0x0aU, 0x9eU, 0x06U, 0x0bU, 0x29U,
        0x05U, 0xd6U, 0xa5U, 0xb6U, 0x7eU, 0xcdU, 0x3bU, 0x36U,
        0x92U, 0xddU, 0xbdU, 0x7fU, 0x2dU, 0x77U, 0x8bU, 0x8cU,
        0x98U, 0x03U, 0xaeU, 0xe3U, 0x28U, 0x09U, 0x1bU, 0x58U,
        0xfaU, 0xb3U, 0x24U, 0xe4U, 0xfaU, 0xd6U, 0x75U, 0x94U,
        0x55U, 0x85U, 0x80U, 0x8bU, 0x48U, 0x31U, 0xd7U, 0xbcU,
        0x3fU, 0xf4U, 0xdeU, 0xf0U, 0x8eU, 0x4bU, 0x7aU, 0x9dU,
        0xe5U, 0x76U, 0xd2U, 0x65U, 0x86U, 0xceU, 0xc6U, 0x4bU,
        0x61U, 0x16U
    };
    const uint8_t expected_tag[16] = {
        0x1aU, 0xe1U, 0x0bU, 0x59U, 0x4fU, 0x09U, 0xe2U, 0x6aU,
        0x7eU, 0x90U, 0x2eU, 0xcbU, 0xd0U, 0x60U, 0x06U, 0x91U
    };
    uint8_t tag[16];
    uint8_t bad_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> recovered;
    ft_size_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext) - 1U, ciphertext, tag));
    FT_ASSERT(crypto_bytes_equal(&ciphertext[0], expected_ciphertext,
        sizeof(expected_ciphertext)));
    FT_ASSERT(crypto_bytes_equal(tag, expected_tag, sizeof(tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_open(key, nonce,
        associated_data, sizeof(associated_data), &ciphertext[0],
        ciphertext.size(), tag, recovered));
    FT_ASSERT_EQ(sizeof(plaintext) - 1U, recovered.size());
    FT_ASSERT(crypto_bytes_equal(&recovered[0], plaintext, recovered.size()));
    index = 0U;
    while (index < sizeof(bad_tag))
    {
        bad_tag[index] = tag[index];
        index += 1U;
    }
    bad_tag[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(key,
        nonce, associated_data, sizeof(associated_data), &ciphertext[0],
        ciphertext.size(), bad_tag, recovered));
    FT_ASSERT_EQ(0U, recovered.size());
    (void)ciphertext.destroy();
    (void)recovered.destroy();
    return (1);
}

FT_TEST(test_crypto_random_test_provider_is_repeatable)
{
    uint8_t first[32];
    uint8_t second[32];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_random_bytes(first, sizeof(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_random_bytes(second, sizeof(second)));
    FT_ASSERT(crypto_bytes_equal(first, second, sizeof(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}

FT_TEST(test_crypto_random_test_provider_failure_is_reported)
{
    uint8_t output[16];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0xabcdef01U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_fail_next());
    FT_ASSERT_EQ(FT_ERR_IO, crypto_random_bytes(output, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_random_bytes(output, sizeof(output)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}

FT_TEST(test_crypto_aead_allocation_failure_clears_output)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {0U};
    const uint8_t plaintext[64] = {0U};
    uint8_t tag[16] = {0U};
    ft_vector<uint8_t> ciphertext;
    test_cma_failure_controller controller;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, crypto_chacha20_poly1305_seal(key, nonce,
        ft_nullptr, 0U, plaintext, sizeof(plaintext), ciphertext, tag));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, ciphertext.is_initialised());
    FT_ASSERT_EQ(0U, ciphertext.size());
    (void)ciphertext.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    return (1);
}

FT_TEST(test_crypto_aead_authentication_allocation_failure_is_transactional)
{
    const uint8_t key[32] = {0U};
    const uint8_t nonce[12] = {0U};
    const uint8_t plaintext[64] = {0U};
    uint8_t tag[16];
    ft_vector<uint8_t> ciphertext;
    test_cma_failure_controller controller;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.push_back(0x5aU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_after(controller,
            TEST_CMA_FAILURE_ALLOCATE, 1U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, crypto_chacha20_poly1305_seal(key, nonce,
        ft_nullptr, 0U, plaintext, sizeof(plaintext), ciphertext, tag));
    FT_ASSERT_EQ(0U, ciphertext.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_aead_allocation_failure_sweep_is_transactional)
{
    const uint8_t key[32] = {1U};
    const uint8_t nonce[12] = {2U};
    const uint8_t plaintext[96] = {3U};
    ft_size_t failure_after;

    failure_after = 0U;
    while (failure_after < 8U)
    {
        uint8_t tag[16];
        uint8_t zero_tag[16] = {0U};
        ft_vector<uint8_t> ciphertext;
        test_cma_failure_controller controller;
        int32_t result;

        ft_memset(tag, 0xa5U, sizeof(tag));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            test_cma_failure_controller_initialize(controller));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            test_cma_failure_controller_begin(controller));
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            test_cma_failure_controller_fail_after(controller,
                TEST_CMA_FAILURE_ALLOCATE, failure_after));
        result = crypto_chacha20_poly1305_seal(key, nonce, ft_nullptr, 0U,
            plaintext, sizeof(plaintext), ciphertext, tag);
        if (result == FT_ERR_NO_MEMORY)
        {
            FT_ASSERT_EQ(0U, ciphertext.size());
            FT_ASSERT_EQ(0, ft_memcmp(tag, zero_tag, sizeof(tag)));
        }
        else
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
            FT_ASSERT_EQ(sizeof(plaintext), ciphertext.size());
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            test_cma_failure_controller_end(controller));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
        failure_after += 1U;
    }
    return (1);
}

#if NETWORKING_HAS_OPENSSL
FT_TEST(test_crypto_primitives_differential_against_openssl)
{
    const uint8_t key[32] = {
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU,
        0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U,
        0x18U, 0x19U, 0x1aU, 0x1bU, 0x1cU, 0x1dU, 0x1eU, 0x1fU
    };
    const uint8_t nonce[12] = {
        0x30U, 0x31U, 0x32U, 0x33U, 0x34U, 0x35U,
        0x36U, 0x37U, 0x38U, 0x39U, 0x3aU, 0x3bU
    };
    const uint8_t associated_data[9] = {
        0xa0U, 0xa1U, 0xa2U, 0xa3U, 0xa4U, 0xa5U, 0xa6U, 0xa7U, 0xa8U
    };
    const uint8_t plaintext[37] = {
        0x40U, 0x41U, 0x42U, 0x43U, 0x44U, 0x45U, 0x46U, 0x47U,
        0x48U, 0x49U, 0x4aU, 0x4bU, 0x4cU, 0x4dU, 0x4eU, 0x4fU,
        0x50U, 0x51U, 0x52U, 0x53U, 0x54U, 0x55U, 0x56U, 0x57U,
        0x58U, 0x59U, 0x5aU, 0x5bU, 0x5cU, 0x5dU, 0x5eU, 0x5fU,
        0x60U, 0x61U, 0x62U, 0x63U, 0x64U
    };
    const uint8_t x25519_private_key[32] = {
        0x77U, 0x07U, 0x6dU, 0x0aU, 0x73U, 0x18U, 0xa5U, 0x7dU,
        0x3cU, 0x16U, 0xc1U, 0x72U, 0x51U, 0xb2U, 0x66U, 0x45U,
        0xdfU, 0x4cU, 0x2fU, 0x87U, 0xebU, 0xc0U, 0x99U, 0x2aU,
        0xb1U, 0x77U, 0xfbU, 0xa5U, 0x1dU, 0xb9U, 0x2cU, 0x2aU
    };
    const uint8_t x25519_peer_public_key[32] = {
        0xdeU, 0x9eU, 0xdbU, 0x7dU, 0x7bU, 0x7dU, 0xc1U, 0xb4U,
        0xd3U, 0x5bU, 0x61U, 0xc2U, 0xecU, 0xe4U, 0x35U, 0x37U,
        0x3fU, 0x83U, 0x43U, 0xc8U, 0x5bU, 0x78U, 0x67U, 0x4dU,
        0xadU, 0xfcU, 0x7eU, 0x14U, 0x6fU, 0x88U, 0x2bU, 0x4fU
    };
    uint8_t expected_sha[32];
    uint8_t actual_sha[32];
    uint8_t expected_hmac[32];
    uint8_t actual_hmac[32];
    uint8_t expected_ciphertext[sizeof(plaintext)];
    uint8_t expected_tag[16];
    uint8_t actual_tag[16];
    uint8_t expected_shared_secret[32];
    uint8_t actual_shared_secret[32];
    ft_vector<uint8_t> actual_ciphertext;
    ft_size_t index;

    FT_ASSERT(crypto_openssl_sha256(plaintext, sizeof(plaintext), expected_sha));
    FT_ASSERT(crypto_openssl_hmac_sha256(key, sizeof(key), plaintext,
        sizeof(plaintext), expected_hmac));
    FT_ASSERT(crypto_openssl_aead(key, nonce, associated_data,
        sizeof(associated_data), plaintext, sizeof(plaintext),
        expected_ciphertext, expected_tag));
    FT_ASSERT(crypto_openssl_x25519(x25519_private_key,
        x25519_peer_public_key, expected_shared_secret));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(plaintext, sizeof(plaintext),
        actual_sha));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hmac_sha256(key, sizeof(key), plaintext,
        sizeof(plaintext), actual_hmac));
    FT_ASSERT(crypto_bytes_equal(actual_sha, expected_sha, sizeof(actual_sha)));
    FT_ASSERT(crypto_bytes_equal(actual_hmac, expected_hmac, sizeof(actual_hmac)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(
        x25519_private_key, x25519_peer_public_key, actual_shared_secret));
    FT_ASSERT(crypto_bytes_equal(actual_shared_secret, expected_shared_secret,
        sizeof(actual_shared_secret)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext, sizeof(plaintext),
        actual_ciphertext, actual_tag));
    FT_ASSERT_EQ(sizeof(plaintext), actual_ciphertext.size());
    index = 0U;
    while (index < sizeof(plaintext))
    {
        FT_ASSERT_EQ(static_cast<int32_t>(expected_ciphertext[index]),
            static_cast<int32_t>(actual_ciphertext[index]));
        index += 1U;
    }
    FT_ASSERT(crypto_bytes_equal(actual_tag, expected_tag, sizeof(actual_tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual_ciphertext.destroy());
    return (1);
}
#endif

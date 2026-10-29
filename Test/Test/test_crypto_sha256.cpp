#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Errno/errno.hpp"

static ft_bool crypto_sha256_test_equal(const uint8_t *left,
    const uint8_t *right, ft_size_t length) noexcept
{
    ft_size_t index;

    index = 0U;
    while (index < length)
    {
        if (left[index] != right[index])
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

FT_TEST(test_crypto_sha256_empty_and_abc_known_answers)
{
    static const uint8_t empty_expected[32] = {
        0xe3U, 0xb0U, 0xc4U, 0x42U, 0x98U, 0xfcU, 0x1cU, 0x14U,
        0x9aU, 0xfbU, 0xf4U, 0xc8U, 0x99U, 0x6fU, 0xb9U, 0x24U,
        0x27U, 0xaeU, 0x41U, 0xe4U, 0x64U, 0x9bU, 0x93U, 0x4cU,
        0xa4U, 0x95U, 0x99U, 0x1bU, 0x78U, 0x52U, 0xb8U, 0x55U
    };
    static const uint8_t abc_expected[32] = {
        0xbaU, 0x78U, 0x16U, 0xbfU, 0x8fU, 0x01U, 0xcfU, 0xeaU,
        0x41U, 0x41U, 0x40U, 0xdeU, 0x5dU, 0xaeU, 0x22U, 0x23U,
        0xb0U, 0x03U, 0x61U, 0xa3U, 0x96U, 0x17U, 0x7aU, 0x9cU,
        0xb4U, 0x10U, 0xffU, 0x61U, 0xf2U, 0x00U, 0x15U, 0xadU
    };
    uint8_t digest[32];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(ft_nullptr, 0U, digest));
    FT_ASSERT(crypto_sha256_test_equal(digest, empty_expected, 32U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash("abc", 3U, digest));
    FT_ASSERT(crypto_sha256_test_equal(digest, abc_expected, 32U));
    return (1);
}

FT_TEST(test_crypto_sha256_rfc6234_multiblock_known_answer)
{
    const uint8_t message[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const uint8_t expected[32] = {
        0x24U, 0x8dU, 0x6aU, 0x61U, 0xd2U, 0x06U, 0x38U, 0xb8U,
        0xe5U, 0xc0U, 0x26U, 0x93U, 0x0cU, 0x3eU, 0x60U, 0x39U,
        0xa3U, 0x3cU, 0xe4U, 0x59U, 0x64U, 0xffU, 0x21U, 0x67U,
        0xf6U, 0xecU, 0xedU, 0xd4U, 0x19U, 0xdbU, 0x06U, 0xc1U
    };
    uint8_t digest[32];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(message,
        sizeof(message) - 1U, digest));
    FT_ASSERT(crypto_sha256_test_equal(digest, expected, sizeof(digest)));
    return (1);
}

FT_TEST(test_crypto_sha256_rfc6234_one_million_a_known_answer)
{
    const uint8_t expected[32] = {
        0xcdU, 0xc7U, 0x6eU, 0x5cU, 0x99U, 0x14U, 0xfbU, 0x92U,
        0x81U, 0xa1U, 0xc7U, 0xe2U, 0x84U, 0xd7U, 0x3eU, 0x67U,
        0xf1U, 0x80U, 0x9aU, 0x48U, 0xa4U, 0x97U, 0x20U, 0x0eU,
        0x04U, 0x6dU, 0x39U, 0xccU, 0xc7U, 0x11U, 0x2cU, 0xd0U
    };
    uint8_t block[1000];
    uint8_t digest[32];
    crypto_sha256 context;
    uint32_t block_index;

    ft_memset(block, 'a', sizeof(block));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    block_index = 0U;
    while (block_index < 1000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(block, sizeof(block)));
        block_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(digest));
    FT_ASSERT(crypto_sha256_test_equal(digest, expected, sizeof(expected)));
    FT_ASSERT_EQ(FT_TRUE, context.test_state_is_wiped());
    return (1);
}

FT_TEST(test_crypto_sha256_incremental_splits_match_one_shot)
{
    uint8_t message[129];
    uint8_t expected[32];
    uint8_t actual[32];
    crypto_sha256 context;
    ft_size_t lengths[9] = {1U, 55U, 56U, 63U, 64U, 65U, 127U, 128U, 129U};
    uint32_t length_index;
    ft_size_t split;
    ft_size_t index;

    index = 0U;
    while (index < sizeof(message))
    {
        message[index] = static_cast<uint8_t>(index * 29U + 3U);
        index += 1U;
    }
    length_index = 0U;
    while (length_index < 9U)
    {
        index = lengths[length_index];
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(message, index,
            expected));
        split = 0U;
        while (split <= index)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(message, split));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(message + split,
                index - split));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(actual));
            FT_ASSERT(crypto_sha256_test_equal(actual, expected, 32U));
            FT_ASSERT_EQ(FT_TRUE, context.test_state_is_wiped());
            FT_ASSERT_EQ(FT_ERR_SUCCESS, context.destroy());
            split += 1U;
        }
        length_index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_sha256_lifecycle_and_invalid_calls)
{
    crypto_sha256 context;
    uint8_t digest[32];

    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.update("x", 1U));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.final(digest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, context.update(ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(ft_nullptr, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(digest));
    FT_ASSERT_EQ(FT_TRUE, context.test_state_is_wiped());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.final(digest));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, context.update("x", 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.destroy());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crypto_sha256_hash(ft_nullptr, 1U, digest));
    return (1);
}

FT_TEST(test_crypto_sha256_one_byte_updates_and_length_overflow)
{
    uint8_t message[129];
    uint8_t expected[32];
    uint8_t actual[32];
    crypto_sha256 context;
    ft_size_t index;

    index = 0U;
    while (index < sizeof(message))
    {
        message[index] = static_cast<uint8_t>(index * 11U + 5U);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(message,
        sizeof(message), expected));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    index = 0U;
    while (index < sizeof(message))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, context.update(message + index, 1U));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.final(actual));
    FT_ASSERT(crypto_sha256_test_equal(actual, expected, 32U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, context.update(message,
        0x2000000000000000ULL));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.destroy());
    return (1);
}

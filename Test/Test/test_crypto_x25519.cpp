#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Errno/errno.hpp"

static ft_bool crypto_x25519_equal(const uint8_t *left, const uint8_t *right,
    ft_size_t length) noexcept
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

FT_TEST(test_crypto_x25519_clamping_high_bit_and_input_immutability)
{
    uint8_t first_private[32];
    uint8_t second_private[32];
    uint8_t original_first_private[32];
    uint8_t original_second_private[32];
    uint8_t peer_private[32];
    uint8_t first_public[32];
    uint8_t second_public[32];
    uint8_t peer_public[32];
    uint8_t high_bit_peer[32];
    uint8_t first_shared[32];
    uint8_t second_shared[32];
    uint32_t index = 0U;

    while (index < 32U)
    {
        first_private[index] = static_cast<uint8_t>(index * 7U + 11U);
        second_private[index] = first_private[index];
        peer_private[index] = static_cast<uint8_t>(index * 13U + 5U);
        index += 1U;
    }
    second_private[0] ^= 0x07U;
    second_private[31] ^= 0xc0U;
    ft_memcpy(original_first_private, first_private,
        sizeof(original_first_private));
    ft_memcpy(original_second_private, second_private,
        sizeof(original_second_private));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(first_private,
        first_public));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(second_private,
        second_public));
    FT_ASSERT(crypto_x25519_equal(first_public, second_public, 32U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(peer_private,
        peer_public));
    ft_memcpy(high_bit_peer, peer_public, sizeof(high_bit_peer));
    high_bit_peer[31] |= 0x80U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(first_private,
        peer_public, first_shared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(first_private,
        high_bit_peer, second_shared));
    FT_ASSERT(crypto_x25519_equal(first_shared, second_shared, 32U));
    FT_ASSERT(crypto_x25519_equal(first_private, original_first_private, 32U));
    FT_ASSERT(crypto_x25519_equal(second_private, original_second_private, 32U));
    return (1);
}

FT_TEST(test_crypto_x25519_rfc7748_noncanonical_u_coordinates)
{
    uint8_t private_key[32];
    uint8_t canonical_point[32];
    uint8_t noncanonical_point[32];
    uint8_t canonical_secret[32];
    uint8_t noncanonical_secret[32];
    uint32_t index;
    uint32_t value;

    index = 0U;
    while (index < 32U)
    {
        private_key[index] = static_cast<uint8_t>(index * 31U + 7U);
        index += 1U;
    }
    value = 0U;
    while (value < 19U)
    {
        ft_memset(canonical_point, 0, sizeof(canonical_point));
        ft_memset(noncanonical_point, 0xff, sizeof(noncanonical_point));
        canonical_point[0] = static_cast<uint8_t>(value);
        noncanonical_point[0] = static_cast<uint8_t>(0xedU + value);
        noncanonical_point[31] = 0x7fU;
        if (value < 2U)
        {
            FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
                crypto_x25519_shared_secret(private_key, canonical_point,
                    canonical_secret));
            FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
                crypto_x25519_shared_secret(private_key, noncanonical_point,
                    noncanonical_secret));
        }
        else
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS,
                crypto_x25519_shared_secret(private_key, canonical_point,
                    canonical_secret));
            FT_ASSERT_EQ(FT_ERR_SUCCESS,
                crypto_x25519_shared_secret(private_key, noncanonical_point,
                    noncanonical_secret));
            FT_ASSERT(crypto_x25519_equal(canonical_secret,
                noncanonical_secret, sizeof(canonical_secret)));
        }
        value += 1U;
    }
    ft_memset(noncanonical_point, 0xff, sizeof(noncanonical_point));
    noncanonical_point[0] = 0xf6U;
    noncanonical_point[31] = 0xffU;
    ft_memset(canonical_point, 0, sizeof(canonical_point));
    canonical_point[0] = 9U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(private_key,
        canonical_point, canonical_secret));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(private_key,
        noncanonical_point, noncanonical_secret));
    FT_ASSERT(crypto_x25519_equal(canonical_secret, noncanonical_secret,
        sizeof(canonical_secret)));
    return (1);
}

FT_TEST(test_crypto_x25519_exact_input_output_aliasing)
{
    uint8_t private_key[32];
    uint8_t peer_private_key[32];
    uint8_t peer_public_key[32];
    uint8_t expected_public_key[32];
    uint8_t expected_shared_secret[32];
    uint8_t scalar_alias[32];
    uint8_t point_alias[32];
    uint32_t index;

    index = 0U;
    while (index < 32U)
    {
        private_key[index] = static_cast<uint8_t>(index * 17U + 3U);
        peer_private_key[index] = static_cast<uint8_t>(index * 29U + 11U);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(private_key,
        expected_public_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(peer_private_key,
        peer_public_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(private_key,
        peer_public_key, expected_shared_secret));

    ft_memcpy(scalar_alias, private_key, sizeof(scalar_alias));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(scalar_alias,
        scalar_alias));
    FT_ASSERT(crypto_x25519_equal(scalar_alias, expected_public_key,
        sizeof(scalar_alias)));

    ft_memcpy(scalar_alias, private_key, sizeof(scalar_alias));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(scalar_alias,
        peer_public_key, scalar_alias));
    FT_ASSERT(crypto_x25519_equal(scalar_alias, expected_shared_secret,
        sizeof(scalar_alias)));

    ft_memcpy(point_alias, peer_public_key, sizeof(point_alias));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(private_key,
        point_alias, point_alias));
    FT_ASSERT(crypto_x25519_equal(point_alias, expected_shared_secret,
        sizeof(point_alias)));
    return (1);
}

FT_TEST(test_crypto_x25519_rfc7748_iteration_vectors)
{
    const uint8_t expected_one_iteration[32] = {
        0x42U, 0x2cU, 0x8eU, 0x7aU, 0x62U, 0x27U, 0xd7U, 0xbcU,
        0xa1U, 0x35U, 0x0bU, 0x3eU, 0x2bU, 0xb7U, 0x27U, 0x9fU,
        0x78U, 0x97U, 0xb8U, 0x7bU, 0xb6U, 0x85U, 0x4bU, 0x78U,
        0x3cU, 0x60U, 0xe8U, 0x03U, 0x11U, 0xaeU, 0x30U, 0x79U
    };
    const uint8_t expected_thousand_iterations[32] = {
        0x68U, 0x4cU, 0xf5U, 0x9bU, 0xa8U, 0x33U, 0x09U, 0x55U,
        0x28U, 0x00U, 0xefU, 0x56U, 0x6fU, 0x2fU, 0x4dU, 0x3cU,
        0x1cU, 0x38U, 0x87U, 0xc4U, 0x93U, 0x60U, 0xe3U, 0x87U,
        0x5fU, 0x2eU, 0xb9U, 0x4dU, 0x99U, 0x53U, 0x2cU, 0x51U
    };
    uint8_t scalar[32];
    uint8_t coordinate[32];
    uint8_t previous_scalar[32];
    uint8_t next_scalar[32];
    uint32_t iteration;

    ft_memset(scalar, 0, sizeof(scalar));
    ft_memset(coordinate, 0, sizeof(coordinate));
    scalar[0] = 9U;
    coordinate[0] = 9U;
    iteration = 0U;
    while (iteration < 1000U)
    {
        ft_memcpy(previous_scalar, scalar, sizeof(previous_scalar));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(scalar,
            coordinate, next_scalar));
        ft_memcpy(coordinate, previous_scalar, sizeof(coordinate));
        ft_memcpy(scalar, next_scalar, sizeof(scalar));
        iteration += 1U;
        if (iteration == 1U)
            FT_ASSERT(crypto_x25519_equal(scalar,
                expected_one_iteration, sizeof(scalar)));
    }
    FT_ASSERT(crypto_x25519_equal(scalar, expected_thousand_iterations,
        sizeof(scalar)));
    return (1);
}

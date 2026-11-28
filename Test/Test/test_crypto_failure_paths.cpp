#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Crypto/crypto_random.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_test_hooks.hpp"
#include "../../Modules/Crypto/crypto_session.hpp"
#include "../../Modules/Errno/errno.hpp"

class crypto_failure_test_random_guard
{
    public:
        crypto_failure_test_random_guard() noexcept
        {
            return ;
        }

        ~crypto_failure_test_random_guard() noexcept
        {
            (void)crypto_test_random_clear();
            return ;
        }
};

FT_TEST(test_crypto_random_failure_clears_partial_output)
{
    uint8_t output[32];
    ft_size_t index = 0U;
    crypto_failure_test_random_guard random_guard;

    while (index < sizeof(output))
    {
        output[index] = 0xa5U;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0xabcddcbaU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_fail_next());
    FT_ASSERT_EQ(FT_ERR_IO, crypto_random_bytes(output, sizeof(output)));
    index = 0U;
    while (index < sizeof(output))
    {
        FT_ASSERT_EQ(0U, output[index]);
        index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_aead_clears_outputs_on_rejection)
{
    const uint8_t key[32] = {1U};
    const uint8_t nonce[12] = {2U};
    const uint8_t plaintext[3] = {3U, 4U, 5U};
    uint8_t tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> opened;
    uint32_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.push_back(0xeeU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        ft_nullptr, 0U, plaintext, sizeof(plaintext), ciphertext, tag));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.push_back(0xefU));
    tag[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, crypto_chacha20_poly1305_open(key,
        nonce, ft_nullptr, 0U, &ciphertext[0], ciphertext.size(), tag, opened));
    FT_ASSERT_EQ(0U, opened.size());
    index = 0U;
    while (index < sizeof(tag))
    {
        tag[index] = 0xa5U;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_chacha20_poly1305_seal(
        key, nonce, ft_nullptr, 0U, ft_nullptr, 1U, ciphertext, tag));
    index = 0U;
    while (index < sizeof(tag))
    {
        FT_ASSERT_EQ(0U, tag[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(0U, ciphertext.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, opened.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_hkdf_maximum_output_boundary)
{
    uint8_t pseudorandom_key[32] = {0U};
    uint8_t output[8160];
    ft_size_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, 8159U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, sizeof(output)));
    index = 0U;
    while (index < sizeof(output))
    {
        output[index] = 0xa5U;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output, sizeof(output) + 1U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, crypto_hkdf_sha256_expand(
        pseudorandom_key, ft_nullptr, 0U, output,
        static_cast<ft_size_t>(-1)));
    index = 0U;
    while (index < sizeof(output))
    {
        FT_ASSERT_EQ(0xa5U, output[index]);
        index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_secure_wipe_clears_complete_buffer)
{
    uint8_t buffer[257];
    ft_size_t index = 0U;

    while (index < sizeof(buffer))
    {
        buffer[index] = static_cast<uint8_t>(index * 13U + 1U);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_secure_wipe(buffer, sizeof(buffer)));
    index = 0U;
    while (index < sizeof(buffer))
    {
        FT_ASSERT_EQ(0U, buffer[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_secure_wipe(ft_nullptr, 1U));
    return (1);
}

FT_TEST(test_crypto_hash_hmac_and_session_failures_clear_outputs)
{
    uint8_t digest[32];
    uint8_t send_key[32];
    uint8_t receive_key[32];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];
    uint8_t private_key[32] = {1U};
    uint8_t peer_public_key[32] = {2U};
    uint8_t transcript[1] = {3U};
    uint32_t index;

    ft_memset(digest, 0xa5, sizeof(digest));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crypto_sha256_hash(ft_nullptr, 1U, digest));
    index = 0U;
    while (index < sizeof(digest))
    {
        FT_ASSERT_EQ(0U, digest[index]);
        digest[index] = 0xa5U;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_hmac_sha256(ft_nullptr,
        1U, transcript, sizeof(transcript), digest));
    index = 0U;
    while (index < sizeof(digest))
    {
        FT_ASSERT_EQ(0U, digest[index]);
        index += 1U;
    }
    ft_memset(send_key, 0xa5, sizeof(send_key));
    ft_memset(receive_key, 0xa5, sizeof(receive_key));
    ft_memset(send_iv, 0xa5, sizeof(send_iv));
    ft_memset(receive_iv, 0xa5, sizeof(receive_iv));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_derive_session_keys(
        private_key, peer_public_key, transcript, sizeof(transcript),
        static_cast<crypto_session_role>(2U), send_key, receive_key,
        send_iv, receive_iv));
    index = 0U;
    while (index < sizeof(send_key))
    {
        FT_ASSERT_EQ(0U, send_key[index]);
        FT_ASSERT_EQ(0U, receive_key[index]);
        if (index < sizeof(send_iv))
        {
            FT_ASSERT_EQ(0U, send_iv[index]);
            FT_ASSERT_EQ(0U, receive_iv[index]);
        }
        index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_session_rejects_overlapping_buffers_without_mutation)
{
    uint8_t private_key[32] = {1U};
    uint8_t private_key_before[32];
    uint8_t peer_public_key[32] = {2U};
    uint8_t transcript[4] = {3U, 4U, 5U, 6U};
    uint8_t overlapping_keys[64];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];
    uint32_t index;

    ft_memcpy(private_key_before, private_key, sizeof(private_key));
    ft_memset(overlapping_keys, 0xa5, sizeof(overlapping_keys));
    ft_memset(send_iv, 0x5a, sizeof(send_iv));
    ft_memset(receive_iv, 0x5a, sizeof(receive_iv));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_derive_session_keys(
        private_key, peer_public_key, transcript, sizeof(transcript),
        crypto_session_role::CLIENT, overlapping_keys,
        overlapping_keys + 16U, send_iv, receive_iv));
    index = 0U;
    while (index < sizeof(overlapping_keys))
    {
        FT_ASSERT_EQ(0xa5U, overlapping_keys[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_derive_session_keys(
        private_key, peer_public_key, transcript, sizeof(transcript),
        crypto_session_role::CLIENT, private_key, overlapping_keys,
        send_iv, receive_iv));
    FT_ASSERT_EQ(0, ft_memcmp(private_key_before, private_key,
        sizeof(private_key)));
    index = 0U;
    while (index < sizeof(send_iv))
    {
        FT_ASSERT_EQ(0x5aU, send_iv[index]);
        FT_ASSERT_EQ(0x5aU, receive_iv[index]);
        index += 1U;
    }
    return (1);
}

#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Crypto/crypto_chacha20.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_poly1305.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Networking/networking_secure_channel.hpp"
#include "../../Modules/Networking/openssl_support.hpp"
#include "../../Modules/Errno/errno.hpp"

#ifdef LIBFT_TEST_HAVE_SODIUM
# include <sodium.h>

FT_TEST(test_crypto_sha256_differential_matrix_matches_libsodium)
{
    const ft_size_t lengths[9] = {0U, 1U, 55U, 56U, 63U, 64U, 65U, 128U,
        257U};
    uint8_t input[257];
    uint8_t expected[crypto_hash_sha256_BYTES];
    uint8_t actual[32];
    uint32_t byte_index = 0U;
    ft_size_t case_index = 0U;

    FT_ASSERT(sodium_init() >= 0);
    while (byte_index < sizeof(input))
    {
        input[byte_index] = static_cast<uint8_t>(byte_index * 29U + 17U);
        byte_index += 1U;
    }
    while (case_index < 9U)
    {
        FT_ASSERT_EQ(0, crypto_hash_sha256(expected, input,
            static_cast<unsigned long long>(lengths[case_index])));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(input,
            lengths[case_index], actual));
        FT_ASSERT_EQ(0, ft_memcmp(expected, actual, sizeof(actual)));
        case_index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_chacha20_deterministic_differential_matrix)
{
    const ft_size_t lengths[10] = {0U, 1U, 63U, 64U, 65U, 127U, 128U,
        129U, 255U, 512U};
    uint8_t key[32];
    uint8_t nonce[12];
    uint8_t input[512];
    uint8_t expected[512];
    ft_vector<uint8_t> actual;
    uint32_t case_index = 0U;

    FT_ASSERT(sodium_init() >= 0);
    while (case_index < 64U)
    {
        uint32_t state = 0x243f6a88U ^ (case_index * 0x9e3779b9U);
        uint32_t byte_index = 0U;
        ft_size_t message_length = lengths[case_index % 10U];
        uint32_t initial_counter = case_index % 7U;

        while (byte_index < sizeof(key))
        {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            key[byte_index] = static_cast<uint8_t>(state);
            byte_index += 1U;
        }
        byte_index = 0U;
        while (byte_index < sizeof(nonce))
        {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            nonce[byte_index] = static_cast<uint8_t>(state);
            byte_index += 1U;
        }
        byte_index = 0U;
        while (byte_index < sizeof(input))
        {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            input[byte_index] = static_cast<uint8_t>(state);
            byte_index += 1U;
        }
        FT_ASSERT_EQ(0, crypto_stream_chacha20_ietf_xor_ic(expected, input,
            static_cast<unsigned long long>(message_length), nonce,
            static_cast<unsigned long long>(initial_counter), key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_xor(key, nonce,
            initial_counter, input, message_length, actual));
        FT_ASSERT_EQ(message_length, actual.size());
        if (message_length != 0U)
            FT_ASSERT_EQ(0, ft_memcmp(&actual[0], expected, message_length));
        actual.clear();
        case_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual.destroy());
    return (1);
}

FT_TEST(test_crypto_poly1305_matches_libsodium_reference)
{
    const ft_size_t lengths[13] = {0U, 1U, 15U, 16U, 17U, 31U, 32U,
        33U, 63U, 64U, 65U, 127U, 128U};
    uint8_t key[crypto_onetimeauth_KEYBYTES];
    uint8_t message[128];
    uint8_t expected_tag[crypto_onetimeauth_BYTES];
    uint8_t actual_tag[crypto_onetimeauth_BYTES];
    uint32_t byte_index = 0U;
    ft_size_t length_index = 0U;

    FT_ASSERT(sodium_init() >= 0);
    while (byte_index < sizeof(key))
    {
        key[byte_index] = static_cast<uint8_t>(byte_index * 31U + 11U);
        byte_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(message))
    {
        message[byte_index] = static_cast<uint8_t>(byte_index * 17U + 3U);
        byte_index += 1U;
    }
    while (length_index < 13U)
    {
        FT_ASSERT_EQ(0, crypto_onetimeauth(expected_tag, message,
            lengths[length_index], key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_poly1305_auth(actual_tag, message,
            lengths[length_index], key));
        FT_ASSERT_EQ(0, ft_memcmp(expected_tag, actual_tag,
            sizeof(actual_tag)));
        length_index += 1U;
    }
    return (1);
}

FT_TEST(test_crypto_aead_matches_libsodium_reference)
{
    const uint8_t key[32] = {0x31U};
    const uint8_t nonce[12] = {0x42U};
    const ft_size_t lengths[10] = {0U, 1U, 15U, 16U, 17U, 63U, 64U,
        65U, 127U, 128U};
    uint8_t associated_data[65];
    uint8_t plaintext[128];
    uint8_t reference_ciphertext[128];
    uint8_t reference_tag[16];
    uint8_t actual_tag[16];
    ft_vector<uint8_t> actual_ciphertext;
    uint32_t byte_index;
    ft_size_t case_index;

    FT_ASSERT(sodium_init() >= 0);
    byte_index = 0U;
    while (byte_index < sizeof(associated_data))
    {
        associated_data[byte_index] = static_cast<uint8_t>(byte_index * 37U
            + 13U);
        byte_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(plaintext))
    {
        plaintext[byte_index] = static_cast<uint8_t>(byte_index * 19U + 7U);
        byte_index += 1U;
    }
    case_index = 0U;
    while (case_index < 10U)
    {
        ft_size_t message_length = lengths[case_index];
        ft_size_t associated_length = lengths[9U - case_index]
            % (sizeof(associated_data) + 1U);

        FT_ASSERT_EQ(0, crypto_aead_chacha20poly1305_ietf_encrypt_detached(
            reference_ciphertext, reference_tag, ft_nullptr, plaintext,
            message_length, associated_data, associated_length, ft_nullptr,
            nonce, key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key,
            nonce, associated_data, associated_length, plaintext,
            message_length, actual_ciphertext, actual_tag));
        FT_ASSERT_EQ(message_length, actual_ciphertext.size());
        if (message_length != 0U)
            FT_ASSERT_EQ(0, ft_memcmp(&actual_ciphertext[0],
                reference_ciphertext, message_length));
        FT_ASSERT_EQ(0, ft_memcmp(actual_tag, reference_tag,
            sizeof(actual_tag)));
        case_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual_ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_x25519_matches_libsodium_reference)
{
    uint8_t private_key[32];
    uint8_t peer_private_key[32];
    uint8_t peer_public_key[32];
    uint8_t reference_public_key[32];
    uint8_t reference_secret[32];
    uint8_t actual_public_key[32];
    uint8_t actual_secret[32];
    uint32_t case_index;

    FT_ASSERT(sodium_init() >= 0);
    case_index = 0U;
    while (case_index < 64U)
    {
        uint32_t byte_index = 0U;
        uint32_t state = 0x6a09e667U ^ (case_index * 0x9e3779b9U);

        while (byte_index < 32U)
        {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            private_key[byte_index] = static_cast<uint8_t>(state);
            state ^= 0x3c6ef372U + byte_index;
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            peer_private_key[byte_index] = static_cast<uint8_t>(state);
            byte_index += 1U;
        }
        FT_ASSERT_EQ(0, crypto_scalarmult_base(reference_public_key,
            peer_private_key));
        ft_memcpy(peer_public_key, reference_public_key,
            sizeof(peer_public_key));
        FT_ASSERT_EQ(0, crypto_scalarmult(reference_secret, private_key,
            peer_public_key));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_public_key(private_key,
            actual_public_key));
        FT_ASSERT_EQ(0, crypto_scalarmult_base(reference_public_key,
            private_key));
        FT_ASSERT_EQ(0, ft_memcmp(reference_public_key, actual_public_key,
            sizeof(actual_public_key)));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(private_key,
            peer_public_key, actual_secret));
        FT_ASSERT_EQ(0, ft_memcmp(reference_secret, actual_secret,
            sizeof(actual_secret)));
        case_index += 1U;
    }
    return (1);
}

FT_TEST(test_networking_secure_channel_matches_crypto_and_libsodium)
{
    const uint8_t key[32] = {0x5dU};
    const uint8_t initialization_vector[12] = {0x27U};
    uint8_t nonce[12];
    uint8_t associated_data[19];
    uint8_t plaintext[73];
    uint8_t reference_tag[16];
    uint8_t standalone_tag[16];
    uint8_t transport_tag[16];
    uint64_t packet_number = 0x0102030405060708ULL;
    uint32_t byte_index;
    uint32_t index;
    uint32_t shift = 56U;
    ft_vector<uint8_t> reference_ciphertext;
    ft_vector<uint8_t> standalone_ciphertext;
    ft_vector<uint8_t> transport_ciphertext;
    networking_secure_channel channel;

    FT_ASSERT(sodium_init() >= 0);
    byte_index = 0U;
    while (byte_index < sizeof(nonce))
    {
        nonce[byte_index] = initialization_vector[byte_index];
        byte_index += 1U;
    }
    index = 4U;
    while (index < 12U)
    {
        nonce[index] = static_cast<uint8_t>(nonce[index]
            ^ ((packet_number >> shift) & 0xffU));
        index += 1U;
        shift -= 8U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(associated_data))
    {
        associated_data[byte_index] = static_cast<uint8_t>(byte_index * 23U
            + 4U);
        byte_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(plaintext))
    {
        plaintext[byte_index] = static_cast<uint8_t>(byte_index * 17U + 3U);
        byte_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext), standalone_ciphertext, standalone_tag));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        reference_ciphertext.resize(sizeof(plaintext)));
    FT_ASSERT_EQ(0, crypto_aead_chacha20poly1305_ietf_encrypt_detached(
        &reference_ciphertext[0],
        reference_tag, ft_nullptr, plaintext, sizeof(plaintext),
        associated_data, sizeof(associated_data), ft_nullptr, nonce, key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_TRUE, channel.seal(packet_number, associated_data,
        sizeof(associated_data), plaintext, sizeof(plaintext),
        transport_ciphertext, transport_tag));
    FT_ASSERT_EQ(sizeof(plaintext), transport_ciphertext.size());
    FT_ASSERT_EQ(0, ft_memcmp(&transport_ciphertext[0],
        &standalone_ciphertext[0], sizeof(plaintext)));
    FT_ASSERT_EQ(0, ft_memcmp(&transport_ciphertext[0],
        &reference_ciphertext[0], sizeof(plaintext)));
    FT_ASSERT_EQ(0, ft_memcmp(transport_tag, standalone_tag,
        sizeof(transport_tag)));
    FT_ASSERT_EQ(0, ft_memcmp(transport_tag, reference_tag,
        sizeof(transport_tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, standalone_ciphertext.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reference_ciphertext.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport_ciphertext.destroy());
    return (1);
}
#endif

#if NETWORKING_HAS_OPENSSL
# include <openssl/evp.h>

FT_TEST(test_crypto_sha256_differential_matrix_matches_openssl)
{
    const ft_size_t lengths[9] = {0U, 1U, 55U, 56U, 63U, 64U, 65U, 128U,
        257U};
    uint8_t input[257];
    uint8_t expected[EVP_MAX_MD_SIZE];
    uint8_t actual[32];
    unsigned int expected_length;
    uint32_t byte_index = 0U;
    ft_size_t case_index = 0U;

    while (byte_index < sizeof(input))
    {
        input[byte_index] = static_cast<uint8_t>(byte_index * 41U + 23U);
        byte_index += 1U;
    }
    while (case_index < 9U)
    {
        expected_length = 0U;
        FT_ASSERT_EQ(1, EVP_Digest(input, lengths[case_index], expected,
            &expected_length, EVP_sha256(), ft_nullptr));
        FT_ASSERT_EQ(32U, expected_length);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_sha256_hash(input,
            lengths[case_index], actual));
        FT_ASSERT_EQ(0, ft_memcmp(expected, actual, sizeof(actual)));
        case_index += 1U;
    }
    return (1);
}

static ft_bool crypto_differential_openssl_x25519(
    const uint8_t private_key[32], const uint8_t peer_public_key[32],
    uint8_t shared_secret[32]) noexcept
{
    EVP_PKEY *private_handle = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519,
        ft_nullptr, private_key, 32U);
    EVP_PKEY *peer_handle = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519,
        ft_nullptr, peer_public_key, 32U);
    EVP_PKEY_CTX *context;
    size_t output_length = 32U;
    int32_t result;

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
    if (result == 1)
        result = EVP_PKEY_derive(context, shared_secret, &output_length);
    EVP_PKEY_CTX_free(context);
    EVP_PKEY_free(private_handle);
    EVP_PKEY_free(peer_handle);
    if (result != 1 || output_length != 32U)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_crypto_aead_matches_openssl_reference)
{
    const uint8_t key[32] = {0x31U};
    const uint8_t nonce[12] = {0x42U};
    uint8_t associated_data[17];
    uint8_t plaintext[65];
    uint8_t expected_ciphertext[sizeof(plaintext) + 16U];
    uint8_t expected_tag[16];
    uint8_t actual_tag[16];
    EVP_CIPHER_CTX *context;
    int32_t written;
    int32_t final_written;
    uint32_t index = 0U;
    ft_vector<uint8_t> actual_ciphertext;

    while (index < sizeof(associated_data))
    {
        associated_data[index] = static_cast<uint8_t>(index * 11U + 5U);
        index = index + 1U;
    }
    index = 0U;
    while (index < sizeof(plaintext))
    {
        plaintext[index] = static_cast<uint8_t>(index * 7U + 9U);
        index = index + 1U;
    }
    context = EVP_CIPHER_CTX_new();
    FT_ASSERT(context != ft_nullptr);
    FT_ASSERT_EQ(1, EVP_EncryptInit_ex(context, EVP_chacha20_poly1305(),
        ft_nullptr, ft_nullptr, ft_nullptr));
    FT_ASSERT_EQ(1, EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_SET_IVLEN,
        static_cast<int32_t>(sizeof(nonce)), ft_nullptr));
    FT_ASSERT_EQ(1, EVP_EncryptInit_ex(context, ft_nullptr, ft_nullptr,
        key, nonce));
    FT_ASSERT_EQ(1, EVP_EncryptUpdate(context, ft_nullptr, &written,
        associated_data, static_cast<int32_t>(sizeof(associated_data))));
    FT_ASSERT_EQ(1, EVP_EncryptUpdate(context, expected_ciphertext, &written,
        plaintext, static_cast<int32_t>(sizeof(plaintext))));
    FT_ASSERT_EQ(static_cast<int32_t>(sizeof(plaintext)), written);
    FT_ASSERT_EQ(1, EVP_EncryptFinal_ex(context,
        expected_ciphertext + written, &final_written));
    FT_ASSERT_EQ(0, final_written);
    FT_ASSERT_EQ(1, EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_GET_TAG,
        static_cast<int32_t>(sizeof(expected_tag)), expected_tag));
    EVP_CIPHER_CTX_free(context);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key, nonce,
        associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext), actual_ciphertext, actual_tag));
    FT_ASSERT_EQ(sizeof(plaintext), actual_ciphertext.size());
    FT_ASSERT_EQ(0, ft_memcmp(&actual_ciphertext[0], expected_ciphertext,
        sizeof(plaintext)));
    FT_ASSERT_EQ(0, ft_memcmp(actual_tag, expected_tag, sizeof(actual_tag)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual_ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_aead_deterministic_length_matrix_matches_openssl)
{
    const uint8_t key[32] = {0x73U};
    const uint8_t nonce[12] = {0x29U};
    const ft_size_t lengths[10] = {0U, 1U, 15U, 16U, 17U, 63U, 64U, 65U,
        127U, 128U};
    uint8_t associated_data[65];
    uint8_t plaintext[128];
    uint8_t reference_ciphertext[128];
    uint8_t reference_tag[16];
    uint8_t actual_tag[16];
    EVP_CIPHER_CTX *context;
    ft_vector<uint8_t> actual_ciphertext;
    ft_size_t case_index;
    uint32_t byte_index;
    int32_t written;
    int32_t final_written;

    byte_index = 0U;
    while (byte_index < sizeof(associated_data))
    {
        associated_data[byte_index] = static_cast<uint8_t>(byte_index * 37U
            + 13U);
        byte_index += 1U;
    }
    byte_index = 0U;
    while (byte_index < sizeof(plaintext))
    {
        plaintext[byte_index] = static_cast<uint8_t>(byte_index * 19U + 7U);
        byte_index += 1U;
    }
    case_index = 0U;
    while (case_index < 10U)
    {
        ft_size_t message_length = lengths[case_index];
        ft_size_t associated_length = lengths[9U - case_index]
            % (sizeof(associated_data) + 1U);
        int32_t associated_length_i32 = static_cast<int32_t>(associated_length);
        int32_t message_length_i32 = static_cast<int32_t>(message_length);

        context = EVP_CIPHER_CTX_new();
        FT_ASSERT(context != ft_nullptr);
        FT_ASSERT_EQ(1, EVP_EncryptInit_ex(context,
            EVP_chacha20_poly1305(), ft_nullptr, ft_nullptr, ft_nullptr));
        FT_ASSERT_EQ(1, EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_SET_IVLEN,
            static_cast<int32_t>(sizeof(nonce)), ft_nullptr));
        FT_ASSERT_EQ(1, EVP_EncryptInit_ex(context, ft_nullptr, ft_nullptr,
            key, nonce));
        FT_ASSERT_EQ(1, EVP_EncryptUpdate(context, ft_nullptr, &written,
            associated_data, associated_length_i32));
        FT_ASSERT_EQ(1, EVP_EncryptUpdate(context, reference_ciphertext,
            &written, plaintext, message_length_i32));
        FT_ASSERT_EQ(message_length_i32, written);
        FT_ASSERT_EQ(1, EVP_EncryptFinal_ex(context,
            reference_ciphertext + written, &final_written));
        FT_ASSERT_EQ(0, final_written);
        FT_ASSERT_EQ(1, EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_AEAD_GET_TAG,
            static_cast<int32_t>(sizeof(reference_tag)), reference_tag));
        EVP_CIPHER_CTX_free(context);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_chacha20_poly1305_seal(key,
            nonce, associated_data, associated_length, plaintext,
            message_length, actual_ciphertext, actual_tag));
        FT_ASSERT_EQ(message_length, actual_ciphertext.size());
        if (message_length != 0U)
            FT_ASSERT_EQ(0, ft_memcmp(&actual_ciphertext[0],
                reference_ciphertext, message_length));
        FT_ASSERT_EQ(0, ft_memcmp(actual_tag, reference_tag,
            sizeof(actual_tag)));
        case_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual_ciphertext.destroy());
    return (1);
}

FT_TEST(test_crypto_x25519_deterministic_differential_matrix)
{
    uint8_t private_key[32];
    uint8_t peer_public_key[32];
    uint8_t reference_secret[32];
    uint8_t actual_secret[32];
    uint32_t case_index = 0U;

    while (case_index < 64U)
    {
        uint32_t byte_index = 0U;
        uint32_t state = 0x9e3779b9U ^ (case_index * 0x85ebca6bU);

        while (byte_index < 32U)
        {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            private_key[byte_index] = static_cast<uint8_t>(state);
            state ^= 0xa5a5a5a5U + byte_index;
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            peer_public_key[byte_index] = static_cast<uint8_t>(state);
            byte_index += 1U;
        }
        FT_ASSERT(crypto_differential_openssl_x25519(private_key,
            peer_public_key, reference_secret));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_x25519_shared_secret(
            private_key, peer_public_key, actual_secret));
        FT_ASSERT_EQ(0, ft_memcmp(reference_secret, actual_secret,
            sizeof(actual_secret)));
        case_index += 1U;
    }
    return (1);
}

#endif

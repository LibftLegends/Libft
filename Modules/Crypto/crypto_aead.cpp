#include "crypto_aead.hpp"
#include "crypto_chacha20.hpp"
#include "crypto_poly1305.hpp"
#include "crypto_primitives.hpp"
#include "../Errno/errno.hpp"

namespace
{
    static void store_u64_le(uint8_t *data, uint64_t value) noexcept
    {
        uint32_t index;

        index = 0U;
        while (index < 8U)
        {
            data[index] = static_cast<uint8_t>(value >> (index * 8U));
            index += 1U;
        }
        return ;
    }

    static int32_t append_bytes(ft_vector<uint8_t> &buffer,
        const uint8_t *data, ft_size_t size) noexcept
    {
        ft_size_t index;

        if (data == ft_nullptr && size != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        index = 0U;
        while (index < size)
        {
            int32_t result = buffer.push_back(data[index]);

            if (result != FT_ERR_SUCCESS)
                return (result);
            index += 1U;
        }
        return (FT_ERR_SUCCESS);
    }

    static ft_bool constant_time_equal(const uint8_t *left,
        const uint8_t *right, ft_size_t length) noexcept
    {
        uint8_t difference;
        ft_size_t index;

        difference = 0U;
        index = 0U;
        while (index < length)
        {
            difference = static_cast<uint8_t>(difference
                | (left[index] ^ right[index]));
            index += 1U;
        }
        return (static_cast<ft_bool>(difference == 0U));
    }

    static int32_t build_authentication_data(const uint8_t *associated_data,
        ft_size_t associated_data_length, const uint8_t *ciphertext,
        ft_size_t ciphertext_length, ft_vector<uint8_t> &authenticated) noexcept
    {
        uint8_t lengths[16];
        uint8_t zero_padding[16];
        int32_t result;

        if (associated_data == ft_nullptr && associated_data_length != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (ciphertext == ft_nullptr && ciphertext_length != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        ft_memset(lengths, 0, sizeof(lengths));
        ft_memset(zero_padding, 0, sizeof(zero_padding));
        store_u64_le(lengths, associated_data_length);
        store_u64_le(lengths + 8U, ciphertext_length);
        result = authenticated.initialize();
        if (result == FT_ERR_SUCCESS)
            result = append_bytes(authenticated, associated_data,
                associated_data_length);
        if (result == FT_ERR_SUCCESS && associated_data_length % 16U != 0U)
            result = append_bytes(authenticated, zero_padding,
                16U - (associated_data_length % 16U));
        if (result == FT_ERR_SUCCESS)
            result = append_bytes(authenticated, ciphertext,
                ciphertext_length);
        if (result == FT_ERR_SUCCESS && ciphertext_length % 16U != 0U)
            result = append_bytes(authenticated, zero_padding,
                16U - (ciphertext_length % 16U));
        if (result == FT_ERR_SUCCESS)
            result = append_bytes(authenticated, lengths, sizeof(lengths));
        if (result != FT_ERR_SUCCESS)
        {
            (void)authenticated.destroy();
            return (result);
        }
        return (FT_ERR_SUCCESS);
    }
}

int32_t crypto_chacha20_poly1305_seal(const uint8_t key[32],
    const uint8_t nonce[12], const uint8_t *associated_data,
    ft_size_t associated_data_length, const uint8_t *plaintext,
    ft_size_t plaintext_length, ft_vector<uint8_t> &ciphertext,
    uint8_t authentication_tag[16]) noexcept
{
    uint8_t poly1305_key_stream[64];
    ft_vector<uint8_t> authenticated;
    const uint8_t *ciphertext_data;
    int32_t result;

    if (key == ft_nullptr || nonce == ft_nullptr
        || authentication_tag == ft_nullptr
        || (associated_data == ft_nullptr && associated_data_length != 0U)
        || (plaintext == ft_nullptr && plaintext_length != 0U))
    {
        if (ciphertext.is_initialised() == FT_CLASS_STATE_INITIALISED)
            ciphertext.clear();
        if (authentication_tag != ft_nullptr)
            ft_memset(authentication_tag, 0, 16U);
        (void)crypto_secure_wipe(poly1305_key_stream,
            sizeof(poly1305_key_stream));
        return (FT_ERR_INVALID_ARGUMENT);
    }
    ft_memset(poly1305_key_stream, 0, sizeof(poly1305_key_stream));
    result = crypto_chacha20_xor(key, nonce, 1U, plaintext, plaintext_length,
        ciphertext);
    if (result != FT_ERR_SUCCESS)
    {
        if (ciphertext.is_initialised() == FT_CLASS_STATE_INITIALISED)
            ciphertext.clear();
        ft_memset(authentication_tag, 0, 16U);
        return (result);
    }
    crypto_chacha20_block(key, 0U, nonce, poly1305_key_stream);
    ciphertext_data = ft_nullptr;
    if (ciphertext.size() != 0U)
        ciphertext_data = &ciphertext[0];
    result = build_authentication_data(associated_data, associated_data_length,
        ciphertext_data, ciphertext.size(),
        authenticated);
    if (result == FT_ERR_SUCCESS)
        result = crypto_poly1305_auth(authentication_tag, &authenticated[0],
            authenticated.size(), poly1305_key_stream);
    (void)authenticated.destroy();
    (void)crypto_secure_wipe(poly1305_key_stream,
        sizeof(poly1305_key_stream));
    if (result != FT_ERR_SUCCESS)
    {
        ciphertext.clear();
        ft_memset(authentication_tag, 0, 16U);
    }
    return (result);
}

int32_t crypto_chacha20_poly1305_open(const uint8_t key[32],
    const uint8_t nonce[12], const uint8_t *associated_data,
    ft_size_t associated_data_length, const uint8_t *ciphertext,
    ft_size_t ciphertext_length, const uint8_t authentication_tag[16],
    ft_vector<uint8_t> &plaintext) noexcept
{
    uint8_t expected_tag[16];
    uint8_t poly1305_key_stream[64];
    ft_vector<uint8_t> authenticated;
    int32_t result;

    if (plaintext.is_initialised() == FT_CLASS_STATE_INITIALISED)
        plaintext.clear();
    ft_memset(expected_tag, 0, sizeof(expected_tag));
    ft_memset(poly1305_key_stream, 0, sizeof(poly1305_key_stream));
    if (key == ft_nullptr || nonce == ft_nullptr
        || authentication_tag == ft_nullptr
        || (associated_data == ft_nullptr && associated_data_length != 0U)
        || (ciphertext == ft_nullptr && ciphertext_length != 0U))
    {
        (void)crypto_secure_wipe(expected_tag, sizeof(expected_tag));
        (void)crypto_secure_wipe(poly1305_key_stream,
            sizeof(poly1305_key_stream));
        return (FT_ERR_INVALID_ARGUMENT);
    }
    crypto_chacha20_block(key, 0U, nonce, poly1305_key_stream);
    result = build_authentication_data(associated_data, associated_data_length,
        ciphertext, ciphertext_length, authenticated);
    if (result == FT_ERR_SUCCESS)
        result = crypto_poly1305_auth(expected_tag, &authenticated[0],
            authenticated.size(), poly1305_key_stream);
    if (result == FT_ERR_SUCCESS
        && constant_time_equal(expected_tag, authentication_tag, 16U) == FT_FALSE)
        result = FT_ERR_PERMISSION_DENIED;
    if (result != FT_ERR_SUCCESS)
    {
        if (plaintext.is_initialised() == FT_CLASS_STATE_INITIALISED)
            plaintext.clear();
        (void)authenticated.destroy();
        (void)crypto_secure_wipe(expected_tag, sizeof(expected_tag));
        (void)crypto_secure_wipe(poly1305_key_stream,
            sizeof(poly1305_key_stream));
        return (result);
    }
    result = crypto_chacha20_xor(key, nonce, 1U, ciphertext, ciphertext_length,
        plaintext);
    if (result != FT_ERR_SUCCESS
        && plaintext.is_initialised() == FT_CLASS_STATE_INITIALISED)
        plaintext.clear();
    (void)authenticated.destroy();
    (void)crypto_secure_wipe(expected_tag, sizeof(expected_tag));
    (void)crypto_secure_wipe(poly1305_key_stream,
        sizeof(poly1305_key_stream));
    return (result);
}

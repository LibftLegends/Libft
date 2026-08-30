#include "crypto_session.hpp"
#include "crypto_primitives.hpp"
#include "crypto_x25519.hpp"
#include "../Errno/errno.hpp"

namespace
{
    static void wipe(void *data, ft_size_t length) noexcept
    {
        (void)crypto_secure_wipe(data, length);
        return ;
    }

    static int32_t derive_label(const uint8_t pseudorandom_key[32],
        const uint8_t *label, ft_size_t label_length, uint8_t *output,
        ft_size_t output_length) noexcept
    {
        return (crypto_hkdf_sha256_expand(pseudorandom_key, label,
            label_length, output, output_length));
    }
}

int32_t crypto_derive_session_keys(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], const uint8_t *transcript,
    ft_size_t transcript_length, crypto_session_role role,
    uint8_t send_key[32], uint8_t receive_key[32],
    uint8_t send_initialization_vector[12],
    uint8_t receive_initialization_vector[12]) noexcept
{
    static const uint8_t client_to_server_key_label[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x43U, 0x32U, 0x53U, 0x4bU
    };
    static const uint8_t server_to_client_key_label[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x53U, 0x32U, 0x43U, 0x4bU
    };
    static const uint8_t client_to_server_iv_label[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x43U, 0x32U, 0x53U, 0x49U
    };
    static const uint8_t server_to_client_iv_label[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x53U, 0x32U, 0x43U, 0x49U
    };
    uint8_t shared_secret[32];
    uint8_t transcript_hash[32];
    uint8_t pseudorandom_key[32];
    const uint8_t *send_key_label;
    const uint8_t *receive_key_label;
    const uint8_t *send_iv_label;
    const uint8_t *receive_iv_label;
    int32_t result;

    if (private_key == ft_nullptr || peer_public_key == ft_nullptr
        || transcript == ft_nullptr || transcript_length == 0U
        || send_key == ft_nullptr || receive_key == ft_nullptr
        || send_initialization_vector == ft_nullptr
        || receive_initialization_vector == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (role != crypto_session_role::CLIENT
        && role != crypto_session_role::SERVER)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memset(shared_secret, 0, sizeof(shared_secret));
    ft_memset(transcript_hash, 0, sizeof(transcript_hash));
    ft_memset(pseudorandom_key, 0, sizeof(pseudorandom_key));
    result = crypto_x25519_shared_secret(private_key, peer_public_key,
        shared_secret);
    if (result == FT_ERR_SUCCESS)
        result = crypto_sha256_hash(transcript, transcript_length,
            transcript_hash);
    if (result == FT_ERR_SUCCESS)
        result = crypto_hkdf_sha256_extract(transcript_hash,
            sizeof(transcript_hash), shared_secret, sizeof(shared_secret),
            pseudorandom_key);
    send_key_label = client_to_server_key_label;
    receive_key_label = server_to_client_key_label;
    send_iv_label = client_to_server_iv_label;
    receive_iv_label = server_to_client_iv_label;
    if (role == crypto_session_role::SERVER)
    {
        send_key_label = server_to_client_key_label;
        receive_key_label = client_to_server_key_label;
        send_iv_label = server_to_client_iv_label;
        receive_iv_label = client_to_server_iv_label;
    }
    if (result == FT_ERR_SUCCESS)
        result = derive_label(pseudorandom_key, send_key_label, 8U,
            send_key, 32U);
    if (result == FT_ERR_SUCCESS)
        result = derive_label(pseudorandom_key, receive_key_label, 8U,
            receive_key, 32U);
    if (result == FT_ERR_SUCCESS)
        result = derive_label(pseudorandom_key, send_iv_label, 8U,
            send_initialization_vector, 12U);
    if (result == FT_ERR_SUCCESS)
        result = derive_label(pseudorandom_key, receive_iv_label, 8U,
            receive_initialization_vector, 12U);
    wipe(shared_secret, sizeof(shared_secret));
    wipe(transcript_hash, sizeof(transcript_hash));
    wipe(pseudorandom_key, sizeof(pseudorandom_key));
    if (result != FT_ERR_SUCCESS)
    {
        wipe(send_key, 32U);
        wipe(receive_key, 32U);
        wipe(send_initialization_vector, 12U);
        wipe(receive_initialization_vector, 12U);
    }
    return (result);
}

int32_t crypto_derive_key_update(const uint8_t current_key[32],
    uint64_t next_epoch, uint8_t updated_key[32],
    uint8_t updated_initialization_vector[12]) noexcept
{
    static const uint8_t label[16] = {
        'L', 'i', 'b', 'f', 't', ' ', 'k', 'e',
        'y', ' ', 'u', 'p', 'd', 'a', 't', 'e'
    };
    uint8_t pseudorandom_key[32];
    uint8_t info[24];
    uint8_t expanded[44];
    uint32_t index;
    int32_t result;

    if (current_key == ft_nullptr || updated_key == ft_nullptr
        || updated_initialization_vector == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memset(pseudorandom_key, 0, sizeof(pseudorandom_key));
    ft_memset(info, 0, sizeof(info));
    ft_memset(expanded, 0, sizeof(expanded));
    ft_memcpy(info, label, sizeof(label));
    index = 0U;
    while (index < 8U)
    {
        info[16U + index] = static_cast<uint8_t>(
            (next_epoch >> (56U - index * 8U)) & 0xffU);
        index += 1U;
    }
    result = crypto_hkdf_sha256_extract(ft_nullptr, 0U, current_key, 32U,
        pseudorandom_key);
    if (result == FT_ERR_SUCCESS)
        result = crypto_hkdf_sha256_expand(pseudorandom_key, info,
            sizeof(info), expanded, sizeof(expanded));
    if (result == FT_ERR_SUCCESS)
    {
        ft_memcpy(updated_key, expanded, 32U);
        ft_memcpy(updated_initialization_vector, expanded + 32U, 12U);
    }
    else
    {
        ft_memset(updated_key, 0, 32U);
        ft_memset(updated_initialization_vector, 0, 12U);
    }
    wipe(pseudorandom_key, sizeof(pseudorandom_key));
    wipe(info, sizeof(info));
    wipe(expanded, sizeof(expanded));
    return (result);
}

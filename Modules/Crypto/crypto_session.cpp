#include "crypto_session.hpp"
#include "crypto_primitives_internal.hpp"
#include "crypto_x25519.hpp"
#include "../Errno/errno.hpp"

namespace
{
    static ft_bool memory_ranges_overlap(const void *left,
        ft_size_t left_length, const void *right,
        ft_size_t right_length) noexcept
    {
        uintptr_t left_address;
        uintptr_t right_address;

        if (left == ft_nullptr || right == ft_nullptr
            || left_length == 0U || right_length == 0U)
            return (FT_FALSE);
        left_address = reinterpret_cast<uintptr_t>(left);
        right_address = reinterpret_cast<uintptr_t>(right);
        if (left_address <= right_address)
            return (right_address - left_address < left_length);
        return (left_address - right_address < right_length);
    }

    static void wipe(void *data, ft_size_t length) noexcept
    {
        (void)crypto_secure_wipe(data, length);
        return ;
    }

    static void clear_session_outputs(uint8_t send_key[32],
        uint8_t receive_key[32], uint8_t send_initialization_vector[12],
        uint8_t receive_initialization_vector[12]) noexcept
    {
        if (send_key != ft_nullptr)
            wipe(send_key, 32U);
        if (receive_key != ft_nullptr)
            wipe(receive_key, 32U);
        if (send_initialization_vector != ft_nullptr)
            wipe(send_initialization_vector, 12U);
        if (receive_initialization_vector != ft_nullptr)
            wipe(receive_initialization_vector, 12U);
        return ;
    }
}

int32_t crypto_derive_session_keys(const uint8_t private_key[32],
    const uint8_t peer_public_key[32], const uint8_t *transcript,
    ft_size_t transcript_length, crypto_session_role role,
    uint8_t send_key[32], uint8_t receive_key[32],
    uint8_t send_initialization_vector[12],
    uint8_t receive_initialization_vector[12]) noexcept
{
    constexpr uint8_t CLIENT_TO_SERVER_KEY_LABEL[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x43U, 0x32U, 0x53U, 0x4bU
    };
    constexpr uint8_t SERVER_TO_CLIENT_KEY_LABEL[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x53U, 0x32U, 0x43U, 0x4bU
    };
    constexpr uint8_t CLIENT_TO_SERVER_IV_LABEL[8] = {
        0x4cU, 0x46U, 0x54U, 0x01U, 0x43U, 0x32U, 0x53U, 0x49U
    };
    constexpr uint8_t SERVER_TO_CLIENT_IV_LABEL[8] = {
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

    if (memory_ranges_overlap(send_key, 32U, receive_key, 32U) != FT_FALSE
        || memory_ranges_overlap(send_key, 32U,
            send_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(send_key, 32U,
            receive_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(receive_key, 32U,
            send_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(receive_key, 32U,
            receive_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(send_initialization_vector, 12U,
            receive_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(private_key, 32U, send_key, 32U) != FT_FALSE
        || memory_ranges_overlap(private_key, 32U, receive_key, 32U) != FT_FALSE
        || memory_ranges_overlap(private_key, 32U,
            send_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(private_key, 32U,
            receive_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(peer_public_key, 32U, send_key, 32U) != FT_FALSE
        || memory_ranges_overlap(peer_public_key, 32U, receive_key, 32U) != FT_FALSE
        || memory_ranges_overlap(peer_public_key, 32U,
            send_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(peer_public_key, 32U,
            receive_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(transcript, transcript_length,
            send_key, 32U) != FT_FALSE
        || memory_ranges_overlap(transcript, transcript_length,
            receive_key, 32U) != FT_FALSE
        || memory_ranges_overlap(transcript, transcript_length,
            send_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(transcript, transcript_length,
            receive_initialization_vector, 12U) != FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (private_key == ft_nullptr || peer_public_key == ft_nullptr
        || transcript == ft_nullptr || transcript_length == 0U
        || send_key == ft_nullptr || receive_key == ft_nullptr
        || send_initialization_vector == ft_nullptr
        || receive_initialization_vector == ft_nullptr)
    {
        clear_session_outputs(send_key, receive_key,
            send_initialization_vector, receive_initialization_vector);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (role != crypto_session_role::CLIENT
        && role != crypto_session_role::SERVER)
    {
        clear_session_outputs(send_key, receive_key,
            send_initialization_vector, receive_initialization_vector);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    result = crypto_x25519_shared_secret(private_key, peer_public_key,
        shared_secret);
    if (result == FT_ERR_SUCCESS)
        result = crypto_sha256_hash(transcript, transcript_length,
            transcript_hash);
    if (result == FT_ERR_SUCCESS)
        result = crypto_hkdf_sha256_extract(transcript_hash,
            sizeof(transcript_hash), shared_secret, sizeof(shared_secret),
            pseudorandom_key);
    send_key_label = CLIENT_TO_SERVER_KEY_LABEL;
    receive_key_label = SERVER_TO_CLIENT_KEY_LABEL;
    send_iv_label = CLIENT_TO_SERVER_IV_LABEL;
    receive_iv_label = SERVER_TO_CLIENT_IV_LABEL;
    if (role == crypto_session_role::SERVER)
    {
        send_key_label = SERVER_TO_CLIENT_KEY_LABEL;
        receive_key_label = CLIENT_TO_SERVER_KEY_LABEL;
        send_iv_label = SERVER_TO_CLIENT_IV_LABEL;
        receive_iv_label = CLIENT_TO_SERVER_IV_LABEL;
    }
    if (result == FT_ERR_SUCCESS)
    {
        const crypto_hkdf_sha256_label labels[] = {
            {send_key_label, sizeof(CLIENT_TO_SERVER_KEY_LABEL), send_key, 32U},
            {receive_key_label, sizeof(SERVER_TO_CLIENT_KEY_LABEL), receive_key, 32U},
            {send_iv_label, sizeof(CLIENT_TO_SERVER_IV_LABEL), send_initialization_vector, 12U},
            {receive_iv_label, sizeof(SERVER_TO_CLIENT_IV_LABEL), receive_initialization_vector, 12U}
        };
        result = crypto_hkdf_sha256_expand_labels(pseudorandom_key, labels,
            sizeof(labels) / sizeof(labels[0]));
    }
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
    constexpr uint8_t LABEL[16] = {
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
    {
        if (updated_key != ft_nullptr)
            wipe(updated_key, 32U);
        if (updated_initialization_vector != ft_nullptr)
            wipe(updated_initialization_vector, 12U);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (memory_ranges_overlap(current_key, 32U, updated_key, 32U) != FT_FALSE
        || memory_ranges_overlap(current_key, 32U,
            updated_initialization_vector, 12U) != FT_FALSE
        || memory_ranges_overlap(updated_key, 32U,
            updated_initialization_vector, 12U) != FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_memcpy_fixed<sizeof(LABEL)>(info, LABEL);
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
        ft_memcpy_fixed<32U>(updated_key, expanded);
        ft_memcpy_fixed<12U>(updated_initialization_vector, expanded + 32U);
    }
    else
    {
        wipe(updated_key, 32U);
        wipe(updated_initialization_vector, 12U);
    }
    wipe(pseudorandom_key, sizeof(pseudorandom_key));
    wipe(info, sizeof(info));
    wipe(expanded, sizeof(expanded));
    return (result);
}
